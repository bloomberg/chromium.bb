// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_SERVICE_STATE_H_
#define CLOUD_PRINT_SERVICE_SERVICE_STATE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"

// Manages Cloud Print part of Service State.
class ServiceState {
 public:
  ServiceState();
  virtual ~ServiceState();

  void Reset();

  // Initialize object from json.
  bool FromString(const std::string& json);

  // Returns object state as json.
  std::string ToString();

  // Setups object using data provided by delegate.
  bool Configure(const std::string& email,
                 const std::string& password,
                 const std::string& proxy_id);

  // Returns authentication token provided by Google server.
  virtual std::string LoginToGoogle(const std::string& service,
                                    const std::string& email,
                                    const std::string& password);

  // Returns true of object state is valid.
  bool IsValid() const;

  std::string email() const {
    return email_;
  };

  std::string proxy_id() const {
    return proxy_id_;
  };

  std::string robot_email() const {
    return robot_email_;
  };

  std::string robot_token() const {
    return robot_token_;
  };

  std::string auth_token() const {
    return auth_token_;
  };

  std::string xmpp_auth_token() const {
    return xmpp_auth_token_;
  };

  void set_email(const std::string& value) {
    email_ = value;
  };

  void set_proxy_id(const std::string& value) {
    proxy_id_ = value;
  };

  void set_robot_email(const std::string& value) {
    robot_email_ = value;
  };

  void set_robot_token(const std::string& value) {
    robot_token_ = value;
  };

  void set_auth_token(const std::string& value) {
    auth_token_ = value;
  };

  void set_xmpp_auth_token(const std::string& value) {
    xmpp_auth_token_ = value;
  };

 private:
  std::string email_;
  std::string proxy_id_;
  std::string robot_email_;
  std::string robot_token_;
  std::string auth_token_;
  std::string xmpp_auth_token_;

  DISALLOW_COPY_AND_ASSIGN(ServiceState);
};

#endif  // CLOUD_PRINT_SERVICE_SERVICE_STATE_H_

