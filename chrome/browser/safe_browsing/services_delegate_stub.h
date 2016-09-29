// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_STUB_H_
#define CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_STUB_H_

#include "base/macros.h"
#include "chrome/browser/safe_browsing/services_delegate.h"

namespace safe_browsing {

// Dummy ServicesDelegate implementation. Create via ServicesDelegate::Create().
class ServicesDelegateStub : public ServicesDelegate {
 public:
  ServicesDelegateStub();
  ~ServicesDelegateStub() override;

 private:
  // ServicesDelegate:
  void Initialize() override;
  void InitializeCsdService(
      net::URLRequestContextGetter* context_getter) override;
  void ShutdownServices() override;
  void RefreshState(bool enable) override;
  void ProcessResourceRequest(const ResourceRequestInfo* request) override;
  std::unique_ptr<TrackedPreferenceValidationDelegate>
      CreatePreferenceValidationDelegate(Profile* profile) override;
  void RegisterDelayedAnalysisCallback(
      const DelayedAnalysisCallback& callback) override;
  void RegisterExtendedReportingOnlyDelayedAnalysisCallback(
      const DelayedAnalysisCallback& callback) override;
  void AddDownloadManager(content::DownloadManager* download_manager) override;
  ClientSideDetectionService* GetCsdService() override;
  DownloadProtectionService* GetDownloadService() override;

  void StartOnIOThread(
    net::URLRequestContextGetter* url_request_context_getter,
    const V4ProtocolConfig& v4_config) override;
  void StopOnIOThread(bool shutdown) override;

  DISALLOW_COPY_AND_ASSIGN(ServicesDelegateStub);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_STUB_H_
