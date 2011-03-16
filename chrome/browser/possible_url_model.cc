// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/possible_url_model.h"

#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/browser/cancelable_request.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/codec/png_codec.h"

using base::Time;
using base::TimeDelta;

namespace {

// The default favicon.
SkBitmap* default_favicon = NULL;

// How long we query entry points for.
const int kPossibleURLTimeScope = 30;

}  // anonymous namespace

// Contains the data needed to show a result.
struct PossibleURLModel::Result {
  Result() : index(0) {}

  GURL url;
  // Index of this Result in results_. This is used as the key into
  // favicon_map_ to lookup the favicon for the url, as well as the index
  // into results_ when the favicon is received.
  size_t index;
  ui::SortedDisplayURL display_url;
  std::wstring title;
};


PossibleURLModel::PossibleURLModel()
    : profile_(NULL),
      observer_(NULL) {
  if (!default_favicon) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_favicon = rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
  }
}

PossibleURLModel::~PossibleURLModel() {
}

void PossibleURLModel::Reload(Profile *profile) {
  profile_ = profile;
  consumer_.CancelAllRequests();
  HistoryService* hs =
      profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    history::QueryOptions options;
    options.end_time = Time::Now();
    options.begin_time =
        options.end_time - TimeDelta::FromDays(kPossibleURLTimeScope);
    options.max_count = 50;

    hs->QueryHistory(string16(), options, &consumer_,
        NewCallback(this, &PossibleURLModel::OnHistoryQueryComplete));
  }
}

void PossibleURLModel::OnHistoryQueryComplete(HistoryService::Handle h,
                                              history::QueryResults* result) {
  results_.resize(result->size());
  std::string languages = profile_ ?
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages) : std::string();
  for (size_t i = 0; i < result->size(); ++i) {
    results_[i].url = (*result)[i].url();
    results_[i].index = i;
    results_[i].display_url =
        ui::SortedDisplayURL((*result)[i].url(), languages);
    results_[i].title = UTF16ToWide((*result)[i].title());
  }

  // The old version of this code would filter out all but the most recent
  // visit to each host, plus all typed URLs and AUTO_BOOKMARK transitions. I
  // think this dialog has a lot of work, and I'm not sure those old
  // conditions are correct (the results look about equal quality for my
  // history with and without those conditions), so I'm not spending time
  // re-implementing them here. They used to be implemented in the history
  // service, but I think they should be implemented here because that was
  // pretty specific behavior that shouldn't be generally exposed.

  favicon_map_.clear();
  if (observer_)
    observer_->OnModelChanged();
}

int PossibleURLModel::RowCount() {
  return static_cast<int>(results_.size());
}

const GURL& PossibleURLModel::GetURL(int row) {
  if (row < 0 || row >= RowCount()) {
    NOTREACHED();
    return GURL::EmptyGURL();
  }
  return results_[row].url;
}

const std::wstring& PossibleURLModel::GetTitle(int row) {
  if (row < 0 || row >= RowCount()) {
    NOTREACHED();
    return EmptyWString();
  }
  return results_[row].title;
}

string16 PossibleURLModel::GetText(int row, int col_id) {
  if (row < 0 || row >= RowCount()) {
    NOTREACHED();
    return string16();
  }

  if (col_id == IDS_ASI_PAGE_COLUMN) {
    string16 title = WideToUTF16Hack(GetTitle(row));
    // TODO(xji): Consider adding a special case if the title text is a URL,
    // since those should always have LTR directionality. Please refer to
    // http://crbug.com/6726 for more information.
    base::i18n::AdjustStringForLocaleDirection(&title);
    return title;
  }

  // TODO(brettw): this should probably pass the GURL up so the URL elider
  // can be used at a higher level when we know the width.
  string16 url = results_[row].display_url.display_url();
  return base::i18n::GetDisplayStringInLTRDirectionality(url);
}

SkBitmap PossibleURLModel::GetIcon(int row) {
  if (row < 0 || row >= RowCount()) {
    NOTREACHED();
    return *default_favicon;
  }

  Result& result = results_[row];
  FavIconMap::iterator i = favicon_map_.find(result.index);
  if (i != favicon_map_.end()) {
    // We already requested the favicon, return it.
    if (!i->second.isNull())
      return i->second;
  } else if (profile_) {
    FaviconService* favicon_service =
        profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
    if (favicon_service) {
      CancelableRequestProvider::Handle h =
          favicon_service->GetFaviconForURL(
              result.url, history::FAVICON, &consumer_,
              NewCallback(this, &PossibleURLModel::OnFaviconAvailable));
      consumer_.SetClientData(favicon_service, h, result.index);
      // Add an entry to the map so that we don't attempt to request the
      // favicon again.
      favicon_map_[result.index] = SkBitmap();
    }
  }
  return *default_favicon;
}

int PossibleURLModel::CompareValues(int row1, int row2, int column_id) {
  if (column_id == IDS_ASI_URL_COLUMN) {
    return results_[row1].display_url.Compare(
        results_[row2].display_url, GetCollator());
  }
  return ui::TableModel::CompareValues(row1, row2, column_id);
}

void PossibleURLModel::OnFaviconAvailable(
    FaviconService::Handle h,
    history::FaviconData favicon) {
  if (profile_) {
    FaviconService* favicon_service =
        profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
    size_t index = consumer_.GetClientData(favicon_service, h);
    if (favicon.is_valid()) {
      // The decoder will leave our bitmap empty on error.
      gfx::PNGCodec::Decode(favicon.image_data->front(),
                            favicon.image_data->size(),
                            &(favicon_map_[index]));

      // Notify the observer.
      if (!favicon_map_[index].isNull() && observer_)
        observer_->OnItemsChanged(static_cast<int>(index), 1);
    }
  }
}

void PossibleURLModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}
