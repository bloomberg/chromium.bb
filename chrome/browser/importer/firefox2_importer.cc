// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox2_importer.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/mork_reader.h"
#include "chrome/browser/importer/nss_decryptor.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_parser.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/data_url.h"
#include "webkit/glue/password_form.h"

Firefox2Importer::Firefox2Importer() : parsing_bookmarks_html_file_(false) {
}

Firefox2Importer::~Firefox2Importer() {
}

void Firefox2Importer::StartImport(const importer::ProfileInfo& profile_info,
                                   uint16 items,
                                   ImporterBridge* bridge) {
  bridge_ = bridge;
  source_path_ = profile_info.source_path;
  app_path_ = profile_info.app_path;

  parsing_bookmarks_html_file_ =
      (profile_info.browser_type == importer::BOOKMARKS_HTML);

  // The order here is important!
  bridge_->NotifyStarted();
  if ((items & importer::HOME_PAGE) && !cancelled())
    ImportHomepage();  // Doesn't have a UI item.

  // Note history should be imported before bookmarks because bookmark import
  // will also import favicons and we store favicon for a URL only if the URL
  // exist in history or bookmarks.
  if ((items & importer::HISTORY) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::HISTORY);
    ImportHistory();
    bridge_->NotifyItemEnded(importer::HISTORY);
  }

  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportBookmarks();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
  if ((items & importer::SEARCH_ENGINES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::SEARCH_ENGINES);
    ImportSearchEngines();
    bridge_->NotifyItemEnded(importer::SEARCH_ENGINES);
  }
  if ((items & importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::PASSWORDS);
    ImportPasswords();
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }
  bridge_->NotifyEnded();
}

// static
void Firefox2Importer::LoadDefaultBookmarks(const FilePath& app_path,
                                            std::set<GURL> *urls) {
  FilePath file = app_path.AppendASCII("defaults")
                      .AppendASCII("profile")
                      .AppendASCII("bookmarks.html");

  urls->clear();

  // Read the whole file.
  std::string content;
  file_util::ReadFileToString(file, &content);
  std::vector<std::string> lines;
  base::SplitString(content, '\n', &lines);

  std::string charset;
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string line;
    TrimString(lines[i], " ", &line);

    // Get the encoding of the bookmark file.
    if (ParseCharsetFromLine(line, &charset))
      continue;

    // Get the bookmark.
    std::wstring title;
    GURL url, favicon;
    std::wstring shortcut;
    base::Time add_date;
    std::wstring post_data;
    if (ParseBookmarkFromLine(line, charset, &title, &url,
                              &favicon, &shortcut, &add_date,
                              &post_data))
      urls->insert(url);
  }
}

// static
TemplateURL* Firefox2Importer::CreateTemplateURL(const std::wstring& title,
                                                 const std::wstring& keyword,
                                                 const GURL& url) {
  // Skip if the keyword or url is invalid.
  if (keyword.empty() && url.is_valid())
    return NULL;

  TemplateURL* t_url = new TemplateURL();
  // We set short name by using the title if it exists.
  // Otherwise, we use the shortcut.
  t_url->set_short_name(WideToUTF16Hack(!title.empty() ? title : keyword));
  t_url->set_keyword(WideToUTF16Hack(keyword));
  t_url->SetURL(TemplateURLRef::DisplayURLToURLRef(UTF8ToUTF16(url.spec())),
                0, 0);
  return t_url;
}

