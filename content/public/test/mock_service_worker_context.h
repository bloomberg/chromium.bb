// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_MOCK_SERVICE_WORKER_CONTEXT_H_

#include <string>

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

  // Some functions cannot be mocked because they use OnceCallback which is not
  // copyable.
  MOCK_METHOD1(AddObserver, void(ServiceWorkerContextObserver*));
  MOCK_METHOD1(RemoveObserver, void(ServiceWorkerContextObserver*));
  void RegisterServiceWorker(const GURL& pattern,
                             const GURL& script_url,
                             ResultCallback callback) override;
  void UnregisterServiceWorker(const GURL& pattern,
                               ResultCallback callback) override;
  MOCK_METHOD2(StartingExternalRequest, bool(int64_t, const std::string&));
  MOCK_METHOD2(FinishedExternalRequest, bool(int64_t, const std::string&));
  void CountExternalRequestsForTest(
      const GURL& url,
      CountExternalRequestsCallback callback) override;
  void GetAllOriginsInfo(GetUsageInfoCallback callback) override;
  void DeleteForOrigin(const GURL& origin, ResultCallback callback) override;
  void CheckHasServiceWorker(const GURL& url,
                             const GURL& other_url,
                             CheckHasServiceWorkerCallback callback) override;
  void ClearAllServiceWorkersForTest(base::OnceClosure) override;
  void StartActiveWorkerForPattern(
      const GURL& pattern,
      ServiceWorkerContext::StartActiveWorkerCallback info_callback,
      base::OnceClosure failure_callback) override;
  MOCK_METHOD2(StartServiceWorkerForNavigationHint,
               void(const GURL&,
                    const StartServiceWorkerForNavigationHintCallback&));
  MOCK_METHOD1(StopAllServiceWorkersForOrigin, void(const GURL&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockServiceWorkerContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_SERVICE_WORKER_CONTEXT_H_
