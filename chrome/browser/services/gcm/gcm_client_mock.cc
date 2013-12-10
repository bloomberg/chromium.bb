// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_client_mock.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/sys_byteorder.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"

namespace gcm {

namespace {

// Converts the 8-byte prefix of a string into a uint64 value.
uint64 HashToUInt64(const std::string& hash) {
  uint64 value;
  DCHECK_GE(hash.size(), sizeof(value));
  memcpy(&value, hash.data(), sizeof(value));
  return base::HostToNet64(value);
}

}  // namespace

GCMClientMock::GCMClientMock() : checkin_failure_enabled_(false) {
}

GCMClientMock::~GCMClientMock() {
}

void GCMClientMock::CheckIn(const std::string& username, Delegate* delegate) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  DCHECK(delegates_.find(username) == delegates_.end());
  delegates_[username] = delegate;

  // Simulate the android_id and secret by some sort of hashing.
  CheckInInfo checkin_info;
  if (!checkin_failure_enabled_)
    checkin_info = GetCheckInInfoFromUsername(username);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GCMClientMock::CheckInFinished,
                 base::Unretained(this),
                 username,
                 checkin_info));
}

void GCMClientMock::Register(const std::string& username,
                             const std::string& app_id,
                             const std::string& cert,
                             const std::vector<std::string>& sender_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  std::string registration_id = GetRegistrationIdFromSenderIds(sender_ids);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GCMClientMock::RegisterFinished,
                 base::Unretained(this),
                 username,
                 app_id,
                 registration_id));
}

void GCMClientMock::Unregister(const std::string& username,
                               const std::string& app_id) {
}

void GCMClientMock::Send(const std::string& username,
                         const std::string& app_id,
                         const std::string& receiver_id,
                         const OutgoingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GCMClientMock::SendFinished,
                 base::Unretained(this),
                 username,
                 app_id,
                 message.id));
}

bool GCMClientMock::IsLoading() const {
  return false;
}

void GCMClientMock::ReceiveMessage(const std::string& username,
                                   const std::string& app_id,
                                   const IncomingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMClientMock::MessageReceived,
                 base::Unretained(this),
                 username,
                 app_id,
                 message));
}

void GCMClientMock::DeleteMessages(const std::string& username,
                                   const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMClientMock::MessagesDeleted,
                 base::Unretained(this),
                 username,
                 app_id));
}

GCMClient::CheckInInfo GCMClientMock::GetCheckInInfoFromUsername(
    const std::string& username) const {
  CheckInInfo checkin_info;
  checkin_info.android_id = HashToUInt64(username);
  checkin_info.secret = checkin_info.android_id / 10;
  return checkin_info;
}

std::string GCMClientMock::GetRegistrationIdFromSenderIds(
    const std::vector<std::string>& sender_ids) const {
  // Simulate the registration_id by concaternating all sender IDs.
  // Set registration_id to empty to denote an error if sender_ids contains a
  // hint.
  std::string registration_id;
  if (sender_ids.size() != 1 ||
      sender_ids[0].find("error") == std::string::npos) {
    for (size_t i = 0; i < sender_ids.size(); ++i) {
      if (i > 0)
        registration_id += ",";
      registration_id += sender_ids[i];
    }
  }
  return registration_id;
}

GCMClient::Delegate* GCMClientMock::GetDelegate(
    const std::string& username) const {
  std::map<std::string, Delegate*>::const_iterator iter =
      delegates_.find(username);
  DCHECK(iter != delegates_.end());
  return iter->second;
}

void GCMClientMock::CheckInFinished(std::string username,
                                    CheckInInfo checkin_info) {
  GetDelegate(username)->OnCheckInFinished(
      checkin_info, checkin_info.IsValid() ? SUCCESS : SERVER_ERROR);
}

void GCMClientMock::RegisterFinished(std::string username,
                                     std::string app_id,
                                     std::string registrion_id) {
  GetDelegate(username)->OnRegisterFinished(
      app_id, registrion_id, registrion_id.empty() ? SERVER_ERROR : SUCCESS);
}

void GCMClientMock::SendFinished(std::string username,
                                 std::string app_id,
                                 std::string message_id) {
  GetDelegate(username)->OnSendFinished(app_id, message_id, SUCCESS);

  // Simulate send error if message id contains a hint.
  if (message_id.find("error") != std::string::npos) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GCMClientMock::MessageSendError,
                   base::Unretained(this),
                   username,
                   app_id,
                   message_id),
        base::TimeDelta::FromMilliseconds(200));
  }
}

void GCMClientMock::MessageReceived(std::string username,
                                    std::string app_id,
                                    IncomingMessage message) {
  GetDelegate(username)->OnMessageReceived(app_id, message);
}

void GCMClientMock::MessagesDeleted(std::string username, std::string app_id) {
  GetDelegate(username)->OnMessagesDeleted(app_id);
}

void GCMClientMock::MessageSendError(std::string username,
                                     std::string app_id,
                                     std::string message_id) {
  GetDelegate(username)->OnMessageSendError(app_id, message_id, NETWORK_ERROR);
}

}  // namespace gcm
