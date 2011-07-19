// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_publisher.h"

#include <atlsafe.h>
#include <objbase.h>
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

// Instantiates a IChromeHistoryIndexer COM object. Takes a COM class id
// in |name| and returns the object in |indexer|. Returns false if the
// operation fails.
bool CoCreateIndexerFromName(const wchar_t* name,
                             IChromeHistoryIndexer** indexer) {
  CLSID clsid;
  HRESULT hr = CLSIDFromString(const_cast<wchar_t*>(name), &clsid);
  if (FAILED(hr))
    return false;
  hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC,
                        __uuidof(IChromeHistoryIndexer),
                        reinterpret_cast<void**>(indexer));
  if (FAILED(hr))
    return false;
  return true;
}

// Instantiates the registered indexers from the registry |root| + |path| key
// and adds them to the |indexers| list.
void AddRegisteredIndexers(HKEY root, const wchar_t* path,
    std::vector< base::win::ScopedComPtr<IChromeHistoryIndexer> >* indexers) {
  IChromeHistoryIndexer* indexer;
  base::win::RegistryKeyIterator r_iter(root, path);
  while (r_iter.Valid()) {
    if (CoCreateIndexerFromName(r_iter.Name(), &indexer)) {
      indexers->push_back(
          base::win::ScopedComPtr<IChromeHistoryIndexer>(indexer));
      indexer->Release();
    }
    ++r_iter;
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
  CoInitialize(NULL);
}

HistoryPublisher::~HistoryPublisher() {
  CoUninitialize();
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

void HistoryPublisher::DeleteUserHistoryBetween(const base::Time& begin_time,
                                                const base::Time& end_time)
    const {
  base::win::ScopedVariant var_begin_time(TimeToUTCVariantTime(begin_time),
                                          VT_DATE);
  base::win::ScopedVariant var_end_time(TimeToUTCVariantTime(end_time),
                                        VT_DATE);
  for (size_t i = 0; i < indexers_.size(); ++i) {
    indexers_[i]->DeleteUserHistoryBetween(var_begin_time, var_end_time);
  }
}

}  // namespace history
