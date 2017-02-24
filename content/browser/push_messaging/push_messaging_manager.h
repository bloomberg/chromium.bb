// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MANAGER_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/push_messaging.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/push_messaging_status.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "url/gurl.h"

namespace content {

class PushMessagingService;
class ServiceWorkerContextWrapper;
struct PushSubscriptionOptions;

// Documented at definition.
extern const char kPushSenderIdServiceWorkerKey[];
extern const char kPushRegistrationIdServiceWorkerKey[];

class PushMessagingManager : public mojom::PushMessaging {
 public:
  PushMessagingManager(int render_process_id,
                       ServiceWorkerContextWrapper* service_worker_context);

  void BindRequest(mojom::PushMessagingRequest request);

  // mojom::PushMessaging impl, run on IO thread.
  void Subscribe(int32_t render_frame_id,
                 int64_t service_worker_registration_id,
                 const PushSubscriptionOptions& options,
                 const SubscribeCallback& callback) override;
  void Unsubscribe(int64_t service_worker_registration_id,
                   const UnsubscribeCallback& callback) override;
  void GetSubscription(int64_t service_worker_registration_id,
                       const GetSubscriptionCallback& callback) override;
  void GetPermissionStatus(
      int64_t service_worker_registration_id,
      bool user_visible,
      const GetPermissionStatusCallback& callback) override;

 private:
  struct RegisterData;
  class Core;

  friend class BrowserThread;
  friend class base::DeleteHelper<PushMessagingManager>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;

  ~PushMessagingManager() override;

  void DidCheckForExistingRegistration(
      const RegisterData& data,
      const std::vector<std::string>& push_registration_id,
      ServiceWorkerStatusCode service_worker_status);

  void DidGetEncryptionKeys(const RegisterData& data,
                            const std::string& push_registration_id,
                            bool success,
                            const std::vector<uint8_t>& p256dh,
                            const std::vector<uint8_t>& auth);

  void DidGetSenderIdFromStorage(const RegisterData& data,
                                 const std::vector<std::string>& sender_id,
                                 ServiceWorkerStatusCode service_worker_status);

  // Called via PostTask from UI thread.
  void PersistRegistrationOnIO(const RegisterData& data,
                               const std::string& push_registration_id,
                               const std::vector<uint8_t>& p256dh,
                               const std::vector<uint8_t>& auth);

  void DidPersistRegistrationOnIO(
      const RegisterData& data,
      const std::string& push_registration_id,
      const std::vector<uint8_t>& p256dh,
      const std::vector<uint8_t>& auth,
      ServiceWorkerStatusCode service_worker_status);

  // Called both from IO thread, and via PostTask from UI thread.
  void SendSubscriptionError(const RegisterData& data,
                             PushRegistrationStatus status);
  // Called both from IO thread, and via PostTask from UI thread.
  void SendSubscriptionSuccess(const RegisterData& data,
                               PushRegistrationStatus status,
                               const std::string& push_subscription_id,
                               const std::vector<uint8_t>& p256dh,
                               const std::vector<uint8_t>& auth);

  void UnsubscribeHavingGottenSenderId(
      const UnsubscribeCallback& callback,
      int64_t service_worker_registration_id,
      const GURL& requesting_origin,
      const std::vector<std::string>& sender_id,
      ServiceWorkerStatusCode service_worker_status);

  // Called both from IO thread, and via PostTask from UI thread.
  void DidUnregister(const UnsubscribeCallback& callback,
                     PushUnregistrationStatus unregistration_status);

  void DidGetSubscription(
      const GetSubscriptionCallback& callback,
      int64_t service_worker_registration_id,
      const std::vector<std::string>& push_subscription_id_and_sender_info,
      ServiceWorkerStatusCode service_worker_status);

  void DidGetSubscriptionKeys(const GetSubscriptionCallback& callback,
                              const GURL& endpoint,
                              const std::string& sender_info,
                              bool success,
                              const std::vector<uint8_t>& p256dh,
                              const std::vector<uint8_t>& auth);

  // Helper methods on either thread -------------------------------------------

  // Creates an endpoint for |subscription_id| with either the default protocol,
  // or the standardized Web Push Protocol, depending on |standard_protocol|.
  GURL CreateEndpoint(bool standard_protocol,
                      const std::string& subscription_id) const;

  // Inner core of this message filter which lives on the UI thread.
  std::unique_ptr<Core, BrowserThread::DeleteOnUIThread> ui_core_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // Whether the PushMessagingService was available when constructed.
  bool service_available_;

  GURL default_endpoint_;
  GURL web_push_protocol_endpoint_;

  mojo::BindingSet<mojom::PushMessaging> bindings_;

  base::WeakPtrFactory<PushMessagingManager> weak_factory_io_to_io_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MANAGER_H_
