// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/default_settings_fetcher.h"

#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

#if defined(GOOGLE_CHROME_BUILD)
constexpr char kOmahaUrl[] = "https://tools.google.com/service/update2";
#endif  // defined(GOOGLE_CHROME_BUILD)

}  // namespace

// static
void DefaultSettingsFetcher::FetchDefaultSettings(SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |settings_fetcher| will delete itself when default settings have been
  // fetched after the call to |Start()|.
  DefaultSettingsFetcher* settings_fetcher =
      new DefaultSettingsFetcher(std::move(callback));
  settings_fetcher->Start();
}

// static
void DefaultSettingsFetcher::FetchDefaultSettingsForTesting(
    SettingsCallback callback,
    std::unique_ptr<BrandcodedDefaultSettings> fetched_settings) {
  DefaultSettingsFetcher* settings_fetcher =
      new DefaultSettingsFetcher(std::move(callback));
  // Inject the given settings directly by passing them to
  // |PostCallbackAndDeleteSelf()|.
  settings_fetcher->PostCallbackAndDeleteSelf(std::move(fetched_settings));
}

DefaultSettingsFetcher::DefaultSettingsFetcher(SettingsCallback callback)
    : callback_(std::move(callback)) {}

DefaultSettingsFetcher::~DefaultSettingsFetcher() {}

void DefaultSettingsFetcher::Start() {
  DCHECK(!config_fetcher_);

#if defined(GOOGLE_CHROME_BUILD)
  std::string brandcode;
  if (google_brand::GetBrand(&brandcode) && !brandcode.empty()) {
    config_fetcher_.reset(new BrandcodeConfigFetcher(
        base::Bind(&DefaultSettingsFetcher::OnSettingsFetched,
                   base::Unretained(this)),
        GURL(kOmahaUrl), brandcode));
    return;
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  // For non Google Chrome builds and cases with an empty |brandcode|, we create
  // a default-constructed |BrandcodedDefaultSettings| object and post the
  // callback immediately.
  PostCallbackAndDeleteSelf(base::MakeUnique<BrandcodedDefaultSettings>());
}

void DefaultSettingsFetcher::OnSettingsFetched() {
  DCHECK(config_fetcher_);
  DCHECK(!config_fetcher_->IsActive());

  PostCallbackAndDeleteSelf(config_fetcher_->GetSettings());
}

void DefaultSettingsFetcher::PostCallbackAndDeleteSelf(
    std::unique_ptr<BrandcodedDefaultSettings> default_settings) {
  // Use default settings if fetching of BrandcodedDefaultSettings failed.
  if (!default_settings)
    default_settings.reset(new BrandcodedDefaultSettings());

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(std::move(callback_), base::Passed(&default_settings)));
  delete this;
}

}  // namespace safe_browsing
