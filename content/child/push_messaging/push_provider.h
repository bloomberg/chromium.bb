// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PUSH_MESSAGING_PUSH_PROVIDER_H_
#define CONTENT_CHILD_PUSH_MESSAGING_PUSH_PROVIDER_H_

#include <string>

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "content/child/push_messaging/push_dispatcher.h"
#include "content/child/worker_task_runner.h"
#include "content/public/common/push_messaging_status.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushError.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushProvider.h"

class GURL;

namespace blink {
struct WebPushSubscriptionOptions;
}

namespace content {

class ThreadSafeSender;

class PushProvider : public blink::WebPushProvider,
                     public WorkerTaskRunner::Observer {
 public:
  ~PushProvider() override;

  // The |thread_safe_sender| and |push_dispatcher| are used if calling this
  // leads to construction.
  static PushProvider* ThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender,
      PushDispatcher* push_dispatcher);

  // WorkerTaskRunner::Observer implementation.
  void OnWorkerRunLoopStopped() override;

  // blink::WebPushProvider implementation.
  virtual void subscribe(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      const blink::WebPushSubscriptionOptions& options,
      blink::WebPushSubscriptionCallbacks* callbacks);
  virtual void unsubscribe(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebPushUnsubscribeCallbacks* callbacks);
  virtual void getSubscription(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebPushSubscriptionCallbacks* callbacks);
  virtual void getPermissionStatus(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      const blink::WebPushSubscriptionOptions& options,
      blink::WebPushPermissionStatusCallbacks* callbacks);

  // Called by the PushDispatcher.
  bool OnMessageReceived(const IPC::Message& message);

 private:
  PushProvider(ThreadSafeSender* thread_safe_sender,
               PushDispatcher* push_dispatcher);

  // IPC message handlers.
  void OnRegisterFromWorkerSuccess(int request_id,
                                   const GURL& endpoint,
                                   const std::string& registration_id);
  void OnRegisterFromWorkerError(int request_id, PushRegistrationStatus status);
  void OnUnregisterSuccess(int request_id, bool did_unregister);
  void OnUnregisterError(int request_id,
                         blink::WebPushError::ErrorType error_type,
                         const std::string& error_message);
  void OnGetRegistrationSuccess(int request_id,
                                const GURL& endpoint,
                                const std::string& registration_id);
  void OnGetRegistrationError(int request_id, PushGetRegistrationStatus status);
  void OnGetPermissionStatusSuccess(int request_id,
                                    blink::WebPushPermissionStatus status);
  void OnGetPermissionStatusError(int request_id);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<PushDispatcher> push_dispatcher_;

  // Stores the subscription callbacks with their request ids. This class owns
  // the callbacks.
  IDMap<blink::WebPushSubscriptionCallbacks, IDMapOwnPointer>
      subscription_callbacks_;

  // Stores the permission status callbacks with their request ids. This class
  // owns the callbacks.
  IDMap<blink::WebPushPermissionStatusCallbacks, IDMapOwnPointer>
      permission_status_callbacks_;

  // Stores the unsubscription callbacks with their request ids. This class owns
  // the callbacks.
  IDMap<blink::WebPushUnsubscribeCallbacks, IDMapOwnPointer>
      unsubscribe_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PushProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_PUSH_MESSAGING_PUSH_PROVIDER_H_
