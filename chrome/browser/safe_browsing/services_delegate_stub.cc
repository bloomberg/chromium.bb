// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/services_delegate_stub.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/safe_browsing_db/v4_local_database_manager.h"

namespace safe_browsing {

// static
std::unique_ptr<ServicesDelegate> ServicesDelegate::Create(
    SafeBrowsingService* safe_browsing_service) {
  return base::WrapUnique(new ServicesDelegateStub);
}

// static
std::unique_ptr<ServicesDelegate> ServicesDelegate::CreateForTest(
    SafeBrowsingService* safe_browsing_service,
    ServicesDelegate::ServicesCreator* services_creator) {
  NOTREACHED();
  return base::WrapUnique(new ServicesDelegateStub);
}

ServicesDelegateStub::ServicesDelegateStub() {}

ServicesDelegateStub::~ServicesDelegateStub() {}

void ServicesDelegateStub::InitializeCsdService(
    net::URLRequestContextGetter* context_getter) {}

const scoped_refptr<SafeBrowsingDatabaseManager>&
ServicesDelegateStub::v4_local_database_manager() const {
  return v4_local_database_manager_;
}

void ServicesDelegateStub::Initialize(bool v4_enabled) {}

void ServicesDelegateStub::ShutdownServices() {}

void ServicesDelegateStub::RefreshState(bool enable) {}

void ServicesDelegateStub::ProcessResourceRequest(
    const ResourceRequestInfo* request) {}

std::unique_ptr<TrackedPreferenceValidationDelegate>
ServicesDelegateStub::CreatePreferenceValidationDelegate(Profile* profile) {
  return std::unique_ptr<TrackedPreferenceValidationDelegate>();
}

void ServicesDelegateStub::RegisterDelayedAnalysisCallback(
    const DelayedAnalysisCallback& callback) {}

void ServicesDelegateStub::AddDownloadManager(
    content::DownloadManager* download_manager) {}

ClientSideDetectionService* ServicesDelegateStub::GetCsdService() {
  return nullptr;
}

DownloadProtectionService* ServicesDelegateStub::GetDownloadService() {
  return nullptr;
}

void ServicesDelegateStub::StartOnIOThread(
    net::URLRequestContextGetter* url_request_context_getter,
    const V4ProtocolConfig& v4_config) {}

void ServicesDelegateStub::StopOnIOThread(bool shutdown) {}

}  // namespace safe_browsing
