// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_CONNECTION_OPTIONS_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_CONNECTION_OPTIONS_H_

#include <string>

#include "talk/base/cryptstring.h"
#include "talk/base/helpers.h"

namespace notifier {

class ConnectionOptions {
 public:
  ConnectionOptions();

  bool autodetect_proxy() const { return autodetect_proxy_; }
  const std::string& proxy_host() const { return proxy_host_; }
  int proxy_port() const { return proxy_port_; }
  bool use_proxy_auth() const { return use_proxy_auth_; }
  const std::string& auth_user() const { return auth_user_; }
  const talk_base::CryptString& auth_pass() const { return auth_pass_; }
  bool allow_unverified_certs() const { return allow_unverified_certs_; }

  void set_autodetect_proxy(bool f) { autodetect_proxy_ = f; }
  void set_proxy_host(const std::string& val) { proxy_host_ = val; }
  void set_proxy_port(int val) { proxy_port_ = val; }
  void set_use_proxy_auth(bool f) { use_proxy_auth_ = f; }
  void set_auth_user(const std::string& val) { auth_user_ = val; }
  void set_auth_pass(const talk_base::CryptString& val) { auth_pass_ = val; }

  // Setting this to true opens a security hole, so it is *highly* recommended
  // that you don't do this.
  void set_allow_unverified_certs(bool allow_unverified_certs) {
    allow_unverified_certs_ = allow_unverified_certs;
  }

 private:
  bool autodetect_proxy_;
  std::string proxy_host_;
  int proxy_port_;
  bool use_proxy_auth_;
  std::string auth_user_;
  talk_base::CryptString auth_pass_;
  bool allow_unverified_certs_;
  // Allow the copy constructor and operator=.
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_CONNECTION_OPTIONS_H_