// static
void Firefox2Importer::ImportBookmarksFile(
    const FilePath& file_path,
    const std::set<GURL>& default_urls,
    bool import_to_bookmark_bar,
    const std::wstring& first_folder_name,
    Importer* importer,
    std::vector<ProfileWriter::BookmarkEntry>* bookmarks,
    std::vector<TemplateURL*>* template_urls,
    std::vector<history::ImportedFaviconUsage>* favicons) {
  std::string content;
  file_util::ReadFileToString(file_path, &content);
  std::vector<std::string> lines;
  base::SplitString(content, '\n', &lines);

  std::vector<ProfileWriter::BookmarkEntry> toolbar_bookmarks;
  std::wstring last_folder = first_folder_name;
  bool last_folder_on_toolbar = false;
  bool last_folder_is_empty = true;
  base::Time last_folder_add_date;
  std::vector<std::wstring> path;
  size_t toolbar_folder = 0;
  std::string charset;
  for (size_t i = 0; i < lines.size() && (!importer || !importer->cancelled());
       ++i) {
    std::string line;
    TrimString(lines[i], " ", &line);

    // Get the encoding of the bookmark file.
    if (ParseCharsetFromLine(line, &charset))
      continue;

    // Get the folder name.
    if (ParseFolderNameFromLine(line, charset, &last_folder,
                                &last_folder_on_toolbar,
                                &last_folder_add_date))
      continue;

    // Get the bookmark entry.
    std::wstring title, shortcut;
    GURL url, favicon;
    base::Time add_date;
    std::wstring post_data;
    bool is_bookmark;
    // TODO(jcampan): http://b/issue?id=1196285 we do not support POST based
    //                keywords yet.
    is_bookmark = ParseBookmarkFromLine(line, charset, &title,
                                        &url, &favicon, &shortcut, &add_date,
                                        &post_data) ||
        ParseMinimumBookmarkFromLine(line, charset, &title, &url);

    if (is_bookmark)
      last_folder_is_empty = false;

    if (is_bookmark &&
        post_data.empty() &&
        CanImportURL(GURL(url)) &&
        default_urls.find(url) == default_urls.end()) {
      if (toolbar_folder > path.size() && !path.empty()) {
        NOTREACHED();  // error in parsing.
        break;
      }

      ProfileWriter::BookmarkEntry entry;
      entry.creation_time = add_date;
      entry.url = url;
      entry.title = title;

      if (import_to_bookmark_bar && toolbar_folder) {
        // Flatten the items in toolbar.
        entry.in_toolbar = true;
        entry.path.assign(path.begin() + toolbar_folder, path.end());
        toolbar_bookmarks.push_back(entry);
      } else {
        // Insert the item into the "Imported from Firefox" folder.
        entry.path.assign(path.begin(), path.end());
        if (import_to_bookmark_bar)
          entry.path.erase(entry.path.begin());
        bookmarks->push_back(entry);
      }

      // Save the favicon. DataURLToFaviconUsage will handle the case where
      // there is no favicon.
      if (favicons)
        DataURLToFaviconUsage(url, favicon, favicons);

      if (template_urls) {
        // If there is a SHORTCUT attribute for this bookmark, we
        // add it as our keywords.
        TemplateURL* t_url = CreateTemplateURL(title, shortcut, url);
        if (t_url)
          template_urls->push_back(t_url);
      }

      continue;
    }

    // Bookmarks in sub-folder are encapsulated with <DL> tag.
    if (StartsWithASCII(line, "<DL>", false)) {
      path.push_back(last_folder);
      last_folder.clear();
      if (last_folder_on_toolbar && !toolbar_folder)
        toolbar_folder = path.size();

      // Mark next folder empty as initial state.
      last_folder_is_empty = true;
    } else if (StartsWithASCII(line, "</DL>", false)) {
      if (path.empty())
        break;  // Mismatch <DL>.

      std::wstring folder_title = path.back();
      path.pop_back();

      if (last_folder_is_empty) {
        // Empty folder should be added explicitly.
        ProfileWriter::BookmarkEntry entry;
        entry.is_folder = true;
        entry.creation_time = last_folder_add_date;
        entry.title = folder_title;
        if (import_to_bookmark_bar && toolbar_folder) {
          // Flatten the folder in toolbar.
          entry.in_toolbar = true;
          entry.path.assign(path.begin() + toolbar_folder, path.end());
          toolbar_bookmarks.push_back(entry);
        } else {
          // Insert the folder into the "Imported from Firefox" folder.
          entry.path.assign(path.begin(), path.end());
          if (import_to_bookmark_bar)
            entry.path.erase(entry.path.begin());
          bookmarks->push_back(entry);
        }

        // Parent folder include current one, so it's not empty.
        last_folder_is_empty = false;
      }

      if (toolbar_folder > path.size())
        toolbar_folder = 0;
    }
  }

  bookmarks->insert(bookmarks->begin(), toolbar_bookmarks.begin(),
                    toolbar_bookmarks.end());
}

