// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"

#include <string>
#include <utility>


#include "base/command_line.h"
#include "chrome/browser/google/google_brand.h"
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

namespace {
const char kTestRequestParam[] = "extension-updater-test-request";
}  // namespace

std::unique_ptr<ExtensionDownloader>
ChromeExtensionDownloaderFactory::CreateForRequestContext(
    net::URLRequestContextGetter* request_context,
    ExtensionDownloaderDelegate* delegate) {
  std::unique_ptr<ExtensionDownloader> downloader(
      new ExtensionDownloader(delegate, request_context));
#if defined(GOOGLE_CHROME_BUILD)
  std::string brand;
  google_brand::GetBrand(&brand);
  if (!brand.empty() && !google_brand::IsOrganic(brand))
    downloader->set_brand_code(brand);
#endif  // defined(GOOGLE_CHROME_BUILD)
  std::string manifest_query_params =
      UpdateQueryParams::Get(UpdateQueryParams::CRX);
  base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTestRequestParam)) {
    manifest_query_params += "&testrequest=1";
  }
  downloader->set_manifest_query_params(manifest_query_params);
  downloader->set_ping_enabled_domain("google.com");
  return downloader;
}

std::unique_ptr<ExtensionDownloader>
ChromeExtensionDownloaderFactory::CreateForProfile(
    Profile* profile,
    ExtensionDownloaderDelegate* delegate) {
  std::unique_ptr<IdentityProvider> identity_provider(
      new ProfileIdentityProvider(
          SigninManagerFactory::GetForProfile(profile),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          LoginUIServiceFactory::GetShowLoginPopupCallbackForProfile(profile)));
  std::unique_ptr<ExtensionDownloader> downloader =
      CreateForRequestContext(profile->GetRequestContext(), delegate);
  downloader->SetWebstoreIdentityProvider(std::move(identity_provider));
  return downloader;
}
