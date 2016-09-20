// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/ios_browsing_data_counter_factory.h"

// static
std::unique_ptr<BrowsingDataCounterWrapper>
BrowsingDataCounterWrapper::CreateCounterWrapper(
    const char* pref_name,
    ios::ChromeBrowserState* browser_state,
    PrefService* pref_service,
    const UpdateUICallback& update_ui_callback) {
  std::unique_ptr<browsing_data::BrowsingDataCounter> counter =
      IOSBrowsingDataCounterFactory::GetForBrowserStateAndPref(browser_state,
                                                               pref_name);
  if (!counter)
    return nullptr;
  return base::WrapUnique<BrowsingDataCounterWrapper>(
      new BrowsingDataCounterWrapper(std::move(counter), pref_service,
                                     update_ui_callback));
}

BrowsingDataCounterWrapper::~BrowsingDataCounterWrapper() {}

void BrowsingDataCounterWrapper::RestartCounter() {
  counter_->Restart();
}

BrowsingDataCounterWrapper::BrowsingDataCounterWrapper(
    std::unique_ptr<browsing_data::BrowsingDataCounter> counter,
    PrefService* pref_service,
    const UpdateUICallback& update_ui_callback)
    : counter_(std::move(counter)), update_ui_callback_(update_ui_callback) {
  counter_->Init(pref_service,
                 base::Bind(&BrowsingDataCounterWrapper::UpdateWithResult,
                            base::Unretained(this)));
}

void BrowsingDataCounterWrapper::UpdateWithResult(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  update_ui_callback_.Run(*result.get());
}
