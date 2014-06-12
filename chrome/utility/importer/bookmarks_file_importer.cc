// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/bookmarks_file_importer.h"

#include "base/bind.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/utility/importer/bookmark_html_reader.h"
#include "components/url_fixer/url_fixer.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"

namespace {

bool IsImporterCancelled(BookmarksFileImporter* importer) {
  return importer->cancelled();
}

}  // namespace

namespace internal {

// Returns true if |url| has a valid scheme that we allow to import. We
// filter out the URL with a unsupported scheme.
bool CanImportURL(const GURL& url) {
  // The URL is not valid.
  if (!url.is_valid())
    return false;

  // Filter out the URLs with unsupported schemes.
  const char* const kInvalidSchemes[] = {"wyciwyg", "place"};
  for (size_t i = 0; i < arraysize(kInvalidSchemes); ++i) {
    if (url.SchemeIs(kInvalidSchemes[i]))
      return false;
  }

  // Check if |url| is about:blank.
  if (url == GURL(url::kAboutBlankURL))
    return true;

  // If |url| starts with chrome:// or about:, check if it's one of the URLs
  // that we support.
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(url::kAboutScheme)) {
    if (url.host() == chrome::kChromeUIUberHost ||
        url.host() == chrome::kChromeUIAboutHost)
      return true;

    GURL fixed_url(url_fixer::FixupURL(url.spec(), std::string()));
    for (size_t i = 0; i < chrome::kNumberOfChromeHostURLs; ++i) {
      if (fixed_url.DomainIs(chrome::kChromeHostURLs[i]))
        return true;
    }

    for (int i = 0; i < chrome::kNumberOfChromeDebugURLs; ++i) {
      if (fixed_url == GURL(chrome::kChromeDebugURLs[i]))
        return true;
    }

    // If url has either chrome:// or about: schemes but wasn't found in the
    // above lists, it means we don't support it, so we don't allow the user
    // to import it.
    return false;
  }

  // Otherwise, we assume the url has a valid (importable) scheme.
  return true;
}

}  // namespace internal

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
      base::Bind(internal::CanImportURL),
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
