// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/bookmarks_file_importer.h"

#include "base/bind.h"
#include "chrome/browser/bookmarks/bookmark_html_reader.h"
#include "chrome/browser/bookmarks/imported_bookmark_entry.h"
#include "chrome/browser/favicon/imported_favicon_usage.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "grit/generated_resources.h"

namespace {

bool IsImporterCancelled(BookmarksFileImporter* importer) {
  return importer->cancelled();
}

}  // namespace

BookmarksFileImporter::BookmarksFileImporter() {}

BookmarksFileImporter::~BookmarksFileImporter() {}

void BookmarksFileImporter::StartImport(
    const importer::SourceProfile& source_profile,
    uint16 items,
    ImporterBridge* bridge) {
  // The only thing this importer can import is a bookmarks file, aka
  // "favorites".
  DCHECK_EQ(importer::FAVORITES, items);

  bridge->NotifyStarted();
  bridge->NotifyItemStarted(importer::FAVORITES);

  std::vector<ImportedBookmarkEntry> bookmarks;
  std::vector<ImportedFaviconUsage> favicons;

  bookmark_html_reader::ImportBookmarksFile(
      base::Bind(IsImporterCancelled, base::Unretained(this)),
      base::Bind(CanImportURL),
      source_profile.source_path,
      &bookmarks,
      &favicons);

  if (!bookmarks.empty() && !cancelled()) {
    base::string16 first_folder_name =
        bridge->GetLocalizedString(IDS_BOOKMARK_GROUP);
    bridge->AddBookmarks(bookmarks, first_folder_name);
  }
  if (!favicons.empty())
    bridge->SetFavicons(favicons);

  bridge->NotifyItemEnded(importer::FAVORITES);
  bridge->NotifyEnded();
}