void Firefox2Importer::ImportBookmarks() {
  // Load the default bookmarks.
  std::set<GURL> default_urls;
  if (!parsing_bookmarks_html_file_)
    LoadDefaultBookmarks(app_path_, &default_urls);

  // Parse the bookmarks.html file.
  std::vector<ProfileWriter::BookmarkEntry> bookmarks, toolbar_bookmarks;
  std::vector<TemplateURL*> template_urls;
  std::vector<history::ImportedFaviconUsage> favicons;
  FilePath file = source_path_;
  if (!parsing_bookmarks_html_file_)
    file = file.AppendASCII("bookmarks.html");
  std::wstring first_folder_name;
  first_folder_name = bridge_->GetLocalizedString(
      parsing_bookmarks_html_file_ ? IDS_BOOKMARK_GROUP :
                                     IDS_BOOKMARK_GROUP_FROM_FIREFOX);

  ImportBookmarksFile(file, default_urls, import_to_bookmark_bar(),
                      first_folder_name, this, &bookmarks, &template_urls,
                      &favicons);

  // Write data into profile.
  if (!bookmarks.empty() && !cancelled()) {
    int options = 0;
    if (import_to_bookmark_bar())
      options |= ProfileWriter::IMPORT_TO_BOOKMARK_BAR;
    if (bookmark_bar_disabled())
      options |= ProfileWriter::BOOKMARK_BAR_DISABLED;
    bridge_->AddBookmarkEntries(bookmarks, first_folder_name, options);
  }
  if (!parsing_bookmarks_html_file_ && !template_urls.empty() &&
      !cancelled()) {
    bridge_->SetKeywords(template_urls, -1, false);
  } else {
    STLDeleteContainerPointers(template_urls.begin(), template_urls.end());
  }
  if (!favicons.empty()) {
    bridge_->SetFavicons(favicons);
  }
}

void Firefox2Importer::ImportPasswords() {
  // Initializes NSS3.
  NSSDecryptor decryptor;
  if (!decryptor.Init(source_path_, source_path_) &&
      !decryptor.Init(app_path_, source_path_)) {
    return;
  }

  // Firefox 2 uses signons2.txt to store the pssswords. If it doesn't
  // exist, we try to find its older version.
  FilePath file = source_path_.AppendASCII("signons2.txt");
  if (!file_util::PathExists(file)) {
    file = source_path_.AppendASCII("signons.txt");
  }

  std::string content;
  file_util::ReadFileToString(file, &content);
  std::vector<webkit_glue::PasswordForm> forms;
  decryptor.ParseSignons(content, &forms);

  if (!cancelled()) {
    for (size_t i = 0; i < forms.size(); ++i) {
      bridge_->SetPasswordForm(forms[i]);
    }
  }
}

void Firefox2Importer::ImportHistory() {
  FilePath file = source_path_.AppendASCII("history.dat");
  ImportHistoryFromFirefox2(file, bridge_);
}

void Firefox2Importer::ImportSearchEngines() {
  std::vector<FilePath> files;
  GetSearchEnginesXMLFiles(&files);

  std::vector<TemplateURL*> search_engines;
  ParseSearchEnginesFromXMLFiles(files, &search_engines);

  int default_index =
      GetFirefoxDefaultSearchEngineIndex(search_engines, source_path_);
  bridge_->SetKeywords(search_engines, default_index, true);
}

void Firefox2Importer::ImportHomepage() {
  GURL home_page = GetHomepage(source_path_);
  if (home_page.is_valid() && !IsDefaultHomepage(home_page, app_path_)) {
    bridge_->AddHomePage(home_page);
  }
}

void Firefox2Importer::GetSearchEnginesXMLFiles(
    std::vector<FilePath>* files) {
  // Search engines are contained in XML files in a searchplugins directory that
  // can be found in 2 locations:
  // - Firefox install dir (default search engines)
  // - the profile dir (user added search engines)
  FilePath dir = app_path_.AppendASCII("searchplugins");
  FindXMLFilesInDir(dir, files);

  FilePath profile_dir = source_path_.AppendASCII("searchplugins");
  FindXMLFilesInDir(profile_dir, files);
}

// static
bool Firefox2Importer::ParseCharsetFromLine(const std::string& line,
                                            std::string* charset) {
  const char kCharset[] = "charset=";
  if (StartsWithASCII(line, "<META", false) &&
      (line.find("CONTENT=\"") != std::string::npos ||
          line.find("content=\"") != std::string::npos)) {
    size_t begin = line.find(kCharset);
    if (begin == std::string::npos)
      return false;
    begin += std::string(kCharset).size();
    size_t end = line.find_first_of('\"', begin);
    *charset = line.substr(begin, end - begin);
    return true;
  }
  return false;
}

