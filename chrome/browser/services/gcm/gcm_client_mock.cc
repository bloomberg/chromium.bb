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

GCMClientMock::GCMClientMock(LoadingDelay loading_delay)
    : delegate_(NULL),
      status_(UNINITIALIZED),
      loading_delay_(loading_delay),
      weak_ptr_factory_(this) {
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

void GCMClientMock::Load() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK_NE(LOADED, status_);

  if (loading_delay_ == DELAY_LOADING)
    return;
  DoLoading();
}

void GCMClientMock::DoLoading() {
  status_ = LOADED;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GCMClientMock::CheckinFinished,
                 weak_ptr_factory_.GetWeakPtr()));
}

void GCMClientMock::Stop() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  status_ = STOPPED;
}

void GCMClientMock::CheckOut() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  status_ = CHECKED_OUT;
}

void GCMClientMock::Register(const std::string& app_id,
                             const std::vector<std::string>& sender_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  std::string registration_id = GetRegistrationIdFromSenderIds(sender_ids);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GCMClientMock::RegisterFinished,
                 weak_ptr_factory_.GetWeakPtr(),
                 app_id,
                 registration_id));
}

void GCMClientMock::Unregister(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GCMClientMock::UnregisterFinished,
                 weak_ptr_factory_.GetWeakPtr(),
                 app_id));
}

void GCMClientMock::Send(const std::string& app_id,
                         const std::string& receiver_id,
                         const OutgoingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GCMClientMock::SendFinished,
                 weak_ptr_factory_.GetWeakPtr(),
                 app_id,
                 message));
}

GCMClient::GCMStatistics GCMClientMock::GetStatistics() const {
  return GCMClient::GCMStatistics();
}

void GCMClientMock::PerformDelayedLoading() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMClientMock::DoLoading, weak_ptr_factory_.GetWeakPtr()));
}

void GCMClientMock::ReceiveMessage(const std::string& app_id,
                                   const IncomingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMClientMock::MessageReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 app_id,
                 message));
}

void GCMClientMock::DeleteMessages(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMClientMock::MessagesDeleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 app_id));
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

void GCMClientMock::CheckinFinished() {
  delegate_->OnGCMReady();
}

void GCMClientMock::RegisterFinished(const std::string& app_id,
                                     const std::string& registrion_id) {
  delegate_->OnRegisterFinished(
      app_id, registrion_id, registrion_id.empty() ? SERVER_ERROR : SUCCESS);
}

void GCMClientMock::UnregisterFinished(const std::string& app_id) {
  delegate_->OnUnregisterFinished(app_id, GCMClient::SUCCESS);
}

void GCMClientMock::SendFinished(const std::string& app_id,
                                 const OutgoingMessage& message) {
  delegate_->OnSendFinished(app_id, message.id, SUCCESS);

  // Simulate send error if message id contains a hint.
  if (message.id.find("error") != std::string::npos) {
    SendErrorDetails send_error_details;
    send_error_details.message_id = message.id;
    send_error_details.result = NETWORK_ERROR;
    send_error_details.additional_data = message.data;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GCMClientMock::MessageSendError,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id,
                   send_error_details),
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

void GCMClientMock::MessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  if (delegate_)
    delegate_->OnMessageSendError(app_id, send_error_details);
}

}  // namespace gcm
