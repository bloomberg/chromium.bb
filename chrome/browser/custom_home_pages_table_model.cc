// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_home_pages_table_model.h"

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "gfx/codec/png_codec.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/base/resource/resource_bundle.h"

struct CustomHomePagesTableModel::Entry {
  Entry() : title_handle(0), fav_icon_handle(0) {}

  // URL of the page.
  GURL url;

  // Page title.  If this is empty, we'll display the URL as the entry.
  string16 title;

  // Icon for the page.
  SkBitmap icon;

  // If non-zero, indicates we're loading the title for the page.
  HistoryService::Handle title_handle;

  // If non-zero, indicates we're loading the favicon for the page.
  FaviconService::Handle fav_icon_handle;
};

CustomHomePagesTableModel::CustomHomePagesTableModel(Profile* profile)
    : default_favicon_(NULL),
      profile_(profile),
      observer_(NULL) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  default_favicon_ = rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
}

CustomHomePagesTableModel::~CustomHomePagesTableModel() {
}

void CustomHomePagesTableModel::SetURLs(const std::vector<GURL>& urls) {
  entries_.resize(urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    entries_[i].url = urls[i];
    entries_[i].title.erase();
    entries_[i].icon.reset();
    LoadTitleAndFavIcon(&(entries_[i]));
  }
  // Complete change, so tell the view to just rebuild itself.
  if (observer_)
    observer_->OnModelChanged();
}

void CustomHomePagesTableModel::Add(int index, const GURL& url) {
  DCHECK(index >= 0 && index <= RowCount());
  entries_.insert(entries_.begin() + static_cast<size_t>(index), Entry());
  entries_[index].url = url;
  LoadTitleAndFavIcon(&(entries_[index]));
  if (observer_)
    observer_->OnItemsAdded(index, 1);
}

void CustomHomePagesTableModel::Remove(int index) {
  DCHECK(index >= 0 && index < RowCount());
  Entry* entry = &(entries_[index]);
  // Cancel any pending load requests now so we don't deref a bogus pointer when
  // we get the loaded notification.
  if (entry->title_handle) {
    HistoryService* history_service =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history_service)
      history_service->CancelRequest(entry->title_handle);
  }
  if (entry->fav_icon_handle) {
    FaviconService* favicon_service =
        profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
    if (favicon_service)
      favicon_service->CancelRequest(entry->fav_icon_handle);
  }
  entries_.erase(entries_.begin() + static_cast<size_t>(index));
  if (observer_)
    observer_->OnItemsRemoved(index, 1);
}

void CustomHomePagesTableModel::SetToCurrentlyOpenPages() {
  // Remove the current entries.
  while (RowCount())
    Remove(0);

  // And add all tabs for all open browsers with our profile.
  int add_index = 0;
  for (BrowserList::const_iterator browser_i = BrowserList::begin();
       browser_i != BrowserList::end(); ++browser_i) {
    Browser* browser = *browser_i;
    if (browser->profile() != profile_)
      continue;  // Skip incognito browsers.

    for (int tab_index = 0; tab_index < browser->tab_count(); ++tab_index) {
      const GURL url = browser->GetTabContentsAt(tab_index)->GetURL();
      if (!url.is_empty() &&
          !(url.SchemeIs(chrome::kChromeUIScheme) &&
            url.host() == chrome::kChromeUISettingsHost))
        Add(add_index++, url);
    }
  }
}

std::vector<GURL> CustomHomePagesTableModel::GetURLs() {
  std::vector<GURL> urls(entries_.size());
  for (size_t i = 0; i < entries_.size(); ++i)
    urls[i] = entries_[i].url;
  return urls;
}

int CustomHomePagesTableModel::RowCount() {
  return static_cast<int>(entries_.size());
}

string16 CustomHomePagesTableModel::GetText(int row, int column_id) {
  DCHECK(column_id == 0);
  DCHECK(row >= 0 && row < RowCount());
  return entries_[row].title.empty() ? FormattedURL(row) : entries_[row].title;
}

SkBitmap CustomHomePagesTableModel::GetIcon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return entries_[row].icon.isNull() ? *default_favicon_ : entries_[row].icon;
}

string16 CustomHomePagesTableModel::GetTooltip(int row) {
  return entries_[row].title.empty() ? string16() :
      l10n_util::GetStringFUTF16(IDS_OPTIONS_STARTUP_PAGE_TOOLTIP,
                                 entries_[row].title, FormattedURL(row));
}

void CustomHomePagesTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

void CustomHomePagesTableModel::LoadTitleAndFavIcon(Entry* entry) {
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (history_service) {
    entry->title_handle = history_service->QueryURL(entry->url, false,
        &query_consumer_,
        NewCallback(this, &CustomHomePagesTableModel::OnGotTitle));
  }
  FaviconService* favicon_service =
      profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
  if (favicon_service) {
    entry->fav_icon_handle = favicon_service->GetFaviconForURL(entry->url,
        &query_consumer_,
        NewCallback(this, &CustomHomePagesTableModel::OnGotFavIcon));
  }
}

void CustomHomePagesTableModel::OnGotTitle(HistoryService::Handle handle,
                                           bool found_url,
                                           const history::URLRow* row,
                                           history::VisitVector* visits) {
  int entry_index;
  Entry* entry =
      GetEntryByLoadHandle(&Entry::title_handle, handle, &entry_index);
  if (!entry) {
    // The URLs changed before we were called back.
    return;
  }
  entry->title_handle = 0;
  if (found_url && !row->title().empty()) {
    entry->title = row->title();
    if (observer_)
      observer_->OnItemsChanged(static_cast<int>(entry_index), 1);
  }
}

void CustomHomePagesTableModel::OnGotFavIcon(
    FaviconService::Handle handle,
    bool know_fav_icon,
    scoped_refptr<RefCountedMemory> image_data,
    bool is_expired,
    GURL icon_url) {
  int entry_index;
  Entry* entry =
      GetEntryByLoadHandle(&Entry::fav_icon_handle, handle, &entry_index);
  if (!entry) {
    // The URLs changed before we were called back.
    return;
  }
  entry->fav_icon_handle = 0;
  if (know_fav_icon && image_data.get() && image_data->size()) {
    int width, height;
    std::vector<unsigned char> decoded_data;
    if (gfx::PNGCodec::Decode(image_data->front(),
                              image_data->size(),
                              gfx::PNGCodec::FORMAT_BGRA, &decoded_data,
                              &width, &height)) {
      entry->icon.setConfig(SkBitmap::kARGB_8888_Config, width, height);
      entry->icon.allocPixels();
      memcpy(entry->icon.getPixels(), &decoded_data.front(),
             width * height * 4);
      if (observer_)
        observer_->OnItemsChanged(static_cast<int>(entry_index), 1);
    }
  }
}

CustomHomePagesTableModel::Entry*
    CustomHomePagesTableModel::GetEntryByLoadHandle(
    CancelableRequestProvider::Handle Entry::* member,
    CancelableRequestProvider::Handle handle,
    int* index) {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].*member == handle) {
      *index = static_cast<int>(i);
      return &entries_[i];
    }
  }
  return NULL;
}

string16 CustomHomePagesTableModel::FormattedURL(int row) const {
  std::string languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  string16 url = net::FormatUrl(entries_[row].url, languages);
  url = base::i18n::GetDisplayStringInLTRDirectionality(url);
  return url;
}
