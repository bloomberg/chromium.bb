// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/gcm_client_impl.h"

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/engine/gcm_store_impl.h"
#include "google_apis/gcm/engine/user_list.h"

namespace gcm {

GCMClientImpl::GCMClientImpl() {
}

GCMClientImpl::~GCMClientImpl() {
}

void GCMClientImpl::Initialize(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  gcm_store_.reset(new GCMStoreImpl(false,
                                    path,
                                    blocking_task_runner));
  user_list_.reset(new UserList(gcm_store_.get()));
}

void GCMClientImpl::SetUserDelegate(const std::string& username,
                                    Delegate* delegate) {
}

void GCMClientImpl::CheckIn(const std::string& username) {
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
