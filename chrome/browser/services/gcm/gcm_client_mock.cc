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

GCMClientMock::GCMClientMock(Status status, ErrorSimulation error_simulation)
    : delegate_(NULL),
      status_(status),
      error_simulation_(error_simulation) {
}

GCMClientMock::~GCMClientMock() {
}

void GCMClientMock::Initialize(
    const checkin_proto::ChromeBuildProto& chrome_build_proto,
    const base::FilePath& store_path,
    const std::vector<std::string>& account_ids,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    Delegate* delegate) {
  delegate_ = delegate;
}

void GCMClientMock::CheckOut() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
}

void GCMClientMock::Register(const std::string& app_id,
                             const std::string& cert,
                             const std::vector<std::string>& sender_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  std::string registration_id;
  if (error_simulation_ == ALWAYS_SUCCEED)
    registration_id = GetRegistrationIdFromSenderIds(sender_ids);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GCMClientMock::RegisterFinished,
                 base::Unretained(this),
                 app_id,
                 registration_id));
}

void GCMClientMock::Unregister(const std::string& app_id) {
}

void GCMClientMock::Send(const std::string& app_id,
                         const std::string& receiver_id,
                         const OutgoingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GCMClientMock::SendFinished,
                 base::Unretained(this),
                 app_id,
                 message.id));
}

bool GCMClientMock::IsReady() const {
  return status_ == READY;
}

void GCMClientMock::ReceiveMessage(const std::string& app_id,
                                   const IncomingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMClientMock::MessageReceived,
                 base::Unretained(this),
                 app_id,
                 message));
}

void GCMClientMock::DeleteMessages(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMClientMock::MessagesDeleted,
                 base::Unretained(this),
                 app_id));
}

void GCMClientMock::SetReady() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(status_, NOT_READY);

  status_ = READY;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMClientMock::SetReadyOnIO,
                 base::Unretained(this)));
}

// static
std::string GCMClientMock::GetRegistrationIdFromSenderIds(
    const std::vector<std::string>& sender_ids) {
  // GCMProfileService normalizes the sender IDs by making them sorted.
  std::vector<std::string> normalized_sender_ids = sender_ids;
  std::sort(normalized_sender_ids.begin(), normalized_sender_ids.end());

  // Simulate the registration_id by concaternating all sender IDs.
  // Set registration_id to empty to denote an error if sender_ids contains a
  // hint.
  std::string registration_id;
  if (sender_ids.size() != 1 ||
      sender_ids[0].find("error") == std::string::npos) {
    for (size_t i = 0; i < normalized_sender_ids.size(); ++i) {
      if (i > 0)
        registration_id += ",";
      registration_id += normalized_sender_ids[i];
    }
  }
  return registration_id;
}

void GCMClientMock::RegisterFinished(const std::string& app_id,
                                     const std::string& registrion_id) {
  delegate_->OnRegisterFinished(
      app_id, registrion_id, registrion_id.empty() ? SERVER_ERROR : SUCCESS);
}

void GCMClientMock::SendFinished(const std::string& app_id,
                                 const std::string& message_id) {
  delegate_->OnSendFinished(app_id, message_id, SUCCESS);

  // Simulate send error if message id contains a hint.
  if (message_id.find("error") != std::string::npos) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GCMClientMock::MessageSendError,
                   base::Unretained(this),
                   app_id,
                   message_id),
        base::TimeDelta::FromMilliseconds(200));
  }
}

void GCMClientMock::MessageReceived(const std::string& app_id,
                                    const IncomingMessage& message) {
  if (delegate_)
    delegate_->OnMessageReceived(app_id, message);
}

void GCMClientMock::MessagesDeleted(const std::string& app_id) {
  if (delegate_)
    delegate_->OnMessagesDeleted(app_id);
}

void GCMClientMock::MessageSendError(const std::string& app_id,
                                     const std::string& message_id) {
  if (delegate_)
    delegate_->OnMessageSendError(app_id, message_id, NETWORK_ERROR);
}

void GCMClientMock::SetReadyOnIO() {
  delegate_->OnGCMReady();
}

}  // namespace gcm
