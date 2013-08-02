// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_XMPP_LISTENER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_XMPP_LISTENER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "jingle/notifier/listener/push_client_observer.h"

namespace base {

class SingleThreadTaskRunner;

}  // namespace base

namespace net {

class URLRequestContextGetter;

}  // namespace net

namespace notifier {

class PushClient;

}  // namespace notifier

class CloudPrintXmppListener: public notifier::PushClientObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Invoked when XMPP connection was established.
    virtual void OnXmppConnected() = 0;

    // Invoked when server rejected our credentials.
    virtual void OnXmppAuthError() = 0;

    // Invoked when server is unavailable.
    virtual void OnXmppNetworkError() = 0;

    // Invoked when new printjob was received.
    virtual void OnXmppNewPrintJob(const std::string& device_id) = 0;

    // Invoked when local settings was updated.
    virtual void OnXmppNewLocalSettings(const std::string& device_id) = 0;

    // Invoked when printer was deleted from server.
    virtual void OnXmppDeleteNotification(const std::string& device_id) = 0;
  };

  CloudPrintXmppListener(
      const std::string& robot_email,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      Delegate* delegate);

  virtual ~CloudPrintXmppListener();

  // Connects to the server.
  void Connect(const std::string& access_token);

 private:
  // notifier::PushClientObserver methods:
  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      notifier::NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const notifier::Notification& notification) OVERRIDE;
  virtual void OnPingResponse() OVERRIDE;

  // Is used for reconnection when number of retries is now exhausted.
  void ReconnectInternal();

  // Credentials:
  std::string robot_email_;
  std::string access_token_;

  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Internal listener.
  scoped_ptr<notifier::PushClient> push_client_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintXmppListener);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_XMPP_LISTENER_H_

