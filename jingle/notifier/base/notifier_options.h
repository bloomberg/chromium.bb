// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_BASE_NOTIFIER_OPTIONS_H_
#define JINGLE_NOTIFIER_BASE_NOTIFIER_OPTIONS_H_

#include "jingle/notifier/base/notification_method.h"
#include "net/base/host_port_pair.h"

namespace notifier {

struct NotifierOptions {
  NotifierOptions()
      : use_chrome_async_socket(true),
        try_ssltcp_first(false),
        notification_method(kDefaultNotificationMethod) {}

  NotifierOptions(const bool use_chrome_async_socket,
                  const bool try_ssltcp_first,
                  const net::HostPortPair& xmpp_host_port,
                  NotificationMethod notification_method)
      : use_chrome_async_socket(use_chrome_async_socket),
        try_ssltcp_first(try_ssltcp_first),
        xmpp_host_port(xmpp_host_port),
        notification_method(notification_method) {}

  // Indicates whether to use the chrome-socket-based buzz::AsyncSocket
  // implementation for notifications.
  bool use_chrome_async_socket;

  // Indicates that the SSLTCP port (443) is to be tried before the the XMPP
  // port (5222) during login.
  bool try_ssltcp_first;

  // Contains a custom URL and port for the notification server, if one is to
  // be used. Empty otherwise.
  net::HostPortPair xmpp_host_port;

  // Indicates the method used by sync clients while sending and listening to
  // notifications.
  NotificationMethod notification_method;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_NOTIFIER_OPTIONS_H_
