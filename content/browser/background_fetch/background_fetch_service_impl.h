// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SERVICE_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace url {
class Origin;
}

namespace content {

class BackgroundFetchContext;
struct BackgroundFetchOptions;
class ServiceWorkerContextWrapper;

class BackgroundFetchServiceImpl : public blink::mojom::BackgroundFetchService {
 public:
  BackgroundFetchServiceImpl(
      scoped_refptr<BackgroundFetchContext> background_fetch_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~BackgroundFetchServiceImpl() override;

  static void Create(
      scoped_refptr<BackgroundFetchContext> background_fetch_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      blink::mojom::BackgroundFetchServiceRequest request);

  // blink::mojom::BackgroundFetchService implementation.
  void Fetch(int64_t service_worker_registration_id,
             const url::Origin& origin,
             const std::string& tag,
             const BackgroundFetchOptions& options,
             const FetchCallback& callback) override;
  void UpdateUI(int64_t service_worker_registration_id,
                const url::Origin& origin,
                const std::string& tag,
                const std::string& title,
                const UpdateUICallback& callback) override;
  void Abort(int64_t service_worker_registration_id,
             const url::Origin& origin,
             const std::string& tag,
             const AbortCallback& callback) override;
  void GetRegistration(int64_t service_worker_registration_id,
                       const url::Origin& origin,
                       const std::string& tag,
                       const GetRegistrationCallback& callback) override;
  void GetTags(int64_t service_worker_registration_id,
               const url::Origin& origin,
               const GetTagsCallback& callback) override;

 private:
  scoped_refptr<BackgroundFetchContext> background_fetch_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SERVICE_IMPL_H_
