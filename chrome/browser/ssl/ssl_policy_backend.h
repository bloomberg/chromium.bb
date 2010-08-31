// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_POLICY_BACKEND_H_
#define CHROME_BROWSER_SSL_SSL_POLICY_BACKEND_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "net/base/x509_certificate.h"

class NavigationController;
class SSLHostState;
class Task;

class SSLPolicyBackend {
 public:
  explicit SSLPolicyBackend(NavigationController* controller);

  // Records that a host has run insecure content.
  void HostRanInsecureContent(const std::string& host, int pid);

  // Returns whether the specified host ran insecure content.
  bool DidHostRunInsecureContent(const std::string& host, int pid) const;

  // Records that |cert| is permitted to be used for |host| in the future.
  void DenyCertForHost(net::X509Certificate* cert, const std::string& host);

  // Records that |cert| is not permitted to be used for |host| in the future.
  void AllowCertForHost(net::X509Certificate* cert, const std::string& host);

  // Queries whether |cert| is allowed or denied for |host|.
  net::CertPolicy::Judgment QueryPolicy(
      net::X509Certificate* cert, const std::string& host);

  // Shows the pending messages (in info-bars) if any.
  void ShowPendingMessages();

  // Clears any pending messages.
  void ClearPendingMessages();

 private:
  // SSLMessageInfo contains the information necessary for displaying a message
  // in an info-bar.
  struct SSLMessageInfo {
   public:
    explicit SSLMessageInfo(const string16& text)
        : message(text),
          action(NULL) { }

    SSLMessageInfo(const string16& message,
                   const string16& link_text,
                   Task* action)
        : message(message), link_text(link_text), action(action) { }

    // Overridden so that std::find works.
    bool operator==(const string16& other_message) const {
      // We are uniquing SSLMessageInfo by their message only.
      return message == other_message;
    }

    string16 message;
    string16 link_text;
    Task* action;
  };

  // Ensure that the specified message is displayed to the user. This will
  // display an InfoBar at the top of the associated tab. It also contains a
  // link that when clicked run the specified task. The SSL Manager becomes the
  // owner of the task.
  void ShowMessageWithLink(const string16& msg,
                           const string16& link_text,
                           Task* task);

  // The NavigationController that owns this SSLManager.  We are responsible
  // for the security UI of this tab.
  NavigationController* controller_;

  // SSL state specific for each host.
  SSLHostState* ssl_host_state_;

  // The list of messages that should be displayed (in info bars) when the page
  // currently loading had loaded.
  std::vector<SSLMessageInfo> pending_messages_;

  DISALLOW_COPY_AND_ASSIGN(SSLPolicyBackend);
};

#endif  // CHROME_BROWSER_SSL_SSL_POLICY_BACKEND_H_
