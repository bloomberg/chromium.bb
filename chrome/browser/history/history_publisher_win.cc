// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_publisher.h"

#include <atlsafe.h>
#include <oleauto.h>
#include <wtypes.h>

#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "googleurl/src/gurl.h"

namespace {

// Instantiates the registered indexers from the registry |root| + |path| key
// and adds them to the |indexers| list.
void AddRegisteredIndexers(
    HKEY root,
    const wchar_t* path,
    std::vector< base::win::ScopedComPtr<IChromeHistoryIndexer> >* indexers) {
  for (base::win::RegistryKeyIterator r_iter(root, path); r_iter.Valid();
       ++r_iter) {
    CLSID clsid;
    if (FAILED(CLSIDFromString(const_cast<wchar_t*>(r_iter.Name()), &clsid)))
      continue;
    base::win::ScopedComPtr<IChromeHistoryIndexer> indexer;
    if (SUCCEEDED(indexer.CreateInstance(clsid, NULL, CLSCTX_INPROC))) {
      indexers->push_back(indexer);
      indexer.Release();
    }
  }
}

}  // namespace

namespace history {

const wchar_t* const HistoryPublisher::kRegKeyRegisteredIndexersInfo =
    L"Software\\Google\\Google Chrome\\IndexerPlugins";

// static
double HistoryPublisher::TimeToUTCVariantTime(const base::Time& time) {
  double var_time = 0;
  if (!time.is_null()) {
    base::Time::Exploded exploded;
    time.UTCExplode(&exploded);

    // Create the system time struct representing our exploded time.
    SYSTEMTIME system_time;
    system_time.wYear = exploded.year;
    system_time.wMonth = exploded.month;
    system_time.wDayOfWeek = exploded.day_of_week;
    system_time.wDay = exploded.day_of_month;
    system_time.wHour = exploded.hour;
    system_time.wMinute = exploded.minute;
    system_time.wSecond = exploded.second;
    system_time.wMilliseconds = exploded.millisecond;
    SystemTimeToVariantTime(&system_time, &var_time);
  }

  return var_time;
}

HistoryPublisher::HistoryPublisher() {
}

HistoryPublisher::~HistoryPublisher() {
}

bool HistoryPublisher::Init() {
  return ReadRegisteredIndexersFromRegistry();
}

// Peruse the registry for Indexer to instantiate and store in |indexers_|.
// Return true if we found at least one indexer object. We look both in HKCU
// and HKLM.
bool HistoryPublisher::ReadRegisteredIndexersFromRegistry() {
  AddRegisteredIndexers(HKEY_CURRENT_USER,
                        kRegKeyRegisteredIndexersInfo, &indexers_);
  AddRegisteredIndexers(HKEY_LOCAL_MACHINE,
                        kRegKeyRegisteredIndexersInfo, &indexers_);
  return !indexers_.empty();
}

void HistoryPublisher::PublishDataToIndexers(const PageData& page_data)
    const {
  double var_time = TimeToUTCVariantTime(page_data.time);

  CComSafeArray<unsigned char> thumbnail_arr;
  if (page_data.thumbnail) {
    for (size_t i = 0; i < page_data.thumbnail->size(); ++i)
      thumbnail_arr.Add((*page_data.thumbnail)[i]);
  }

  // Send data to registered indexers.
  base::win::ScopedVariant time(var_time, VT_DATE);
  base::win::ScopedBstr url(ASCIIToWide(page_data.url.spec()).c_str());
  base::win::ScopedBstr html(page_data.html);
  base::win::ScopedBstr title(page_data.title);
  // Don't send a NULL string through ASCIIToWide.
  base::win::ScopedBstr format(page_data.thumbnail_format ?
      ASCIIToWide(page_data.thumbnail_format).c_str() :
      NULL);
  base::win::ScopedVariant psa(thumbnail_arr.m_psa);
  for (size_t i = 0; i < indexers_.size(); ++i) {
    indexers_[i]->SendPageData(time, url, html, title, format, psa);
  }
}

}  // namespace history
