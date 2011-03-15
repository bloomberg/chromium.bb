// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "chrome/browser/importer/safari_importer.h"

#include <map>
#include <vector>

#include "base/file_util.h"
#include "base/mac/mac_util.h"
#include "base/message_loop.h"
#include "base/scoped_nsobject.h"
#include "base/string16.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/common/sqlite_utils.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/data_url.h"

namespace {

// A function like this is used by other importers in order to filter out
// URLS we don't want to import.
// For now it's pretty basic, but I've split it out so it's easy to slot
// in necessary logic for filtering URLS, should we need it.
bool CanImportSafariURL(const GURL& url) {
  // The URL is not valid.
  if (!url.is_valid())
    return false;

  return true;
}

}  // namespace

SafariImporter::SafariImporter(const FilePath& library_dir)
    : library_dir_(library_dir) {
}

SafariImporter::~SafariImporter() {
}

// static
bool SafariImporter::CanImport(const FilePath& library_dir,
                               uint16 *services_supported) {
  DCHECK(services_supported);
  *services_supported = importer::NONE;

  // Import features are toggled by the following:
  // bookmarks import: existence of ~/Library/Safari/Bookmarks.plist file.
  // history import: existence of ~/Library/Safari/History.plist file.
  FilePath safari_dir = library_dir.Append("Safari");
  FilePath bookmarks_path = safari_dir.Append("Bookmarks.plist");
  FilePath history_path = safari_dir.Append("History.plist");

  using file_util::PathExists;
  if (PathExists(bookmarks_path))
    *services_supported |= importer::FAVORITES;
  if (PathExists(history_path))
    *services_supported |= importer::HISTORY;

  return *services_supported != importer::NONE;
}

