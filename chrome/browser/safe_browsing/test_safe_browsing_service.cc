// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"

#include "base/strings/string_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/test_database_manager.h"

namespace safe_browsing {

// TestSafeBrowsingService functions:
TestSafeBrowsingService::TestSafeBrowsingService()
    : test_shared_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  services_delegate_ = ServicesDelegate::CreateForTest(this, this);
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
}

TestSafeBrowsingService::~TestSafeBrowsingService() {}

V4ProtocolConfig TestSafeBrowsingService::GetV4ProtocolConfig() const {
  if (v4_protocol_config_)
    return *v4_protocol_config_;
  return SafeBrowsingService::GetV4ProtocolConfig();
}

void TestSafeBrowsingService::UseV4LocalDatabaseManager() {
  use_v4_local_db_manager_ = true;
}

void TestSafeBrowsingService::SetUseTestUrlLoaderFactory(
    bool use_test_url_loader_factory) {
  use_test_url_loader_factory_ = use_test_url_loader_factory;
}

network::TestURLLoaderFactory*
TestSafeBrowsingService::GetTestUrlLoaderFactory() {
  DCHECK(use_test_url_loader_factory_);
  return &test_url_loader_factory_;
}

std::unique_ptr<SafeBrowsingService::StateSubscription>
TestSafeBrowsingService::RegisterStateCallback(
    const base::Callback<void(void)>& callback) {
  // This override is required since TestSafeBrowsingService can be destroyed
  // before CertificateReportingService, which causes a crash due to the
  // leftover callback at destruction time.
  return nullptr;
}

std::string TestSafeBrowsingService::serilized_download_report() {
  return serialized_download_report_;
}

void TestSafeBrowsingService::ClearDownloadReport() {
  serialized_download_report_.clear();
}

void TestSafeBrowsingService::SetDatabaseManager(
    TestSafeBrowsingDatabaseManager* database_manager) {
  SetDatabaseManagerForTest(database_manager);
}

void TestSafeBrowsingService::SetUIManager(
    TestSafeBrowsingUIManager* ui_manager) {
  ui_manager->SetSafeBrowsingService(this);
  ui_manager_ = ui_manager;
}

SafeBrowsingUIManager* TestSafeBrowsingService::CreateUIManager() {
  if (ui_manager_)
    return ui_manager_.get();
  return SafeBrowsingService::CreateUIManager();
}

void TestSafeBrowsingService::SendSerializedDownloadReport(
    const std::string& report) {
  serialized_download_report_ = report;
}

const scoped_refptr<SafeBrowsingDatabaseManager>&
TestSafeBrowsingService::database_manager() const {
  if (test_database_manager_)
    return test_database_manager_;
  return SafeBrowsingService::database_manager();
}

void TestSafeBrowsingService::SetV4ProtocolConfig(
    V4ProtocolConfig* v4_protocol_config) {
  v4_protocol_config_.reset(v4_protocol_config);
}
// ServicesDelegate::ServicesCreator:
bool TestSafeBrowsingService::CanCreateDatabaseManager() {
  return !use_v4_local_db_manager_;
}
#if BUILDFLAG(FULL_SAFE_BROWSING)
bool TestSafeBrowsingService::CanCreateDownloadProtectionService() {
  return false;
}
#endif
bool TestSafeBrowsingService::CanCreateIncidentReportingService() {
  return true;
}
bool TestSafeBrowsingService::CanCreateResourceRequestDetector() {
  return false;
}

SafeBrowsingDatabaseManager* TestSafeBrowsingService::CreateDatabaseManager() {
  DCHECK(!use_v4_local_db_manager_);
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return new TestSafeBrowsingDatabaseManager();
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
DownloadProtectionService*
TestSafeBrowsingService::CreateDownloadProtectionService() {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif
IncidentReportingService*
TestSafeBrowsingService::CreateIncidentReportingService() {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return new IncidentReportingService(nullptr);
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
}
ResourceRequestDetector*
TestSafeBrowsingService::CreateResourceRequestDetector() {
  NOTIMPLEMENTED();
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestSafeBrowsingService::GetURLLoaderFactory() {
  if (use_test_url_loader_factory_)
    return test_shared_loader_factory_;
  return SafeBrowsingService::GetURLLoaderFactory();
}

// TestSafeBrowsingServiceFactory functions:
TestSafeBrowsingServiceFactory::TestSafeBrowsingServiceFactory()
    : test_safe_browsing_service_(nullptr), use_v4_local_db_manager_(false) {}

TestSafeBrowsingServiceFactory::~TestSafeBrowsingServiceFactory() {}

SafeBrowsingService*
TestSafeBrowsingServiceFactory::CreateSafeBrowsingService() {
  // Instantiate TestSafeBrowsingService.
  test_safe_browsing_service_ = new TestSafeBrowsingService();
  // Plug-in test member clases accordingly.
  if (use_v4_local_db_manager_)
    test_safe_browsing_service_->UseV4LocalDatabaseManager();
  if (test_ui_manager_)
    test_safe_browsing_service_->SetUIManager(test_ui_manager_.get());
  if (test_database_manager_) {
    test_safe_browsing_service_->SetDatabaseManager(
        test_database_manager_.get());
  }
  return test_safe_browsing_service_;
}

TestSafeBrowsingService*
TestSafeBrowsingServiceFactory::test_safe_browsing_service() {
  return test_safe_browsing_service_;
}

void TestSafeBrowsingServiceFactory::SetTestUIManager(
    TestSafeBrowsingUIManager* ui_manager) {
  test_ui_manager_ = ui_manager;
}

void TestSafeBrowsingServiceFactory::SetTestDatabaseManager(
    TestSafeBrowsingDatabaseManager* database_manager) {
  test_database_manager_ = database_manager;
}
void TestSafeBrowsingServiceFactory::UseV4LocalDatabaseManager() {
  use_v4_local_db_manager_ = true;
}

// TestSafeBrowsingUIManager functions:
TestSafeBrowsingUIManager::TestSafeBrowsingUIManager()
    : SafeBrowsingUIManager(nullptr) {}

TestSafeBrowsingUIManager::TestSafeBrowsingUIManager(
    const scoped_refptr<SafeBrowsingService>& service)
    : SafeBrowsingUIManager(service) {}

void TestSafeBrowsingUIManager::SetSafeBrowsingService(
    SafeBrowsingService* sb_service) {
  sb_service_ = sb_service;
}

void TestSafeBrowsingUIManager::SendSerializedThreatDetails(
    const std::string& serialized) {
  details_.push_back(serialized);
}

std::list<std::string>* TestSafeBrowsingUIManager::GetThreatDetails() {
  return &details_;
}

TestSafeBrowsingUIManager::~TestSafeBrowsingUIManager() {}
}  // namespace safe_browsing
