// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"

#include <string>

#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/signin/core/browser/profile_identity_provider.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/update_client/update_query_params.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "google_apis/gaia/identity_provider.h"

using extensions::ExtensionDownloader;
using extensions::ExtensionDownloaderDelegate;
using update_client::UpdateQueryParams;

scoped_ptr<ExtensionDownloader>
ChromeExtensionDownloaderFactory::CreateForRequestContext(
    net::URLRequestContextGetter* request_context,
    ExtensionDownloaderDelegate* delegate) {
  scoped_ptr<ExtensionDownloader> downloader(
      new ExtensionDownloader(delegate, request_context));
#if defined(GOOGLE_CHROME_BUILD)
  std::string brand;
  google_brand::GetBrand(&brand);
  if (!brand.empty() && !google_brand::IsOrganic(brand))
    downloader->set_brand_code(brand);
#endif  // defined(GOOGLE_CHROME_BUILD)
  downloader->set_manifest_query_params(
      UpdateQueryParams::Get(UpdateQueryParams::CRX));
  downloader->set_ping_enabled_domain("google.com");
  downloader->set_enable_extra_update_metrics(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
  return downloader.Pass();
}

scoped_ptr<ExtensionDownloader>
ChromeExtensionDownloaderFactory::CreateForProfile(
    Profile* profile,
    ExtensionDownloaderDelegate* delegate) {
  scoped_ptr<IdentityProvider> identity_provider(new ProfileIdentityProvider(
      SigninManagerFactory::GetForProfile(profile),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      LoginUIServiceFactory::GetShowLoginPopupCallbackForProfile(profile)));
  scoped_ptr<ExtensionDownloader> downloader =
      CreateForRequestContext(profile->GetRequestContext(), delegate);
  downloader->SetWebstoreIdentityProvider(identity_provider.Pass());
  return downloader.Pass();
}
