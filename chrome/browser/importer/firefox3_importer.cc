// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox3_importer.h"

#include <set>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/firefox2_importer.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/nss_decryptor.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/time_format.h"
#include "chrome/common/sqlite_utils.h"
#include "grit/generated_resources.h"
#include "webkit/glue/password_form.h"

using base::Time;
using importer::BOOKMARKS_HTML;
using importer::FAVORITES;
using importer::HISTORY;
using importer::HOME_PAGE;
using importer::PASSWORDS;
using importer::ProfileInfo;
using importer::SEARCH_ENGINES;
using webkit_glue::PasswordForm;

// Original definition is in http://mxr.mozilla.org/firefox/source/toolkit/
//  components/places/public/nsINavBookmarksService.idl
enum BookmarkItemType {
  TYPE_BOOKMARK = 1,
  TYPE_FOLDER = 2,
  TYPE_SEPARATOR = 3,
  TYPE_DYNAMIC_CONTAINER = 4
};

struct Firefox3Importer::BookmarkItem {
  int parent;
  int id;
  GURL url;
  std::wstring title;
  BookmarkItemType type;
  std::string keyword;
  base::Time date_added;
  int64 favicon;
  bool empty_folder;
};

Firefox3Importer::Firefox3Importer() {
#if defined(OS_LINUX)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  locale_ = g_browser_process->GetApplicationLocale();
#endif
}

Firefox3Importer::~Firefox3Importer() {
}

