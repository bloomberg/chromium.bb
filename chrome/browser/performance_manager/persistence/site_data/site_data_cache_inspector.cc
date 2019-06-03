// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_inspector.h"

#include "base/macros.h"
#include "content/public/browser/browser_context.h"

namespace performance_manager {

namespace {

const void* const kSiteDataCacheInspectorUserKey =
    &kSiteDataCacheInspectorUserKey;

class SiteDataUserData : public base::SupportsUserData::Data {
 public:
  explicit SiteDataUserData(SiteDataCacheInspector* inspector)
      : inspector_(inspector) {}

  SiteDataCacheInspector* inspector() const { return inspector_; }

 private:
  SiteDataCacheInspector* inspector_;
};

}  // namespace

// static
SiteDataCacheInspector* SiteDataCacheInspector::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  SiteDataUserData* data = static_cast<SiteDataUserData*>(
      browser_context->GetUserData(kSiteDataCacheInspectorUserKey));

  if (!data)
    return nullptr;

  return data->inspector();
}

// static
void SiteDataCacheInspector::SetForBrowserContext(
    SiteDataCacheInspector* inspector,
    content::BrowserContext* browser_context) {
  if (inspector) {
    DCHECK_EQ(nullptr, GetForBrowserContext(browser_context));

    browser_context->SetUserData(kSiteDataCacheInspectorUserKey,
                                 std::make_unique<SiteDataUserData>(inspector));
  } else {
    DCHECK_NE(nullptr, GetForBrowserContext(browser_context));
    browser_context->RemoveUserData(kSiteDataCacheInspectorUserKey);
  }
}

}  // namespace performance_manager
