// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MESSAGE_FILTER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/push_messaging_status.h"
#include "url/gurl.h"

class GURL;

namespace content {

class PushMessagingService;
class ServiceWorkerContextWrapper;

class PushMessagingMessageFilter : public BrowserMessageFilter {
 public:
  PushMessagingMessageFilter(
      int render_process_id,
      ServiceWorkerContextWrapper* service_worker_context);

 private:
  struct RegisterData {
    RegisterData();
    RegisterData(const RegisterData& other) = default;
    bool FromDocument() const;
    int request_id;
    GURL requesting_origin;
    int64 service_worker_registration_id;
    // The following two members should only be read if FromDocument() is true.
    int render_frame_id;
    bool user_visible_only;
  };

  ~PushMessagingMessageFilter() override;

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // TODO(mvanouwerkerk): Delete once this is no longer called.
  void OnRegisterFromDocumentOld(int render_frame_id,
                                 int request_id,
                                 const std::string& sender_id,
                                 bool user_visible_only,
                                 int service_worker_provider_id);

  void OnRegisterFromDocument(int render_frame_id,
                              int request_id,
                              const std::string& sender_id,
                              bool user_visible_only,
                              int64 service_worker_registration_id);

  void OnRegisterFromWorker(int request_id,
                            int64 service_worker_registration_id);

  // TODO(mvanouwerkerk): Delete once the Push API flows through platform.
  // https://crbug.com/389194
  void OnPermissionStatusRequest(int render_frame_id,
                                 int service_worker_provider_id,
                                 int permission_callback_id);

  void OnGetPermissionStatus(int request_id,
                             int64 service_worker_registration_id);

  void CheckForExistingRegistration(const RegisterData& data,
                                    const std::string& sender_id);

  void DidCheckForExistingRegistration(
      const RegisterData& data,
      const std::string& sender_id,
      const std::string& push_registration_id,
      ServiceWorkerStatusCode service_worker_status);

  void RegisterOnUI(const RegisterData& data,
                    const std::string& sender_id);

  // TODO(mvanouwerkerk): Delete once the Push API flows through platform.
  // https://crbug.com/389194
  void DoPermissionStatusRequest(const GURL& requesting_origin,
                                 int render_frame_id,
                                 int callback_id);

  void GetPermissionStatusOnUI(const GURL& requesting_origin, int request_id);

  void DidRegister(const RegisterData& data,
                   const std::string& push_registration_id,
                   PushRegistrationStatus status);

  void PersistRegistrationOnIO(const RegisterData& data,
                               const GURL& push_endpoint,
                               const std::string& push_registration_id);

  void DidPersistRegistrationOnIO(
    const RegisterData& data,
    const GURL& push_endpoint,
    const std::string& push_registration_id,
    ServiceWorkerStatusCode service_worker_status);

  void SendRegisterError(const RegisterData& data,
                         PushRegistrationStatus status);
  void SendRegisterSuccess(const RegisterData& data,
                           PushRegistrationStatus status,
                           const GURL& push_endpoint,
                           const std::string& push_registration_id);
  void SendRegisterSuccessOnUI(const RegisterData& data,
                               PushRegistrationStatus status,
                               const std::string& push_registration_id);

  // Returns a push messaging service. The embedder owns the service, and is
  // responsible for ensuring that it outlives RenderProcessHost. It's valid to
  // return NULL. Must be called on the UI thread.
  PushMessagingService* service();

  int render_process_id_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // Owned by the content embedder's browsing context.
  PushMessagingService* service_;

  // Should only be used for asynchronous calls on the IO thread with external
  // dependencies that might outlive this class e.g. ServiceWorkerStorage.
  base::WeakPtrFactory<PushMessagingMessageFilter> weak_factory_io_to_io_;

  // TODO(johnme): Remove this, it seems unsafe since this class could be
  // destroyed on the IO thread while the callback runs on the UI thread.
  base::WeakPtrFactory<PushMessagingMessageFilter> weak_factory_ui_to_ui_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MESSAGE_FILTER_H_
