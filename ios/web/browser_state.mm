// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/browser_state.h"

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "ios/web/active_state_manager_impl.h"
#include "ios/web/public/certificate_policy_cache.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/webui/url_data_manager_ios_backend.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {
// Private key used for safe conversion of base::SupportsUserData to
// web::BrowserState in web::BrowserState::FromSupportsUserData.
const char kBrowserStateIdentifierKey[] = "BrowserStateIdentifierKey";

// Data key names.
const char kCertificatePolicyCacheKeyName[] = "cert_policy_cache";
const char kActiveStateManagerKeyName[] = "active_state_manager";

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
  DCHECK_CURRENTLY_ON(WebThread::UI);
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

// static
bool BrowserState::HasActiveStateManager(BrowserState* browser_state) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  return browser_state->GetUserData(kActiveStateManagerKeyName) != nullptr;
}

// static
ActiveStateManager* BrowserState::GetActiveStateManager(
    BrowserState* browser_state) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(browser_state);

  ActiveStateManagerImpl* active_state_manager =
      static_cast<ActiveStateManagerImpl*>(
          browser_state->GetUserData(kActiveStateManagerKeyName));
  if (!active_state_manager) {
    active_state_manager = new ActiveStateManagerImpl(browser_state);
    browser_state->SetUserData(kActiveStateManagerKeyName,
                               active_state_manager);
  }
  return active_state_manager;
}

BrowserState::BrowserState() : url_data_manager_ios_backend_(nullptr) {
  // (Refcounted)?BrowserStateKeyedServiceFactories needs to be able to convert
  // a base::SupportsUserData to a BrowserState. Moreover, since the factories
  // may be passed a content::BrowserContext instead of a BrowserState, attach
  // an empty object to this via a private key.
  SetUserData(kBrowserStateIdentifierKey, new SupportsUserData::Data);
}

BrowserState::~BrowserState() {
  // Delete the URLDataManagerIOSBackend instance on the IO thread if it has
  // been created. Note that while this check can theoretically race with a
  // call to |GetURLDataManagerIOSBackendOnIOThread()|, if any clients of this
  // BrowserState are still accessing it on the IO thread at this point,
  // they're going to have a bad time anyway.
  if (url_data_manager_ios_backend_) {
    bool posted = web::WebThread::DeleteSoon(web::WebThread::IO, FROM_HERE,
                                             url_data_manager_ios_backend_);
    if (!posted)
      delete url_data_manager_ios_backend_;
  }
}

URLDataManagerIOSBackend*
BrowserState::GetURLDataManagerIOSBackendOnIOThread() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (!url_data_manager_ios_backend_)
    url_data_manager_ios_backend_ = new URLDataManagerIOSBackend();
  return url_data_manager_ios_backend_;
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
