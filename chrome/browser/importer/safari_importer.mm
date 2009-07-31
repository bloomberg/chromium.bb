// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "chrome/browser/importer/safari_importer.h"

#include <vector>

#include "base/message_loop.h"
#include "base/scoped_nsobject.h"
#include "base/string16.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
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
    : writer_(NULL), library_dir_(library_dir) {
}

SafariImporter::~SafariImporter() {
}

void SafariImporter::StartImport(ProfileInfo profile_info,
                                 uint16 items, ProfileWriter* writer,
                                 MessageLoop* delegate_loop,
                                 ImporterHost* host) {
  writer_ = writer;
  importer_host_ = host;

  // The order here is important!
  NotifyStarted();
  if ((items & HOME_PAGE) && !cancelled())
    ImportHomepage();  // Doesn't have a UI item.
  if ((items & FAVORITES) && !cancelled()) {
    NotifyItemStarted(FAVORITES);
    ImportBookmarks();
    NotifyItemEnded(FAVORITES);
  }
  if ((items & SEARCH_ENGINES) && !cancelled()) {
    NotifyItemStarted(SEARCH_ENGINES);
    ImportSearchEngines();
    NotifyItemEnded(SEARCH_ENGINES);
  }
  if ((items & PASSWORDS) && !cancelled()) {
    NotifyItemStarted(PASSWORDS);
    ImportPasswords();
    NotifyItemEnded(PASSWORDS);
  }
  if ((items & HISTORY) && !cancelled()) {
    NotifyItemStarted(HISTORY);
    ImportHistory();
    NotifyItemEnded(HISTORY);
  }
  NotifyEnded();
}

void SafariImporter::ImportBookmarks() {
  NOTIMPLEMENTED();
}

void SafariImporter::ImportSearchEngines() {
  NOTIMPLEMENTED();
}

void SafariImporter::ImportPasswords() {
  // Safari stores it's passwords in the Keychain, same as us so we don't need
  // to import them.
  // Note: that we don't automatically pick them up, there is some logic around
  // the user needing to explicitly input his username in a page and bluring
  // the field before we pick it up, but the details of that are beyond the
  // scope of this comment.
}

void SafariImporter::ImportHistory() {
  std::vector<history::URLRow> rows;
  ParseHistoryItems(&rows);

  if (!rows.empty() && !cancelled()) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddHistoryPage, rows));
  }
}

double SafariImporter::HistoryTimeToEpochTime(NSString* history_time) {
  DCHECK(history_time);
  // Add Difference between Unix epoch and CFAbsoluteTime epoch in seconds.
  // Unix epoch is 1970-01-01 00:00:00.0 UTC,
  // CF epoch is 2001-01-01 00:00:00.0 UTC.
  return CFStringGetDoubleValue(reinterpret_cast<CFStringRef>(history_time)) +
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

  NSArray* safari_history_items = [history_dict valueForKey:@"WebHistoryDates"];

  for (NSDictionary* history_item in safari_history_items) {
    using base::SysNSStringToUTF8;
    using base::SysNSStringToWide;
    NSString* url_ns = [history_item valueForKey:@""];
    if (!url_ns)
      continue;

    GURL url(SysNSStringToUTF8(url_ns));

    if (!CanImportSafariURL(url))
      continue;

    history::URLRow row(url);
    NSString* title_ns = [history_item valueForKey:@"title"];

    // Sometimes items don't have a title, in which case we just substitue
    // the url.
    if (!title_ns)
      title_ns = url_ns;

    row.set_title(SysNSStringToWide(title_ns));
    int visit_count = [[history_item valueForKey:@"visitCount"]
                          intValue];
    row.set_visit_count(visit_count);
    // Include imported URLs in autocompletion - don't hide them.
    row.set_hidden(0);
    // Item was never typed before in the omnibox.
    row.set_typed_count(0);

    NSString* last_visit_str = [history_item valueForKey:@"lastVisitedDate"];
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

void SafariImporter::ImportHomepage() {
  const scoped_nsobject<NSString> homepage_ns(
      reinterpret_cast<const NSString*>(
          CFPreferencesCopyAppValue(CFSTR("HomePage"),
                                    CFSTR("com.apple.Safari"))));
  if (!homepage_ns.get())
    return;

  string16 hompeage_str = base::SysNSStringToUTF16(homepage_ns.get());
  GURL homepage(hompeage_str);
  if (homepage.is_valid()) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddHomepage, homepage));
  }
}

// TODO(jeremy): This is temporary, just copied from the FF import code in case
// we need it, clean this up when writing favicon import code.
// static
void SafariImporter::DataURLToFaviconUsage(
    const GURL& link_url,
    const GURL& favicon_data,
    std::vector<history::ImportedFavIconUsage>* favicons) {
  if (!link_url.is_valid() || !favicon_data.is_valid() ||
      !favicon_data.SchemeIs(chrome::kDataScheme))
    return;

  // Parse the data URL.
  std::string mime_type, char_set, data;
  if (!net::DataURL::Parse(favicon_data, &mime_type, &char_set, &data) ||
      data.empty())
    return;

  history::ImportedFavIconUsage usage;
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

