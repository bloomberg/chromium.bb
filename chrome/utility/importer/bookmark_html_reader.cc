// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/bookmark_html_reader.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/utility/importer/favicon_reencode.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

// Fetches the given |attribute| value from the |attribute_list|. Returns true
// if successful, and |value| will contain the value.
bool GetAttribute(const std::string& attribute_list,
                  const std::string& attribute,
                  std::string* value) {
  const char kQuote[] = "\"";

  size_t begin = attribute_list.find(attribute + "=" + kQuote);
  if (begin == std::string::npos)
    return false;  // Can't find the attribute.

  begin += attribute.size() + 2;
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

// Given the URL of a page and a favicon data URL, adds an appropriate record
// to the given favicon usage vector.
void DataURLToFaviconUsage(
    const GURL& link_url,
    const GURL& favicon_data,
    std::vector<ImportedFaviconUsage>* favicons) {
  if (!link_url.is_valid() || !favicon_data.is_valid() ||
      !favicon_data.SchemeIs(url::kDataScheme))
    return;

  // Parse the data URL.
  std::string mime_type, char_set, data;
  if (!net::DataURL::Parse(favicon_data, &mime_type, &char_set, &data) ||
      data.empty())
    return;

  ImportedFaviconUsage usage;
  if (!importer::ReencodeFavicon(
          reinterpret_cast<const unsigned char*>(&data[0]),
          data.size(), &usage.png_data))
    return;  // Unable to decode.

  // We need to make up a URL for the favicon. We use a version of the page's
  // URL so that we can be sure it will not collide.
  usage.favicon_url = GURL(std::string("made-up-favicon:") + link_url.spec());

  // We only have one URL per favicon for Firefox 2 bookmarks.
  usage.urls.insert(link_url);

  favicons->push_back(usage);
}

}  // namespace

namespace bookmark_html_reader {

void ImportBookmarksFile(
      const base::Callback<bool(void)>& cancellation_callback,
      const base::Callback<bool(const GURL&)>& valid_url_callback,
      const base::FilePath& file_path,
      std::vector<ImportedBookmarkEntry>* bookmarks,
      std::vector<ImportedFaviconUsage>* favicons) {
  std::string content;
  base::ReadFileToString(file_path, &content);
  std::vector<std::string> lines;
  base::SplitString(content, '\n', &lines);

  base::string16 last_folder;
  bool last_folder_on_toolbar = false;
  bool last_folder_is_empty = true;
  bool has_subfolder = false;
  base::Time last_folder_add_date;
  std::vector<base::string16> path;
  size_t toolbar_folder_index = 0;
  std::string charset;
  for (size_t i = 0;
       i < lines.size() &&
           (cancellation_callback.is_null() || !cancellation_callback.Run());
       ++i) {
    std::string line;
    base::TrimString(lines[i], " ", &line);

    // Remove "<HR>" if |line| starts with it. "<HR>" is the bookmark entries
    // separator in Firefox that Chrome does not support. Note that there can be
    // multiple "<HR>" tags at the beginning of a single line.
    // See http://crbug.com/257474.
    static const char kHrTag[] = "<HR>";
    while (StartsWithASCII(line, kHrTag, false)) {
      line.erase(0, arraysize(kHrTag) - 1);
      base::TrimString(line, " ", &line);
    }

    // Get the encoding of the bookmark file.
    if (internal::ParseCharsetFromLine(line, &charset))
      continue;

    // Get the folder name.
    if (internal::ParseFolderNameFromLine(line,
                                          charset,
                                          &last_folder,
                                          &last_folder_on_toolbar,
                                          &last_folder_add_date)) {
      continue;
    }

    // Get the bookmark entry.
    base::string16 title;
    base::string16 shortcut;
    GURL url, favicon;
    base::Time add_date;
    base::string16 post_data;
    bool is_bookmark;
    // TODO(jcampan): http://b/issue?id=1196285 we do not support POST based
    //                keywords yet.
    is_bookmark =
        internal::ParseBookmarkFromLine(line, charset, &title,
                                        &url, &favicon, &shortcut,
                                        &add_date, &post_data) ||
        internal::ParseMinimumBookmarkFromLine(line, charset, &title, &url);

    if (is_bookmark)
      last_folder_is_empty = false;

    if (is_bookmark &&
        post_data.empty() &&
        (valid_url_callback.is_null() || valid_url_callback.Run(url))) {
      if (toolbar_folder_index > path.size() && !path.empty()) {
        NOTREACHED();  // error in parsing.
        break;
      }

      ImportedBookmarkEntry entry;
      entry.creation_time = add_date;
      entry.url = url;
      entry.title = title;

      if (toolbar_folder_index) {
        // The toolbar folder should be at the top level.
        entry.in_toolbar = true;
        entry.path.assign(path.begin() + toolbar_folder_index - 1, path.end());
      } else {
        // Add this bookmark to the list of |bookmarks|.
        if (!has_subfolder && !last_folder.empty()) {
          path.push_back(last_folder);
          last_folder.clear();
        }
        entry.path.assign(path.begin(), path.end());
      }
      bookmarks->push_back(entry);

      // Save the favicon. DataURLToFaviconUsage will handle the case where
      // there is no favicon.
      if (favicons)
        DataURLToFaviconUsage(url, favicon, favicons);

      continue;
    }

    // Bookmarks in sub-folder are encapsulated with <DL> tag.
    if (StartsWithASCII(line, "<DL>", false)) {
      has_subfolder = true;
      if (!last_folder.empty()) {
        path.push_back(last_folder);
        last_folder.clear();
      }
      if (last_folder_on_toolbar && !toolbar_folder_index)
        toolbar_folder_index = path.size();

      // Mark next folder empty as initial state.
      last_folder_is_empty = true;
    } else if (StartsWithASCII(line, "</DL>", false)) {
      if (path.empty())
        break;  // Mismatch <DL>.

      base::string16 folder_title = path.back();
      path.pop_back();

      if (last_folder_is_empty) {
        // Empty folder should be added explicitly.
        ImportedBookmarkEntry entry;
        entry.is_folder = true;
        entry.creation_time = last_folder_add_date;
        entry.title = folder_title;
        if (toolbar_folder_index) {
          // The toolbar folder should be at the top level.
          // Make sure we don't add the toolbar folder itself if it is empty.
          if (toolbar_folder_index <= path.size()) {
            entry.in_toolbar = true;
            entry.path.assign(path.begin() + toolbar_folder_index - 1,
                              path.end());
            bookmarks->push_back(entry);
          }
        } else {
          // Add this folder to the list of |bookmarks|.
          entry.path.assign(path.begin(), path.end());
          bookmarks->push_back(entry);
        }

        // Parent folder include current one, so it's not empty.
        last_folder_is_empty = false;
      }

      if (toolbar_folder_index > path.size())
        toolbar_folder_index = 0;
    }
  }
}

namespace internal {

bool ParseCharsetFromLine(const std::string& line, std::string* charset) {
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

bool ParseFolderNameFromLine(const std::string& line,
                             const std::string& charset,
                             base::string16* folder_name,
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

  base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
                        base::OnStringConversionError::SKIP, folder_name);
  *folder_name = net::UnescapeForHTML(*folder_name);

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
      base::LowerCaseEqualsASCII(value, "true"))
    *is_toolbar_folder = true;
  else
    *is_toolbar_folder = false;

