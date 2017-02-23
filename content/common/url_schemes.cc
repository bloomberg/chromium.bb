// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/url_schemes.h"

#include <string.h>

#include <algorithm>

#include "base/strings/string_util.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "url/url_util.h"

namespace content {
namespace {

// These lists are lazily initialized below and are leaked on shutdown to
// prevent any destructors from being called that will slow us down or cause
// problems.
std::vector<std::string>* savable_schemes = nullptr;
// Note we store GURLs here instead of strings to deal with canonicalization.
std::vector<GURL>* secure_origins = nullptr;
std::vector<std::string>* service_worker_schemes = nullptr;

const char* const kDefaultSavableSchemes[] = {
  url::kHttpScheme,
  url::kHttpsScheme,
  url::kFileScheme,
  url::kFileSystemScheme,
  url::kFtpScheme,
  kChromeDevToolsScheme,
  kChromeUIScheme,
  url::kDataScheme
};

}  // namespace

void RegisterContentSchemes(bool lock_schemes) {
  ContentClient::Schemes schemes;
  GetContentClient()->AddAdditionalSchemes(&schemes);

  url::AddStandardScheme(kChromeDevToolsScheme, url::SCHEME_WITHOUT_PORT);
  url::AddStandardScheme(kChromeUIScheme, url::SCHEME_WITHOUT_PORT);
  url::AddStandardScheme(kGuestScheme, url::SCHEME_WITHOUT_PORT);

  for (auto& scheme : schemes.standard_schemes)
    url::AddStandardScheme(scheme.c_str(), url::SCHEME_WITHOUT_PORT);

  for (auto& scheme : schemes.referrer_schemes)
    url::AddReferrerScheme(scheme.c_str(), url::SCHEME_WITHOUT_PORT);

  schemes.secure_schemes.push_back(kChromeUIScheme);
  for (auto& scheme : schemes.secure_schemes)
    url::AddSecureScheme(scheme.c_str());

  for (auto& scheme : schemes.local_schemes)
    url::AddLocalScheme(scheme.c_str());

  for (auto& scheme : schemes.no_access_schemes)
    url::AddNoAccessScheme(scheme.c_str());

  schemes.cors_enabled_schemes.push_back(kChromeUIScheme);
  for (auto& scheme : schemes.cors_enabled_schemes)
    url::AddCORSEnabledScheme(scheme.c_str());

  for (auto& scheme : schemes.csp_bypassing_schemes)
    url::AddCSPBypassingScheme(scheme.c_str());

  // Prevent future modification of the scheme lists. This is to prevent
  // accidental creation of data races in the program. Add*Scheme aren't
  // threadsafe so must be called when GURL isn't used on any other thread. This
  // is really easy to mess up, so we say that all calls to Add*Scheme in Chrome
  // must be inside this function.
  if (lock_schemes)
    url::LockSchemeRegistries();

  // Combine the default savable schemes with the additional ones given.
  delete savable_schemes;
  savable_schemes = new std::vector<std::string>;
  for (auto* default_scheme : kDefaultSavableSchemes)
    savable_schemes->push_back(default_scheme);
  savable_schemes->insert(savable_schemes->end(),
                          schemes.savable_schemes.begin(),
                          schemes.savable_schemes.end());

  delete service_worker_schemes;
  service_worker_schemes = new std::vector<std::string>;
  *service_worker_schemes = std::move(schemes.service_worker_schemes);

  delete secure_origins;
  secure_origins = new std::vector<GURL>;
  *secure_origins = std::move(schemes.secure_origins);
}

const std::vector<std::string>& GetSavableSchemes() {
  return *savable_schemes;
}

const std::vector<GURL>& GetSecureOrigins() {
  return *secure_origins;
}

const std::vector<std::string>& GetServiceWorkerSchemes() {
  return *service_worker_schemes;
}

}  // namespace content