// static
bool Firefox2Importer::ParseFolderNameFromLine(const std::string& line,
                                               const std::string& charset,
                                               std::wstring* folder_name,
                                               bool* is_toolbar_folder,
                                               base::Time* add_date) {
  const char kFolderOpen[] = "<DT><H3";
  const char kFolderClose[] = "</H3>";
  const char kToolbarFolderAttribute[] = "PERSONAL_TOOLBAR_FOLDER";
  const char kAddDateAttribute[] = "ADD_DATE";

  if (!StartsWithASCII(line, kFolderOpen, true))
    return false;

  size_t end = line.find(kFolderClose);
  size_t tag_end = line.rfind('>', end) + 1;
  // If no end tag or start tag is broken, we skip to find the folder name.
  if (end == std::string::npos || tag_end < arraysize(kFolderOpen))
    return false;

  base::CodepageToWide(line.substr(tag_end, end - tag_end), charset.c_str(),
                       base::OnStringConversionError::SKIP, folder_name);
  HTMLUnescape(folder_name);

  std::string attribute_list = line.substr(arraysize(kFolderOpen),
      tag_end - arraysize(kFolderOpen) - 1);
  std::string value;

  // Add date
  if (GetAttribute(attribute_list, kAddDateAttribute, &value)) {
    int64 time;
    base::StringToInt64(value, &time);
    // Upper bound it at 32 bits.
    if (0 < time && time < (1LL << 32))
      *add_date = base::Time::FromTimeT(time);
  }

  if (GetAttribute(attribute_list, kToolbarFolderAttribute, &value) &&
      LowerCaseEqualsASCII(value, "true"))
    *is_toolbar_folder = true;
  else
    *is_toolbar_folder = false;

  return true;
}

// static
bool Firefox2Importer::ParseBookmarkFromLine(const std::string& line,
                                             const std::string& charset,
                                             std::wstring* title,
                                             GURL* url,
                                             GURL* favicon,
                                             std::wstring* shortcut,
                                             base::Time* add_date,
                                             std::wstring* post_data) {
  const char kItemOpen[] = "<DT><A";
  const char kItemClose[] = "</A>";
  const char kFeedURLAttribute[] = "FEEDURL";
  const char kHrefAttribute[] = "HREF";
  const char kIconAttribute[] = "ICON";
  const char kShortcutURLAttribute[] = "SHORTCUTURL";
  const char kAddDateAttribute[] = "ADD_DATE";
  const char kPostDataAttribute[] = "POST_DATA";

  title->clear();
  *url = GURL();
  *favicon = GURL();
  shortcut->clear();
  post_data->clear();
  *add_date = base::Time();

  if (!StartsWithASCII(line, kItemOpen, true))
    return false;

  size_t end = line.find(kItemClose);
  size_t tag_end = line.rfind('>', end) + 1;
  if (end == std::string::npos || tag_end < arraysize(kItemOpen))
    return false;  // No end tag or start tag is broken.

  std::string attribute_list = line.substr(arraysize(kItemOpen),
      tag_end - arraysize(kItemOpen) - 1);

  // We don't import Live Bookmark folders, which is Firefox's RSS reading
  // feature, since the user never necessarily bookmarked them and we don't
  // have this feature to update their contents.
  std::string value;
  if (GetAttribute(attribute_list, kFeedURLAttribute, &value))
    return false;

  // Title
  base::CodepageToWide(line.substr(tag_end, end - tag_end), charset.c_str(),
                       base::OnStringConversionError::SKIP, title);
  HTMLUnescape(title);

  // URL
  if (GetAttribute(attribute_list, kHrefAttribute, &value)) {
    std::wstring w_url;
    base::CodepageToWide(value, charset.c_str(),
                         base::OnStringConversionError::SKIP, &w_url);
    HTMLUnescape(&w_url);

    string16 url16 = WideToUTF16Hack(w_url);

    *url = GURL(url16);
  }

  // Favicon
  if (GetAttribute(attribute_list, kIconAttribute, &value))
    *favicon = GURL(value);

  // Keyword
  if (GetAttribute(attribute_list, kShortcutURLAttribute, &value)) {
    base::CodepageToWide(value, charset.c_str(),
                         base::OnStringConversionError::SKIP, shortcut);
    HTMLUnescape(shortcut);
  }

  // Add date
  if (GetAttribute(attribute_list, kAddDateAttribute, &value)) {
    int64 time;
    base::StringToInt64(value, &time);
    // Upper bound it at 32 bits.
    if (0 < time && time < (1LL << 32))
      *add_date = base::Time::FromTimeT(time);
  }

  // Post data.
  if (GetAttribute(attribute_list, kPostDataAttribute, &value)) {
    base::CodepageToWide(value, charset.c_str(),
                         base::OnStringConversionError::SKIP, post_data);
    HTMLUnescape(post_data);
  }

  return true;
}

