// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_impl.h"

#include "base/logging.h"

namespace instance_id {

// static
InstanceID* InstanceID::Create(const std::string& app_id) {
  return new InstanceIDImpl(app_id);
}

InstanceIDImpl::InstanceIDImpl(const std::string& app_id)
    : InstanceID(app_id) {
}

InstanceIDImpl::~InstanceIDImpl() {
}

std::string InstanceIDImpl::GetID() {
  NOTIMPLEMENTED();
  return std::string();
}

base::Time InstanceIDImpl::GetCreationTime() {
  NOTIMPLEMENTED();
  return base::Time();
}

void InstanceIDImpl::GetToken(
    const std::string& audience,
    const std::string& scope,
    const std::map<std::string, std::string>& options,
    const GetTokenCallback& callback) {
  NOTIMPLEMENTED();
}

void InstanceIDImpl::DeleteToken(const std::string& audience,
                                 const std::string& scope,
                                 const DeleteTokenCallback& callback) {
  NOTIMPLEMENTED();
}

void InstanceIDImpl::DeleteID(const DeleteIDCallback& callback) {
  NOTIMPLEMENTED();
}

}  // namespace instance_id
