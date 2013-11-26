// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/gcm_client_impl.h"

namespace gcm {

GCMClientImpl::GCMClientImpl() {
}

GCMClientImpl::~GCMClientImpl() {
}

void GCMClientImpl::CheckIn(const std::string& username,
                            Delegate* delegate) {
}

void GCMClientImpl::Register(const std::string& username,
                             const std::string& app_id,
                             const std::string& cert,
                             const std::vector<std::string>& sender_ids) {
}

void GCMClientImpl::Unregister(const std::string& username,
                               const std::string& app_id) {
}

void GCMClientImpl::Send(const std::string& username,
                         const std::string& app_id,
                         const std::string& receiver_id,
                         const OutgoingMessage& message) {
}

bool GCMClientImpl::IsLoading() const {
  return false;
}

}  // namespace gcm