// static
bool Firefox2Importer::ParseMinimumBookmarkFromLine(const std::string& line,
                                                    const std::string& charset,
                                                    std::wstring* title,
                                                    GURL* url) {
  const char kItemOpen[] = "<DT><A";
  const char kItemClose[] = "</";
  const char kHrefAttributeUpper[] = "HREF";
  const char kHrefAttributeLower[] = "href";

  title->clear();
  *url = GURL();

  // Case-insensitive check of open tag.
  if (!StartsWithASCII(line, kItemOpen, false))
    return false;

  // Find any close tag.
  size_t end = line.find(kItemClose);
  size_t tag_end = line.rfind('>', end) + 1;
  if (end == std::string::npos || tag_end < arraysize(kItemOpen))
    return false;  // No end tag or start tag is broken.

  std::string attribute_list = line.substr(arraysize(kItemOpen),
      tag_end - arraysize(kItemOpen) - 1);

  // Title
  base::CodepageToWide(line.substr(tag_end, end - tag_end), charset.c_str(),
                       base::OnStringConversionError::SKIP, title);
  HTMLUnescape(title);

  // URL
  std::string value;
  if (GetAttribute(attribute_list, kHrefAttributeUpper, &value) ||
      GetAttribute(attribute_list, kHrefAttributeLower, &value)) {
    if (charset.length() != 0) {
      std::wstring w_url;
      base::CodepageToWide(value, charset.c_str(),
                           base::OnStringConversionError::SKIP, &w_url);
      HTMLUnescape(&w_url);

      string16 url16 = WideToUTF16Hack(w_url);

      *url = GURL(url16);
    } else {
      *url = GURL(value);
    }
  }

  return true;
}

// static
bool Firefox2Importer::GetAttribute(const std::string& attribute_list,
                                    const std::string& attribute,
                                    std::string* value) {
  const char kQuote[] = "\"";

  size_t begin = attribute_list.find(attribute + "=" + kQuote);
  if (begin == std::string::npos)
    return false;  // Can't find the attribute.

  begin = attribute_list.find(kQuote, begin) + 1;

  size_t end = begin + 1;
  while (end < attribute_list.size()) {
    if (attribute_list[end] == '"' &&
        attribute_list[end - 1] != '\\') {
      break;
    }
    end++;
  }

  if (end == attribute_list.size())
    return false;  // The value is not quoted.

  *value = attribute_list.substr(begin, end - begin);
  return true;
}

// static
void Firefox2Importer::HTMLUnescape(std::wstring *text) {
  string16 text16 = WideToUTF16Hack(*text);
  ReplaceSubstringsAfterOffset(
      &text16, 0, ASCIIToUTF16("&lt;"), ASCIIToUTF16("<"));
  ReplaceSubstringsAfterOffset(
      &text16, 0, ASCIIToUTF16("&gt;"), ASCIIToUTF16(">"));
  ReplaceSubstringsAfterOffset(
      &text16, 0, ASCIIToUTF16("&amp;"), ASCIIToUTF16("&"));
  ReplaceSubstringsAfterOffset(
      &text16, 0, ASCIIToUTF16("&quot;"), ASCIIToUTF16("\""));
  ReplaceSubstringsAfterOffset(
      &text16, 0, ASCIIToUTF16("&#39;"), ASCIIToUTF16("\'"));
  text->assign(UTF16ToWideHack(text16));
}

// static
void Firefox2Importer::FindXMLFilesInDir(
    const FilePath& dir,
    std::vector<FilePath>* xml_files) {
  file_util::FileEnumerator file_enum(dir, false,
                                      file_util::FileEnumerator::FILES,
                                      FILE_PATH_LITERAL("*.xml"));
  FilePath file(file_enum.Next());
  while (!file.empty()) {
    xml_files->push_back(file);
    file = file_enum.Next();
  }
}

// static
void Firefox2Importer::DataURLToFaviconUsage(
    const GURL& link_url,
    const GURL& favicon_data,
    std::vector<history::ImportedFaviconUsage>* favicons) {
  if (!link_url.is_valid() || !favicon_data.is_valid() ||
      !favicon_data.SchemeIs(chrome::kDataScheme))
    return;

  // Parse the data URL.
  std::string mime_type, char_set, data;
  if (!net::DataURL::Parse(favicon_data, &mime_type, &char_set, &data) ||
      data.empty())
    return;

  history::ImportedFaviconUsage usage;
  if (!ReencodeFavicon(reinterpret_cast<const unsigned char*>(&data[0]),
                       data.size(), &usage.png_data))
    return;  // Unable to decode.

  // We need to make up a URL for the favicon. We use a version of the page's
  // URL so that we can be sure it will not collide.
  usage.favicon_url = GURL(std::string("made-up-favicon:") + link_url.spec());

  // We only have one URL per favicon for Firefox 2 bookmarks.
  usage.urls.insert(link_url);

  favicons->push_back(usage);
}
