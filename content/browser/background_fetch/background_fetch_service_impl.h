// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SERVICE_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace url {
class Origin;
}

namespace content {

class BackgroundFetchContext;
struct BackgroundFetchOptions;
struct ServiceWorkerFetchRequest;

class CONTENT_EXPORT BackgroundFetchServiceImpl
    : public blink::mojom::BackgroundFetchService {
 public:
  BackgroundFetchServiceImpl(
      int render_process_id,
      scoped_refptr<BackgroundFetchContext> background_fetch_context);
  ~BackgroundFetchServiceImpl() override;

  static void Create(
      int render_process_id,
      scoped_refptr<BackgroundFetchContext> background_fetch_context,
      blink::mojom::BackgroundFetchServiceRequest request);

  // blink::mojom::BackgroundFetchService implementation.
  void Fetch(int64_t service_worker_registration_id,
             const url::Origin& origin,
             const std::string& developer_id,
             const std::vector<ServiceWorkerFetchRequest>& requests,
             const BackgroundFetchOptions& options,
             FetchCallback callback) override;
  void UpdateUI(const std::string& unique_id,
                const std::string& title,
                UpdateUICallback callback) override;
  void Abort(const std::string& unique_id, AbortCallback callback) override;
  void GetRegistration(int64_t service_worker_registration_id,
                       const url::Origin& origin,
                       const std::string& developer_id,
                       GetRegistrationCallback callback) override;
  void GetDeveloperIds(int64_t service_worker_registration_id,
                       const url::Origin& origin,
                       GetDeveloperIdsCallback callback) override;
  void AddRegistrationObserver(
      const std::string& unique_id,
      blink::mojom::BackgroundFetchRegistrationObserverPtr observer) override;

 private:
  // Validates and returns whether the |developer_id|, |unique_id|, |requests|
  // and |title| respectively have valid values. The renderer will be flagged
  // for having sent a bad message if the values are invalid.
  bool ValidateDeveloperId(const std::string& developer_id) WARN_UNUSED_RESULT;
  bool ValidateUniqueId(const std::string& unique_id) WARN_UNUSED_RESULT;
  bool ValidateRequests(const std::vector<ServiceWorkerFetchRequest>& requests)
      WARN_UNUSED_RESULT;
  bool ValidateTitle(const std::string& title) WARN_UNUSED_RESULT;

  // Id of the renderer process that this service has been created for.
  int render_process_id_;

  // The Background Fetch context on which operations will be dispatched.
  scoped_refptr<BackgroundFetchContext> background_fetch_context_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SERVICE_IMPL_H_