  return true;
}

bool ParseBookmarkFromLine(const std::string& line,
                           const std::string& charset,
                           base::string16* title,
                           GURL* url,
                           GURL* favicon,
                           base::string16* shortcut,
                           base::Time* add_date,
                           base::string16* post_data) {
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
  base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
                        base::OnStringConversionError::SKIP, title);
  *title = net::UnescapeForHTML(*title);

  // URL
  if (GetAttribute(attribute_list, kHrefAttribute, &value)) {
    base::string16 url16;
    base::CodepageToUTF16(value, charset.c_str(),
                          base::OnStringConversionError::SKIP, &url16);
    url16 = net::UnescapeForHTML(url16);

    *url = GURL(url16);
  }

  // Favicon
  if (GetAttribute(attribute_list, kIconAttribute, &value))
    *favicon = GURL(value);

  // Keyword
  if (GetAttribute(attribute_list, kShortcutURLAttribute, &value)) {
    base::CodepageToUTF16(value, charset.c_str(),
                          base::OnStringConversionError::SKIP, shortcut);
    *shortcut = net::UnescapeForHTML(*shortcut);
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
    base::CodepageToUTF16(value, charset.c_str(),
                          base::OnStringConversionError::SKIP, post_data);
    *post_data = net::UnescapeForHTML(*post_data);
  }

  return true;
}

bool ParseMinimumBookmarkFromLine(const std::string& line,
                                  const std::string& charset,
                                  base::string16* title,
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
  base::CodepageToUTF16(line.substr(tag_end, end - tag_end), charset.c_str(),
                        base::OnStringConversionError::SKIP, title);
  *title = net::UnescapeForHTML(*title);

  // URL
  std::string value;
  if (GetAttribute(attribute_list, kHrefAttributeUpper, &value) ||
      GetAttribute(attribute_list, kHrefAttributeLower, &value)) {
    if (charset.length() != 0) {
      base::string16 url16;
      base::CodepageToUTF16(value, charset.c_str(),
                            base::OnStringConversionError::SKIP, &url16);
      url16 = net::UnescapeForHTML(url16);

      *url = GURL(url16);
    } else {
      *url = GURL(value);
    }
  }

  return true;
}

}  // namespace internal

}  // namespace bookmark_html_reader