void Firefox3Importer::StartImport(const importer::ProfileInfo& profile_info,
                                   uint16 items,
                                   ImporterBridge* bridge) {
#if defined(OS_LINUX)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
#endif
  bridge_ = bridge;
  source_path_ = profile_info.source_path;
  app_path_ = profile_info.app_path;

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

void Firefox3Importer::ImportHistory() {
  FilePath file = source_path_.AppendASCII("places.sqlite");
  if (!file_util::PathExists(file))
    return;

  sqlite3* sqlite;
  if (sqlite3_open(WideToUTF8(file.ToWStringHack()).c_str(),
                   &sqlite) != SQLITE_OK) {
    return;
  }
  sqlite_utils::scoped_sqlite_db_ptr db(sqlite);

  SQLStatement s;
  // |visit_type| represent the transition type of URLs (typed, click,
  // redirect, bookmark, etc.) We eliminate some URLs like sub-frames and
  // redirects, since we don't want them to appear in history.
  // Firefox transition types are defined in:
  //   toolkit/components/places/public/nsINavHistoryService.idl
  const char* stmt = "SELECT h.url, h.title, h.visit_count, "
                     "h.hidden, h.typed, v.visit_date "
                     "FROM moz_places h JOIN moz_historyvisits v "
                     "ON h.id = v.place_id "
                     "WHERE v.visit_type <= 3";

  if (s.prepare(db.get(), stmt) != SQLITE_OK)
    return;

  std::vector<history::URLRow> rows;
  while (s.step() == SQLITE_ROW && !cancelled()) {
    GURL url(s.column_string(0));

    // Filter out unwanted URLs.
    if (!CanImportURL(url))
      continue;

    history::URLRow row(url);
    row.set_title(UTF8ToUTF16(s.column_string(1)));
    row.set_visit_count(s.column_int(2));
    row.set_hidden(s.column_int(3) == 1);
    row.set_typed_count(s.column_int(4));
    row.set_last_visit(Time::FromTimeT(s.column_int64(5)/1000000));

    rows.push_back(row);
  }
  if (!rows.empty() && !cancelled()) {
    bridge_->SetHistoryItems(rows, history::SOURCE_FIREFOX_IMPORTED);
  }
}

void Firefox3Importer::ImportBookmarks() {
  FilePath file = source_path_.AppendASCII("places.sqlite");
  if (!file_util::PathExists(file))
    return;

  sqlite3* sqlite;
  if (sqlite3_open(WideToUTF8(file.ToWStringHack()).c_str(),
                   &sqlite) != SQLITE_OK) {
    return;
  }
  sqlite_utils::scoped_sqlite_db_ptr db(sqlite);

  // Get the bookmark folders that we are interested in.
  int toolbar_folder_id = -1;
  int menu_folder_id = -1;
  int unsorted_folder_id = -1;
  LoadRootNodeID(db.get(), &toolbar_folder_id, &menu_folder_id,
                 &unsorted_folder_id);

  // Load livemark IDs.
  std::set<int> livemark_id;
  LoadLivemarkIDs(db.get(), &livemark_id);

  // Load the default bookmarks. Its storage is the same as Firefox 2.
  std::set<GURL> default_urls;
  Firefox2Importer::LoadDefaultBookmarks(app_path_, &default_urls);

  BookmarkList list;
  GetTopBookmarkFolder(db.get(), toolbar_folder_id, &list);
  GetTopBookmarkFolder(db.get(), menu_folder_id, &list);
  GetTopBookmarkFolder(db.get(), unsorted_folder_id, &list);
  size_t count = list.size();
  for (size_t i = 0; i < count; ++i)
    GetWholeBookmarkFolder(db.get(), &list, i, NULL);

  std::vector<ProfileWriter::BookmarkEntry> bookmarks;
  std::vector<TemplateURL*> template_urls;
  FaviconMap favicon_map;

  // TODO(jcampan): http://b/issue?id=1196285 we do not support POST based
  //                keywords yet.  We won't include them in the list.
  std::set<int> post_keyword_ids;
  SQLStatement s;
  const char* stmt = "SELECT b.id FROM moz_bookmarks b "
      "INNER JOIN moz_items_annos ia ON ia.item_id = b.id "
      "INNER JOIN moz_anno_attributes aa ON ia.anno_attribute_id = aa.id "
      "WHERE aa.name = 'bookmarkProperties/POSTData'";
  if (s.prepare(db.get(), stmt) == SQLITE_OK) {
    while (s.step() == SQLITE_ROW && !cancelled())
      post_keyword_ids.insert(s.column_int(0));
  } else {
    NOTREACHED();
    return;
  }

  std::wstring firefox_folder =
      bridge_->GetLocalizedString(IDS_BOOKMARK_GROUP_FROM_FIREFOX);
  for (size_t i = 0; i < list.size(); ++i) {
    BookmarkItem* item = list[i];

    if (item->type == TYPE_FOLDER) {
      // Folders are added implicitly on adding children,
      // so now we pass only empty folders to add them explicitly.
      if (!item->empty_folder)
        continue;
    } else if (item->type == TYPE_BOOKMARK) {
      // Import only valid bookmarks
      if (!CanImportURL(item->url))
        continue;
    } else {
      continue;
    }

    // Skip the default bookmarks and unwanted URLs.
    if (default_urls.find(item->url) != default_urls.end() ||
        post_keyword_ids.find(item->id) != post_keyword_ids.end())
      continue;

    // Find the bookmark path by tracing their links to parent folders.
    std::vector<std::wstring> path;
    BookmarkItem* child = item;
    bool found_path = false;
    bool is_in_toolbar = false;
    while (child->parent >= 0) {
      BookmarkItem* parent = list[child->parent];
      if (parent->id == toolbar_folder_id) {
        // This bookmark entry should be put in the bookmark bar.
        // But we put it in the Firefox group after first run, so
        // that do not mess up the bookmark bar.
        if (import_to_bookmark_bar()) {
          is_in_toolbar = true;
        } else {
          path.insert(path.begin(), parent->title);
          path.insert(path.begin(), firefox_folder);
        }
        found_path = true;
        break;
      } else if (parent->id == menu_folder_id ||
                 parent->id == unsorted_folder_id) {
        // After the first run, the item will be placed in a folder in
        // the "Other bookmarks".
        if (!import_to_bookmark_bar())
          path.insert(path.begin(), firefox_folder);
        found_path = true;
        break;
      } else if (livemark_id.find(parent->id) != livemark_id.end()) {
        // If the entry is under a livemark folder, we don't import it.
        break;
      }
      path.insert(path.begin(), parent->title);
      child = parent;
    }

    if (!found_path)
      continue;

    ProfileWriter::BookmarkEntry entry;
    entry.creation_time = item->date_added;
    entry.title = item->title;
    entry.url = item->url;
    entry.path = path;
    entry.in_toolbar = is_in_toolbar;
    entry.is_folder = item->type == TYPE_FOLDER;

    bookmarks.push_back(entry);

    if (item->favicon)
      favicon_map[item->favicon].insert(item->url);

    // This bookmark has a keyword, we import it to our TemplateURL model.
    TemplateURL* t_url = Firefox2Importer::CreateTemplateURL(
        item->title, UTF8ToWide(item->keyword), item->url);
    if (t_url)
      template_urls.push_back(t_url);
  }

  STLDeleteContainerPointers(list.begin(), list.end());

  // Write into profile.
  if (!bookmarks.empty() && !cancelled()) {
    const std::wstring& first_folder_name =
        bridge_->GetLocalizedString(IDS_BOOKMARK_GROUP_FROM_FIREFOX);
    int options = 0;
    if (import_to_bookmark_bar())
      options = ProfileWriter::IMPORT_TO_BOOKMARK_BAR;
    bridge_->AddBookmarkEntries(bookmarks, first_folder_name, options);
  }
  if (!template_urls.empty() && !cancelled()) {
    bridge_->SetKeywords(template_urls, -1, false);
  } else {
    STLDeleteContainerPointers(template_urls.begin(), template_urls.end());
  }
  if (!favicon_map.empty() && !cancelled()) {
    std::vector<history::ImportedFavIconUsage> favicons;
    LoadFavicons(db.get(), favicon_map, &favicons);
    bridge_->SetFavIcons(favicons);
  }
}

void Firefox3Importer::ImportPasswords() {
  // Initializes NSS3.
  NSSDecryptor decryptor;
  if (!decryptor.Init(source_path_.ToWStringHack(),
                      source_path_.ToWStringHack()) &&
      !decryptor.Init(app_path_.ToWStringHack(),
                      source_path_.ToWStringHack())) {
    return;
  }

  std::vector<PasswordForm> forms;
  FilePath source_path = source_path_;
  FilePath file = source_path.AppendASCII("signons.sqlite");
  if (file_util::PathExists(file)) {
    // Since Firefox 3.1, passwords are in signons.sqlite db.
    decryptor.ReadAndParseSignons(file, &forms);
  } else {
    // Firefox 3.0 uses signons3.txt to store the passwords.
    file = source_path.AppendASCII("signons3.txt");
    if (!file_util::PathExists(file))
      file = source_path.AppendASCII("signons2.txt");

    std::string content;
    file_util::ReadFileToString(file, &content);
    decryptor.ParseSignons(content, &forms);
  }

  if (!cancelled()) {
    for (size_t i = 0; i < forms.size(); ++i) {
      bridge_->SetPasswordForm(forms[i]);
    }
  }
}

void Firefox3Importer::ImportSearchEngines() {
  std::vector<FilePath> files;
  GetSearchEnginesXMLFiles(&files);

  std::vector<TemplateURL*> search_engines;
  ParseSearchEnginesFromXMLFiles(files, &search_engines);
  int default_index =
      GetFirefoxDefaultSearchEngineIndex(search_engines, source_path_);
  bridge_->SetKeywords(search_engines, default_index, true);
}

void Firefox3Importer::ImportHomepage() {
  GURL home_page = GetHomepage(source_path_);
  if (home_page.is_valid() && !IsDefaultHomepage(home_page, app_path_)) {
    bridge_->AddHomePage(home_page);
  }
}

void Firefox3Importer::GetSearchEnginesXMLFiles(
    std::vector<FilePath>* files) {
  FilePath file = source_path_.AppendASCII("search.sqlite");
  if (!file_util::PathExists(file))
    return;

  sqlite3* sqlite;
  if (sqlite3_open(WideToUTF8(file.ToWStringHack()).c_str(),
                   &sqlite) != SQLITE_OK) {
    return;
  }
  sqlite_utils::scoped_sqlite_db_ptr db(sqlite);

  SQLStatement s;
  const char* stmt = "SELECT engineid FROM engine_data "
                     "WHERE engineid NOT IN "
                     "(SELECT engineid FROM engine_data "
                     "WHERE name='hidden') "
                     "ORDER BY value ASC";

  if (s.prepare(db.get(), stmt) != SQLITE_OK)
    return;

  FilePath app_path = app_path_.AppendASCII("searchplugins");
  FilePath profile_path = source_path_.AppendASCII("searchplugins");

  // Firefox doesn't store a search engine in its sqlite database unless the
  // user has added a engine. So we get search engines from sqlite db as well
  // as from the file system.
  if (s.step() == SQLITE_ROW) {
    const std::wstring kAppPrefix = L"[app]/";
    const std::wstring kProfilePrefix = L"[profile]/";
    do {
      FilePath file;
      std::wstring engine = UTF8ToWide(s.column_string(0));

      // The string contains [app]/<name>.xml or [profile]/<name>.xml where
      // the [app] and [profile] need to be replaced with the actual app or
      // profile path.
      size_t index = engine.find(kAppPrefix);
      if (index != std::wstring::npos) {
        // Remove '[app]/'.
        file = app_path.Append(FilePath::FromWStringHack(
                                   engine.substr(index + kAppPrefix.length())));
      } else if ((index = engine.find(kProfilePrefix)) != std::wstring::npos) {
        // Remove '[profile]/'.
          file = profile_path.Append(
              FilePath::FromWStringHack(
                  engine.substr(index + kProfilePrefix.length())));
      } else {
        // Looks like absolute path to the file.
        file = FilePath::FromWStringHack(engine);
      }
      files->push_back(file);
    } while (s.step() == SQLITE_ROW && !cancelled());
  }

#if defined(OS_LINUX)
  // Ubuntu-flavored Firefox3 supports locale-specific search engines via
  // locale-named subdirectories. They fall back to en-US.
  // See http://crbug.com/53899
  // TODO(jshin): we need to make sure our locale code matches that of
  // Firefox.
  FilePath locale_app_path = app_path.AppendASCII(locale_);
  FilePath default_locale_app_path = app_path.AppendASCII("en-US");
  if (file_util::DirectoryExists(locale_app_path))
    app_path = locale_app_path;
  else if (file_util::DirectoryExists(default_locale_app_path))
    app_path = default_locale_app_path;
#endif

  // Get search engine definition from file system.
  file_util::FileEnumerator engines(app_path, false,
                                    file_util::FileEnumerator::FILES);
  for (FilePath engine_path = engines.Next(); !engine_path.value().empty();
       engine_path = engines.Next()) {
    files->push_back(engine_path);
  }
}

void Firefox3Importer::LoadRootNodeID(sqlite3* db,
                                      int* toolbar_folder_id,
                                      int* menu_folder_id,
                                      int* unsorted_folder_id) {
  const char kToolbarFolderName[] = "toolbar";
  const char kMenuFolderName[] = "menu";
  const char kUnsortedFolderName[] = "unfiled";

  SQLStatement s;
  const char* stmt = "SELECT root_name, folder_id FROM moz_bookmarks_roots";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  while (s.step() == SQLITE_ROW) {
    std::string folder = s.column_string(0);
    int id = s.column_int(1);
    if (folder == kToolbarFolderName)
      *toolbar_folder_id = id;
    else if (folder == kMenuFolderName)
      *menu_folder_id = id;
    else if (folder == kUnsortedFolderName)
      *unsorted_folder_id = id;
  }
}

void Firefox3Importer::LoadLivemarkIDs(sqlite3* db,
                                       std::set<int>* livemark) {
  const char kFeedAnnotation[] = "livemark/feedURI";
  livemark->clear();

  SQLStatement s;
  const char* stmt = "SELECT b.item_id "
                     "FROM moz_anno_attributes a "
                     "JOIN moz_items_annos b ON a.id = b.anno_attribute_id "
                     "WHERE a.name = ? ";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  s.bind_string(0, kFeedAnnotation);
  while (s.step() == SQLITE_ROW && !cancelled())
    livemark->insert(s.column_int(0));
}

void Firefox3Importer::GetTopBookmarkFolder(sqlite3* db, int folder_id,
                                            BookmarkList* list) {
  SQLStatement s;
  const char* stmt = "SELECT b.title "
         "FROM moz_bookmarks b "
         "WHERE b.type = 2 AND b.id = ? "
         "ORDER BY b.position";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  s.bind_int(0, folder_id);
  if (s.step() == SQLITE_ROW) {
    BookmarkItem* item = new BookmarkItem;
    item->parent = -1;  // The top level folder has no parent.
    item->id = folder_id;
    item->title = s.column_wstring(0);
    item->type = TYPE_FOLDER;
    item->favicon = 0;
    item->empty_folder = true;
    list->push_back(item);
  }
}

void Firefox3Importer::GetWholeBookmarkFolder(sqlite3* db, BookmarkList* list,
                                              size_t position,
                                              bool* empty_folder) {
  if (position >= list->size()) {
    NOTREACHED();
    return;
  }

  SQLStatement s;
  const char* stmt = "SELECT b.id, h.url, COALESCE(b.title, h.title), "
         "b.type, k.keyword, b.dateAdded, h.favicon_id "
         "FROM moz_bookmarks b "
         "LEFT JOIN moz_places h ON b.fk = h.id "
         "LEFT JOIN moz_keywords k ON k.id = b.keyword_id "
         "WHERE b.type IN (1,2) AND b.parent = ? "
         "ORDER BY b.position";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  s.bind_int(0, (*list)[position]->id);
  BookmarkList temp_list;
  while (s.step() == SQLITE_ROW) {
    BookmarkItem* item = new BookmarkItem;
    item->parent = static_cast<int>(position);
    item->id = s.column_int(0);
    item->url = GURL(s.column_string(1));
    item->title = s.column_wstring(2);
    item->type = static_cast<BookmarkItemType>(s.column_int(3));
    item->keyword = s.column_string(4);
    item->date_added = Time::FromTimeT(s.column_int64(5)/1000000);
    item->favicon = s.column_int64(6);
    item->empty_folder = true;

    temp_list.push_back(item);
    if (empty_folder != NULL)
      *empty_folder = false;
  }

  // Appends all items to the list.
  for (BookmarkList::iterator i = temp_list.begin();
       i != temp_list.end(); ++i) {
    list->push_back(*i);
    // Recursive add bookmarks in sub-folders.
    if ((*i)->type == TYPE_FOLDER)
      GetWholeBookmarkFolder(db, list, list->size() - 1, &(*i)->empty_folder);
  }
}

void Firefox3Importer::LoadFavicons(
    sqlite3* db,
    const FaviconMap& favicon_map,
    std::vector<history::ImportedFavIconUsage>* favicons) {
  SQLStatement s;
  const char* stmt = "SELECT url, data FROM moz_favicons WHERE id=?";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  for (FaviconMap::const_iterator i = favicon_map.begin();
       i != favicon_map.end(); ++i) {
    s.bind_int64(0, i->first);
    if (s.step() == SQLITE_ROW) {
      history::ImportedFavIconUsage usage;

      usage.favicon_url = GURL(s.column_string(0));
      if (!usage.favicon_url.is_valid())
        continue;  // Don't bother importing favicons with invalid URLs.

      std::vector<unsigned char> data;
      if (!s.column_blob_as_vector(1, &data) || data.empty())
        continue;  // Data definitely invalid.

      if (!ReencodeFavicon(&data[0], data.size(), &usage.png_data))
        continue;  // Unable to decode.

      usage.urls = i->second;
      favicons->push_back(usage);
    }
    s.reset();
  }
}
