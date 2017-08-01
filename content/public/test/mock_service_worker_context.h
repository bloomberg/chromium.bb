// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_MOCK_SERVICE_WORKER_CONTEXT_H_

#include "base/callback_forward.h"
#include "content/public/browser/service_worker_context.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

namespace content {

class ServiceWorkerContextObserver;

// Mock implementation of ServiceWorkerContext.
class MockServiceWorkerContext : public ServiceWorkerContext {
 public:
  MockServiceWorkerContext();
  virtual ~MockServiceWorkerContext();

  MOCK_METHOD1(AddObserver, void(ServiceWorkerContextObserver*));
  MOCK_METHOD1(RemoveObserver, void(ServiceWorkerContextObserver*));
  MOCK_METHOD3(RegisterServiceWorker,
               void(const ServiceWorkerContext::Scope&,
                    const GURL&,
                    const ServiceWorkerContext::ResultCallback&));
  MOCK_METHOD2(StartingExternalRequest, bool(int64_t, const std::string&));
  MOCK_METHOD2(FinishedExternalRequest, bool(int64_t, const std::string&));

  // StartActiveWorkerForPattern cannot be mocked because OnceClosure is not
  // copyable.
  void StartActiveWorkerForPattern(
      const GURL& pattern,
      ServiceWorkerContext::StartActiveWorkerCallback info_callback,
      base::OnceClosure failure_callback) override;

  MOCK_METHOD2(UnregisterServiceWorker,
               void(const ServiceWorkerContext::Scope&,
                    const ServiceWorkerContext::ResultCallback&));
  MOCK_METHOD1(GetAllOriginsInfo,
               void(const ServiceWorkerContext::GetUsageInfoCallback&));
  MOCK_METHOD2(DeleteForOrigin,
               void(const GURL&, const ServiceWorkerContext::ResultCallback&));
  MOCK_METHOD3(
      CheckHasServiceWorker,
      void(const GURL&,
           const GURL&,
           const ServiceWorkerContext::CheckHasServiceWorkerCallback&));
  MOCK_METHOD2(
      CountExternalRequestsForTest,
      void(const GURL&,
           const ServiceWorkerContext::CountExternalRequestsCallback&));
  MOCK_METHOD1(StopAllServiceWorkersForOrigin, void(const GURL&));
  MOCK_METHOD1(ClearAllServiceWorkersForTest, void(const base::Closure&));
  MOCK_METHOD2(StartServiceWorkerForNavigationHint,
               void(const GURL&,
                    const StartServiceWorkerForNavigationHintCallback&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockServiceWorkerContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_SERVICE_WORKER_CONTEXT_H_
