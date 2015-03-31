// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/browser_state.h"

#include "base/memory/ref_counted.h"
#include "ios/web/public/certificate_policy_cache.h"
#include "ios/web/public/web_thread.h"

namespace web {
namespace {
// Private key used for safe conversion of base::SupportsUserData to
// web::BrowserState in web::BrowserState::FromSupportsUserData.
const char kBrowserStateIdentifierKey[] = "BrowserStateIdentifierKey";
// Data key names.
const char kCertificatePolicyCacheKeyName[] = "cert_policy_cache";

// Wraps a CertificatePolicyCache as a SupportsUserData::Data; this is necessary
// since reference counted objects can't be user data.
struct CertificatePolicyCacheHandle : public base::SupportsUserData::Data {
  explicit CertificatePolicyCacheHandle(CertificatePolicyCache* cache)
      : policy_cache(cache) {}

  scoped_refptr<CertificatePolicyCache> policy_cache;
};
}

// static
scoped_refptr<CertificatePolicyCache> BrowserState::GetCertificatePolicyCache(
    BrowserState* browser_state) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  if (!browser_state->GetUserData(kCertificatePolicyCacheKeyName)) {
    CertificatePolicyCacheHandle* cert_cache_service_handle =
        new CertificatePolicyCacheHandle(new CertificatePolicyCache());

    browser_state->SetUserData(kCertificatePolicyCacheKeyName,
                               cert_cache_service_handle);
  }

  CertificatePolicyCacheHandle* handle =
      static_cast<CertificatePolicyCacheHandle*>(
          browser_state->GetUserData(kCertificatePolicyCacheKeyName));
  return handle->policy_cache;
}

BrowserState::BrowserState() {
  // (Refcounted)?BrowserStateKeyedServiceFactories needs to be able to convert
  // a base::SupportsUserData to a BrowserState. Moreover, since the factories
  // may be passed a content::BrowserContext instead of a BrowserState, attach
  // an empty object to this via a private key.
  SetUserData(kBrowserStateIdentifierKey, new SupportsUserData::Data);
}

BrowserState::~BrowserState() {
}

// static
BrowserState* BrowserState::FromSupportsUserData(
    base::SupportsUserData* supports_user_data) {
  if (!supports_user_data ||
      !supports_user_data->GetUserData(kBrowserStateIdentifierKey)) {
    return nullptr;
  }
  return static_cast<BrowserState*>(supports_user_data);
}
}  // namespace web