void SafariImporter::StartImport(const importer::ProfileInfo& profile_info,
                                 uint16 services_supported,
                                 ImporterBridge* bridge) {
  bridge_ = bridge;
  // The order here is important!
  bridge_->NotifyStarted();
  // In keeping with import on other platforms (and for other browsers), we
  // don't import the home page (since it may lead to a useless homepage); see
  // crbug.com/25603.
  if ((services_supported & importer::HISTORY) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::HISTORY);
    ImportHistory();
    bridge_->NotifyItemEnded(importer::HISTORY);
  }
  if ((services_supported & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportBookmarks();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
  if ((services_supported & importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::PASSWORDS);
    ImportPasswords();
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }
  bridge_->NotifyEnded();
}

void SafariImporter::ImportBookmarks() {
  std::vector<ProfileWriter::BookmarkEntry> bookmarks;
  ParseBookmarks(&bookmarks);

  // Write bookmarks into profile.
  if (!bookmarks.empty() && !cancelled()) {
    const std::wstring& first_folder_name =
        bridge_->GetLocalizedString(IDS_BOOKMARK_GROUP_FROM_SAFARI);
    int options = 0;
    if (import_to_bookmark_bar())
      options = ProfileWriter::IMPORT_TO_BOOKMARK_BAR;
    bridge_->AddBookmarkEntries(bookmarks, first_folder_name, options);
  }

  // Import favicons.
  sqlite_utils::scoped_sqlite_db_ptr db(OpenFavIconDB());
  FaviconMap favicon_map;
  ImportFavIconURLs(db.get(), &favicon_map);
  // Write favicons into profile.
  if (!favicon_map.empty() && !cancelled()) {
    std::vector<history::ImportedFaviconUsage> favicons;
    LoadFaviconData(db.get(), favicon_map, &favicons);
    bridge_->SetFavicons(favicons);
  }
}

sqlite3* SafariImporter::OpenFavIconDB() {
  // Construct ~/Library/Safari/WebIcons.db path
  NSString* library_dir = [NSString
      stringWithUTF8String:library_dir_.value().c_str()];
  NSString* safari_dir = [library_dir
      stringByAppendingPathComponent:@"Safari"];
  NSString* favicons_db_path = [safari_dir
    stringByAppendingPathComponent:@"WebpageIcons.db"];

  sqlite3* favicons_db;
  const char* safariicons_dbname = [favicons_db_path fileSystemRepresentation];
  if (sqlite3_open(safariicons_dbname, &favicons_db) != SQLITE_OK)
    return NULL;

  return favicons_db;
}

void SafariImporter::ImportFavIconURLs(sqlite3* db, FaviconMap* favicon_map) {
  SQLStatement s;
  const char* stmt = "SELECT iconID, url FROM PageURL;";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  while (s.step() == SQLITE_ROW && !cancelled()) {
    int64 icon_id = s.column_int(0);
    GURL url = GURL(s.column_string(1));
    (*favicon_map)[icon_id].insert(url);
  }
}

void SafariImporter::LoadFaviconData(sqlite3* db,
                                     const FaviconMap& favicon_map,
                        std::vector<history::ImportedFaviconUsage>* favicons) {
  SQLStatement s;
  const char* stmt = "SELECT i.url, d.data "
                     "FROM IconInfo i JOIN IconData d "
                     "ON i.iconID = d.iconID "
                     "WHERE i.iconID = ?;";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  for (FaviconMap::const_iterator i = favicon_map.begin();
       i != favicon_map.end(); ++i) {
    s.bind_int64(0, i->first);
    if (s.step() == SQLITE_ROW) {
      history::ImportedFaviconUsage usage;

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

void SafariImporter::RecursiveReadBookmarksFolder(
    NSDictionary* bookmark_folder,
    const std::vector<std::wstring>& parent_path_elements,
    bool is_in_toolbar,
    std::vector<ProfileWriter::BookmarkEntry>* out_bookmarks) {
  DCHECK(bookmark_folder);

  NSString* type = [bookmark_folder objectForKey:@"WebBookmarkType"];
  NSString* title = [bookmark_folder objectForKey:@"Title"];

  // Are we the dictionary that contains all other bookmarks?
  // We need to know this so we don't add it to the path.
  bool is_top_level_bookmarks_container = [bookmark_folder
      objectForKey:@"WebBookmarkFileVersion"] != nil;

  // We're expecting a list of bookmarks here, if that isn't what we got, fail.
  if (!is_top_level_bookmarks_container) {
    // Top level containers sometimes don't have title attributes.
    if (![type isEqualToString:@"WebBookmarkTypeList"] || !title) {
      DCHECK(false) << "Type =("
      << (type ? base::SysNSStringToUTF8(type) : "Null Type")
      << ") Title=(" << (title ? base::SysNSStringToUTF8(title) : "Null title")
      << ")";
      return;
    }
  }

  std::vector<std::wstring> path_elements(parent_path_elements);
  // Is this the toolbar folder?
  if ([title isEqualToString:@"BookmarksBar"]) {
    // Be defensive, the toolbar items shouldn't have a prepended path.
    path_elements.clear();
    is_in_toolbar = true;
  } else if ([title isEqualToString:@"BookmarksMenu"]) {
    // top level container for normal bookmarks.
    path_elements.clear();
  } else if (!is_top_level_bookmarks_container) {
    if (title)
      path_elements.push_back(base::SysNSStringToWide(title));
  }

  NSArray* elements = [bookmark_folder objectForKey:@"Children"];
  // TODO(jeremy) Does Chrome support importing empty folders?
  if (!elements)
    return;

  // Iterate over individual bookmarks.
  for (NSDictionary* bookmark in elements) {
    NSString* type = [bookmark objectForKey:@"WebBookmarkType"];
    if (!type)
      continue;

    // If this is a folder, recurse.
    if ([type isEqualToString:@"WebBookmarkTypeList"]) {
      RecursiveReadBookmarksFolder(bookmark,
                                   path_elements,
                                   is_in_toolbar,
                                   out_bookmarks);
    }

    // If we didn't see a bookmark folder, then we're expecting a bookmark
    // item, if that's not what we got then ignore it.
    if (![type isEqualToString:@"WebBookmarkTypeLeaf"])
      continue;

    NSString* url = [bookmark objectForKey:@"URLString"];
    NSString* title = [[bookmark objectForKey:@"URIDictionary"]
        objectForKey:@"title"];

    if (!url || !title)
      continue;

    // Output Bookmark.
    ProfileWriter::BookmarkEntry entry;
    // Safari doesn't specify a creation time for the bookmark.
    entry.creation_time = base::Time::Now();
    entry.title = base::SysNSStringToWide(title);
    entry.url = GURL(base::SysNSStringToUTF8(url));
    entry.path = path_elements;
    entry.in_toolbar = is_in_toolbar;

    out_bookmarks->push_back(entry);
  }
}

void SafariImporter::ParseBookmarks(
    std::vector<ProfileWriter::BookmarkEntry>* bookmarks) {
  DCHECK(bookmarks);

  // Construct ~/Library/Safari/Bookmarks.plist path
  NSString* library_dir = [NSString
      stringWithUTF8String:library_dir_.value().c_str()];
  NSString* safari_dir = [library_dir
      stringByAppendingPathComponent:@"Safari"];
  NSString* bookmarks_plist = [safari_dir
    stringByAppendingPathComponent:@"Bookmarks.plist"];

  // Load the plist file.
  NSDictionary* bookmarks_dict = [NSDictionary
      dictionaryWithContentsOfFile:bookmarks_plist];
  if (!bookmarks_dict)
    return;

  // Recursively read in bookmarks.
  std::vector<std::wstring> parent_path_elements;
  RecursiveReadBookmarksFolder(bookmarks_dict, parent_path_elements, false,
                               bookmarks);
}

void SafariImporter::ImportPasswords() {
  // Safari stores it's passwords in the Keychain, same as us so we don't need
  // to import them.
  // Note: that we don't automatically pick them up, there is some logic around
  // the user needing to explicitly input his username in a page and blurring
  // the field before we pick it up, but the details of that are beyond the
  // scope of this comment.
}

void SafariImporter::ImportHistory() {
  std::vector<history::URLRow> rows;
  ParseHistoryItems(&rows);

  if (!rows.empty() && !cancelled()) {
    bridge_->SetHistoryItems(rows, history::SOURCE_SAFARI_IMPORTED);
  }
}

double SafariImporter::HistoryTimeToEpochTime(NSString* history_time) {
  DCHECK(history_time);
  // Add Difference between Unix epoch and CFAbsoluteTime epoch in seconds.
  // Unix epoch is 1970-01-01 00:00:00.0 UTC,
  // CF epoch is 2001-01-01 00:00:00.0 UTC.
  return CFStringGetDoubleValue(base::mac::NSToCFCast(history_time)) +
      kCFAbsoluteTimeIntervalSince1970;
}

void SafariImporter::ParseHistoryItems(
    std::vector<history::URLRow>* history_items) {
  DCHECK(history_items);

  // Construct ~/Library/Safari/History.plist path
  NSString* library_dir = [NSString
      stringWithUTF8String:library_dir_.value().c_str()];
  NSString* safari_dir = [library_dir
      stringByAppendingPathComponent:@"Safari"];
  NSString* history_plist = [safari_dir
      stringByAppendingPathComponent:@"History.plist"];

  // Load the plist file.
  NSDictionary* history_dict = [NSDictionary
      dictionaryWithContentsOfFile:history_plist];
  if (!history_dict)
    return;

  NSArray* safari_history_items = [history_dict
      objectForKey:@"WebHistoryDates"];

  for (NSDictionary* history_item in safari_history_items) {
    using base::SysNSStringToUTF8;
    using base::SysNSStringToUTF16;
    NSString* url_ns = [history_item objectForKey:@""];
    if (!url_ns)
      continue;

    GURL url(SysNSStringToUTF8(url_ns));

    if (!CanImportSafariURL(url))
      continue;

    history::URLRow row(url);
    NSString* title_ns = [history_item objectForKey:@"title"];

    // Sometimes items don't have a title, in which case we just substitue
    // the url.
    if (!title_ns)
      title_ns = url_ns;

    row.set_title(SysNSStringToUTF16(title_ns));
    int visit_count = [[history_item objectForKey:@"visitCount"]
                          intValue];
    row.set_visit_count(visit_count);
    // Include imported URLs in autocompletion - don't hide them.
    row.set_hidden(0);
    // Item was never typed before in the omnibox.
    row.set_typed_count(0);

    NSString* last_visit_str = [history_item objectForKey:@"lastVisitedDate"];
    // The last visit time should always be in the history item, but if not
    /// just continue without this item.
    DCHECK(last_visit_str);
    if (!last_visit_str)
      continue;

    // Convert Safari's last visit time to Unix Epoch time.
    double seconds_since_unix_epoch = HistoryTimeToEpochTime(last_visit_str);
    row.set_last_visit(base::Time::FromDoubleT(seconds_since_unix_epoch));

    history_items->push_back(row);
  }
}
