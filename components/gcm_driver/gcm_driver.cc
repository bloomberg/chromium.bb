// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "components/gcm_driver/gcm_app_handler.h"

namespace gcm {

namespace {
const char kGCMFieldTrialName[] = "GCM";
const char kGCMFieldTrialEnabledGroupName[] = "Enabled";
}  // namespace

// static
bool GCMDriver::IsAllowedForAllUsers() {
  std::string group_name =
      base::FieldTrialList::FindFullName(kGCMFieldTrialName);
  return group_name == kGCMFieldTrialEnabledGroupName;
}

GCMDriver::GCMDriver() {
}

GCMDriver::~GCMDriver() {
}

void GCMDriver::Register(const std::string& app_id,
                         const std::vector<std::string>& sender_ids,
                         const RegisterCallback& callback) {
  DCHECK(!app_id.empty());
  DCHECK(!sender_ids.empty());
  DCHECK(!callback.is_null());

  GCMClient::Result result = EnsureStarted();
  if (result != GCMClient::SUCCESS) {
    callback.Run(std::string(), result);
    return;
  }

  // If previous un/register operation is still in progress, bail out.
  if (IsAsyncOperationPending(app_id)) {
    callback.Run(std::string(), GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  // Normalize the sender IDs by making them sorted.
  std::vector<std::string> normalized_sender_ids = sender_ids;
  std::sort(normalized_sender_ids.begin(), normalized_sender_ids.end());

  register_callbacks_[app_id] = callback;

  RegisterImpl(app_id, normalized_sender_ids);
}

void GCMDriver::Unregister(const std::string& app_id,
                           const UnregisterCallback& callback) {
  DCHECK(!app_id.empty());
  DCHECK(!callback.is_null());

  GCMClient::Result result = EnsureStarted();
  if (result != GCMClient::SUCCESS) {
    callback.Run(result);
    return;
  }

  // If previous un/register operation is still in progress, bail out.
  if (IsAsyncOperationPending(app_id)) {
    callback.Run(GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  unregister_callbacks_[app_id] = callback;

  UnregisterImpl(app_id);
}

void GCMDriver::Send(const std::string& app_id,
                     const std::string& receiver_id,
                     const GCMClient::OutgoingMessage& message,
                     const SendCallback& callback) {
  DCHECK(!app_id.empty());
  DCHECK(!receiver_id.empty());
  DCHECK(!callback.is_null());

  GCMClient::Result result = EnsureStarted();
  if (result != GCMClient::SUCCESS) {
    callback.Run(std::string(), result);
    return;
  }

  // If the message with send ID is still in progress, bail out.
  std::pair<std::string, std::string> key(app_id, message.id);
  if (send_callbacks_.find(key) != send_callbacks_.end()) {
    callback.Run(message.id, GCMClient::INVALID_PARAMETER);
    return;
  }

  send_callbacks_[key] = callback;

  SendImpl(app_id, receiver_id, message);
}

void GCMDriver::RegisterFinished(const std::string& app_id,
                                 const std::string& registration_id,
                                 GCMClient::Result result) {
  std::map<std::string, RegisterCallback>::iterator callback_iter =
      register_callbacks_.find(app_id);
  if (callback_iter == register_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  RegisterCallback callback = callback_iter->second;
  register_callbacks_.erase(callback_iter);
  callback.Run(registration_id, result);
}

void GCMDriver::UnregisterFinished(const std::string& app_id,
                                   GCMClient::Result result) {
  std::map<std::string, UnregisterCallback>::iterator callback_iter =
      unregister_callbacks_.find(app_id);
  if (callback_iter == unregister_callbacks_.end())
    return;

  UnregisterCallback callback = callback_iter->second;
  unregister_callbacks_.erase(callback_iter);
  callback.Run(result);
}

void GCMDriver::SendFinished(const std::string& app_id,
                             const std::string& message_id,
                             GCMClient::Result result) {
  std::map<std::pair<std::string, std::string>, SendCallback>::iterator
      callback_iter = send_callbacks_.find(
          std::pair<std::string, std::string>(app_id, message_id));
  if (callback_iter == send_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  SendCallback callback = callback_iter->second;
  send_callbacks_.erase(callback_iter);
  callback.Run(message_id, result);
}

void GCMDriver::Shutdown() {
  for (GCMAppHandlerMap::const_iterator iter = app_handlers_.begin();
       iter != app_handlers_.end(); ++iter) {
    iter->second->ShutdownHandler();
  }
  app_handlers_.clear();
}

void GCMDriver::AddAppHandler(const std::string& app_id,
                              GCMAppHandler* handler) {
  DCHECK(!app_id.empty());
  DCHECK(handler);
  DCHECK_EQ(app_handlers_.count(app_id), 0u);
  app_handlers_[app_id] = handler;
}

void GCMDriver::RemoveAppHandler(const std::string& app_id) {
  DCHECK(!app_id.empty());
  app_handlers_.erase(app_id);
}

GCMAppHandler* GCMDriver::GetAppHandler(const std::string& app_id) {
  // Look for exact match.
  GCMAppHandlerMap::const_iterator iter = app_handlers_.find(app_id);
  if (iter != app_handlers_.end())
    return iter->second;

  // Ask the handlers whether they know how to handle it.
  for (iter = app_handlers_.begin(); iter != app_handlers_.end(); ++iter) {
    if (iter->second->CanHandle(app_id))
      return iter->second;
  }

  return &default_app_handler_;
}

bool GCMDriver::HasRegisterCallback(const std::string& app_id) {
  return register_callbacks_.find(app_id) != register_callbacks_.end();
}

void GCMDriver::ClearCallbacks() {
  register_callbacks_.clear();
  unregister_callbacks_.clear();
  send_callbacks_.clear();
}

bool GCMDriver::IsAsyncOperationPending(const std::string& app_id) const {
  return register_callbacks_.find(app_id) != register_callbacks_.end() ||
         unregister_callbacks_.find(app_id) != unregister_callbacks_.end();
}

}  // namespace gcm
