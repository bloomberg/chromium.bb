// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/base_safe_browsing_service.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(SAFE_BROWSING_DB_REMOTE)
#include "components/safe_browsing_db/remote_database_manager.h"
#endif

using content::BrowserThread;

namespace safe_browsing {

BaseSafeBrowsingService::BaseSafeBrowsingService() : enabled_(false) {}

BaseSafeBrowsingService::~BaseSafeBrowsingService() {
  // We should have already been shut down. If we're still enabled, then the
  // database isn't going to be closed properly, which could lead to corruption.
  DCHECK(!enabled_);
}

void BaseSafeBrowsingService::Initialize() {
  // TODO(ntfschr): initialize ui_manager_ once CreateUIManager is componentized
}

void BaseSafeBrowsingService::ShutDown() {}

scoped_refptr<net::URLRequestContextGetter>
BaseSafeBrowsingService::url_request_context() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return nullptr;
}

const scoped_refptr<SafeBrowsingDatabaseManager>&
BaseSafeBrowsingService::database_manager() const {
  return database_manager_;
}

void BaseSafeBrowsingService::OnResourceRequest(
    const net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

SafeBrowsingDatabaseManager* BaseSafeBrowsingService::CreateDatabaseManager() {
// For the LocalSafeBrowsingDatabaseManager, use
// chrome/browser/safe_browsing_service
#if defined(SAFE_BROWSING_DB_REMOTE)
  return new RemoteSafeBrowsingDatabaseManager();
#else
  return NULL;
#endif
}

std::unique_ptr<BaseSafeBrowsingService::StateSubscription>
BaseSafeBrowsingService::RegisterStateCallback(
    const base::Callback<void(void)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return state_callback_list_.Add(callback);
}

void BaseSafeBrowsingService::ProcessResourceRequest(
    const ResourceRequestInfo& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

}  // namespace safe_browsing
