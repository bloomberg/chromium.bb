// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/histograms.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"

namespace protector {

const char kProtectorHistogramDefaultSearchProvider[] =
    "Protector.DefaultSearchProvider";

const char kProtectorHistogramNewSearchProvider[] =
    "Protector.SearchProvider.New";
const char kProtectorHistogramSearchProviderApplied[] =
    "Protector.SearchProvider.Applied";
const char kProtectorHistogramSearchProviderDiscarded[] =
    "Protector.SearchProvider.Discarded";
const char kProtectorHistogramSearchProviderFallback[] =
    "Protector.SearchProvider.Fallback";
const char kProtectorHistogramSearchProviderMissing[] =
    "Protector.SearchProvider.Missing";
const char kProtectorHistogramSearchProviderRestored[] =
    "Protector.SearchProvider.Restored";
const char kProtectorHistogramSearchProviderTimeout[] =
    "Protector.SearchProvider.Timeout";

const int kProtectorMaxSearchProviderID = SEARCH_ENGINE_MAX;

int GetSearchProviderHistogramID(const TemplateURL* t_url) {
  if (t_url && t_url->url()) {
    scoped_ptr<TemplateURL> prepopulated_url(
        TemplateURLPrepopulateData::FindPrepopulatedEngine(
            t_url->url()->url()));
    if (prepopulated_url.get())
      return static_cast<int>(prepopulated_url->search_engine_type());
  }
  // If |t_url| is NULL or not among the prepopulated providers, return
  // SEARCH_ENGINE_OTHER as well.
  return SEARCH_ENGINE_OTHER;
}

}  // namespace protector
