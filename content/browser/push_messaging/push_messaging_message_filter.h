// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MESSAGE_FILTER_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/push_messaging_status.h"
#include "url/gurl.h"

class GURL;

namespace content {

class PushMessagingService;
class ServiceWorkerContextWrapper;

extern const char kPushSenderIdServiceWorkerKey[];
extern const char kPushRegistrationIdServiceWorkerKey[];

class PushMessagingMessageFilter : public BrowserMessageFilter {
 public:
  PushMessagingMessageFilter(
      int render_process_id,
      ServiceWorkerContextWrapper* service_worker_context);

 private:
  struct RegisterData;
  class Core;

  friend class BrowserThread;
  friend class base::DeleteHelper<PushMessagingMessageFilter>;

  ~PushMessagingMessageFilter() override;

  // BrowserMessageFilter implementation.
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Subscribe methods on IO thread --------------------------------------------

  void OnSubscribeFromDocument(int render_frame_id,
                               int request_id,
                               const std::string& sender_id,
                               bool user_visible,
                               int64_t service_worker_registration_id);

  void OnSubscribeFromWorker(int request_id,
                             int64_t service_worker_registration_id,
                             bool user_visible);

  void DidPersistSenderId(const RegisterData& data,
                          const std::string& sender_id,
                          ServiceWorkerStatusCode service_worker_status);

  // sender_id is ignored if data.FromDocument() is false.
  void CheckForExistingRegistration(const RegisterData& data,
                                    const std::string& sender_id);

  // sender_id is ignored if data.FromDocument() is false.
  void DidCheckForExistingRegistration(
      const RegisterData& data,
      const std::string& sender_id,
      const std::string& push_registration_id,
      ServiceWorkerStatusCode service_worker_status);

  void DidGetEncryptionKeys(const RegisterData& data,
                            const std::string& push_registration_id,
                            bool success,
                            const std::vector<uint8_t>& p256dh,
                            const std::vector<uint8_t>& auth);

  void DidGetSenderIdFromStorage(const RegisterData& data,
                                 const std::string& sender_id,
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

  // Unsubscribe methods on IO thread ------------------------------------------

  void OnUnsubscribe(int request_id, int64_t service_worker_registration_id);

  void UnsubscribeHavingGottenPushSubscriptionId(
      int request_id,
      int64_t service_worker_registration_id,
      const GURL& requesting_origin,
      const std::string& push_subscription_id,
      ServiceWorkerStatusCode service_worker_status);

  void UnsubscribeHavingGottenSenderId(
      int request_id,
      int64_t service_worker_registration_id,
      const GURL& requesting_origin,
      const std::string& sender_id,
      ServiceWorkerStatusCode service_worker_status);

  // Called via PostTask from UI thread.
  void ClearRegistrationData(int request_id,
                             int64_t service_worker_registration_id,
                             PushUnregistrationStatus unregistration_status);

  void DidClearRegistrationData(int request_id,
                                PushUnregistrationStatus unregistration_status,
                                ServiceWorkerStatusCode service_worker_status);

  // Called both from IO thread, and via PostTask from UI thread.
  void DidUnregister(int request_id,
                     PushUnregistrationStatus unregistration_status);

  // GetSubscription methods on IO thread --------------------------------------

  void OnGetSubscription(int request_id,
                         int64_t service_worker_registration_id);

  void DidGetSubscription(int request_id,
                          int64_t service_worker_registration_id,
                          const std::string& push_subscription_id,
                          ServiceWorkerStatusCode status);

  void DidGetSubscriptionKeys(int request_id,
                              const GURL& endpoint,
                              bool success,
                              const std::vector<uint8_t>& p256dh,
                              const std::vector<uint8_t>& auth);

  // GetPermission methods on IO thread ----------------------------------------

  void OnGetPermissionStatus(int request_id,
                             int64_t service_worker_registration_id,
                             bool user_visible);

  // Helper methods on IO thread -----------------------------------------------

  // Called via PostTask from UI thread.
  void SendIPC(scoped_ptr<IPC::Message> message);

  // Inner core of this message filter which lives on the UI thread.
  scoped_ptr<Core, BrowserThread::DeleteOnUIThread> ui_core_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // Empty if no PushMessagingService was available when constructed.
  GURL push_endpoint_base_;

  base::WeakPtrFactory<PushMessagingMessageFilter> weak_factory_io_to_io_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MESSAGE_FILTER_H_
