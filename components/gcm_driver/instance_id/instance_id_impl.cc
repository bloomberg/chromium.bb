// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_impl.h"

#include <algorithm>
#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "crypto/random.h"

namespace instance_id {

// static
InstanceID* InstanceID::Create(const std::string& app_id,
                               gcm::GCMDriver* gcm_driver) {
  return new InstanceIDImpl(app_id, gcm_driver);
}

InstanceIDImpl::InstanceIDImpl(const std::string& app_id,
                               gcm::GCMDriver* gcm_driver)
    : InstanceID(app_id),
      gcm_driver_(gcm_driver) {
}

InstanceIDImpl::~InstanceIDImpl() {
}

std::string InstanceIDImpl::GetID() {
  EnsureIDGenerated();
  return id_;
}

base::Time InstanceIDImpl::GetCreationTime() {
  return creation_time_;
}

void InstanceIDImpl::GetToken(
    const std::string& authorized_entity,
    const std::string& scope,
    const std::map<std::string, std::string>& options,
    const GetTokenCallback& callback) {
  NOTIMPLEMENTED();
}

void InstanceIDImpl::DeleteToken(const std::string& authorized_entity,
                                 const std::string& scope,
                                 const DeleteTokenCallback& callback) {
  NOTIMPLEMENTED();
}

void InstanceIDImpl::DeleteID(const DeleteIDCallback& callback) {
  // TODO(jianli): Delete the ID from the store.
  id_.clear();
  creation_time_ = base::Time();

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, InstanceID::SUCCESS));
}

void InstanceIDImpl::EnsureIDGenerated() {
  if (!id_.empty())
    return;

  // Now produce the ID in the following steps:

  // 1) Generates the random number in 8 bytes which is required by the server.
  //    We don't want to be strictly cryptographically secure. The server might
  //    reject the ID if there is a conflict or problem.
  uint8 bytes[kInstanceIDByteLength];
  crypto::RandBytes(bytes, sizeof(bytes));

  // 2) Transforms the first 4 bits to 0x7. Note that this is required by the
  //    server.
  bytes[0] &= 0x0f;
  bytes[0] |= 0x70;

  // 3) Encode the value in Android-compatible base64 scheme:
  //    * URL safe: '/' replaced by '_' and '+' replaced by '-'.
  //    * No padding: any trailing '=' will be removed.
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(bytes), sizeof(bytes)),
      &id_);
  std::replace(id_.begin(), id_.end(), '+', '-');
  std::replace(id_.begin(), id_.end(), '/', '_');
  id_.erase(std::remove(id_.begin(), id_.end(), '='), id_.end());

  creation_time_ = base::Time::Now();

  // TODO(jianli):  Save the ID to the store.
}

}  // namespace instance_id
