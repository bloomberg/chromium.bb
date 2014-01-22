// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/gcm_client_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/checkin_request.h"
#include "google_apis/gcm/engine/connection_factory_impl.h"
#include "google_apis/gcm/engine/gcm_store_impl.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/engine/user_list.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/http/http_network_session.h"
#include "url/gurl.h"

namespace gcm {

namespace {
const char kMCSEndpoint[] = "https://mtalk.google.com:5228";
}  // namespace

GCMClientImpl::GCMClientImpl()
    : state_(UNINITIALIZED),
      url_request_context_getter_(NULL),
      pending_checkins_deleter_(&pending_checkins_) {
}

GCMClientImpl::~GCMClientImpl() {
}

void GCMClientImpl::Initialize(
    const checkin_proto::ChromeBuildProto& chrome_build_proto,
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter) {
  DCHECK_EQ(UNINITIALIZED, state_);
  DCHECK(url_request_context_getter);

  chrome_build_proto_.CopyFrom(chrome_build_proto);
  url_request_context_getter_ = url_request_context_getter;

  gcm_store_.reset(new GCMStoreImpl(false, path, blocking_task_runner));
  gcm_store_->Load(base::Bind(&GCMClientImpl::OnLoadCompleted,
                              base::Unretained(this)));
  user_list_.reset(new UserList(gcm_store_.get()));
  connection_factory_.reset(new ConnectionFactoryImpl(GURL(kMCSEndpoint),
                                                      network_session_,
                                                      net_log_.net_log()));
  mcs_client_.reset(new MCSClient(&clock_,
                                  connection_factory_.get(),
                                  gcm_store_.get()));
  state_ = LOADING;
}

void GCMClientImpl::OnLoadCompleted(const GCMStore::LoadResult& result) {
  DCHECK_EQ(LOADING, state_);

  if (!result.success) {
    ResetState();
    return;
  }

  user_list_->Initialize(result.serial_number_mappings);

  device_checkin_info_.android_id = result.device_android_id;
  device_checkin_info_.secret = result.device_security_token;
  InitializeMCSClient(result);
  if (!device_checkin_info_.IsValid()) {
    device_checkin_info_.Reset();
    state_ = INITIAL_DEVICE_CHECKIN;
    StartCheckin(0, device_checkin_info_);
  } else {
    state_ = READY;
    StartMCSLogin();
  }
}

void GCMClientImpl::InitializeMCSClient(const GCMStore::LoadResult& result) {
  mcs_client_->Initialize(
      base::Bind(&GCMClientImpl::OnMCSError, base::Unretained(this)),
      base::Bind(&GCMClientImpl::OnMessageReceivedFromMCS,
                 base::Unretained(this)),
      base::Bind(&GCMClientImpl::OnMessageSentToMCS, base::Unretained(this)),
      result);
}

void GCMClientImpl::OnFirstTimeDeviceCheckinCompleted(
    const CheckinInfo& checkin_info) {
  DCHECK(!device_checkin_info_.IsValid());

  state_ = READY;
  device_checkin_info_.android_id = checkin_info.android_id;
  device_checkin_info_.secret = checkin_info.secret;
  gcm_store_->SetDeviceCredentials(
      checkin_info.android_id, checkin_info.secret,
      base::Bind(&GCMClientImpl::SetDeviceCredentialsCallback,
                 base::Unretained(this)));
  StartMCSLogin();
}

void GCMClientImpl::StartMCSLogin() {
  DCHECK_EQ(READY, state_);
  DCHECK(device_checkin_info_.IsValid());
  mcs_client_->Login(device_checkin_info_.android_id,
                     device_checkin_info_.secret);
}

void GCMClientImpl::ResetState() {
  state_ = UNINITIALIZED;
  // TODO(fgorski): reset all of the necessart objects and start over.
}

void GCMClientImpl::StartCheckin(int64 user_serial_number,
                                 const CheckinInfo& checkin_info) {
  DCHECK_EQ(0U, pending_checkins_.count(user_serial_number));
  CheckinRequest* checkin_request =
      new CheckinRequest(
          base::Bind(&GCMClientImpl::OnCheckinCompleted,
                     base::Unretained(this),
                     user_serial_number),
          chrome_build_proto_,
          user_serial_number,
          checkin_info.android_id,
          checkin_info.secret,
          url_request_context_getter_);
  pending_checkins_[user_serial_number] = checkin_request;
  checkin_request->Start();
}

void GCMClientImpl::OnCheckinCompleted(int64 user_serial_number,
                                       uint64 android_id,
                                       uint64 security_token) {
  CheckinInfo checkin_info;
  checkin_info.android_id = android_id;
  checkin_info.secret = security_token;

  // Delete the checkin request.
  PendingCheckins::iterator iter = pending_checkins_.find(user_serial_number);
  DCHECK(iter != pending_checkins_.end());
  delete iter->second;
  pending_checkins_.erase(iter);

  if (user_serial_number == 0) {
    OnDeviceCheckinCompleted(checkin_info);
    return;
  }

  Delegate* delegate = user_list_->GetDelegateBySerialNumber(
                                       user_serial_number);
  // TODO(fgorski): Add a reasonable Result here. It is possible that we are
  // missing the right parameter on the CheckinRequest level.
  delegate->OnCheckInFinished(checkin_info, SUCCESS);
}

void GCMClientImpl::OnDeviceCheckinCompleted(const CheckinInfo& checkin_info) {
  if (!checkin_info.IsValid()) {
    // TODO(fgorski): Trigger a retry logic here. (no need to start over).
    return;
  }

  if (state_ == INITIAL_DEVICE_CHECKIN) {
    OnFirstTimeDeviceCheckinCompleted(checkin_info);
  } else {
    DCHECK_EQ(READY, state_);
    if (device_checkin_info_.android_id != checkin_info.android_id ||
        device_checkin_info_.secret != checkin_info.secret) {
      ResetState();
    } else {
      // TODO(fgorski): Reset the checkin timer.
    }
  }
}

void GCMClientImpl::SetDeviceCredentialsCallback(bool success) {
  // TODO(fgorski): This is one of the signals that store needs a rebuild.
  DCHECK(success);
}

void GCMClientImpl::SetUserDelegate(const std::string& username,
                                    Delegate* delegate) {
  DCHECK(!username.empty());
  DCHECK(delegate);
  user_list_->SetDelegate(
      username,
      delegate,
      base::Bind(&GCMClientImpl::SetDelegateCompleted, base::Unretained(this)));
}

void GCMClientImpl::SetDelegateCompleted(const std::string& username,
                                         int64 user_serial_number) {
  Delegate* delegate = user_list_->GetDelegateByUsername(username);
  DCHECK(delegate);
  if (state_ == READY) {
    delegate->OnLoadingCompleted();
    return;
  }
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
  return state_ != READY;
}

void GCMClientImpl::OnMessageReceivedFromMCS(const gcm::MCSMessage& message) {
  // We need to do the message parsing here and then dispatch it to the right
  // delegate related to that message
  switch (message.tag()) {
    case kLoginResponseTag:
      DVLOG(1) << "Login response received by GCM Client. Ignoring.";
      return;
    case kDataMessageStanzaTag:
      DVLOG(1) << "A downstream message received. Processing...";
      HandleIncomingMessage(message);
      return;
    default:
      NOTREACHED() << "Message with unexpected tag received by GCMClient";
      return;
  }
}

void GCMClientImpl::OnMessageSentToMCS(int64 user_serial_number,
                                       const std::string& app_id,
                                       const std::string& message_id,
                                       MCSClient::MessageSendStatus status) {
  // TODO(fgorski): This is only a placeholder, it likely has to change the
  // arguments to be able to identify the user and app.
}

void GCMClientImpl::OnMCSError() {
  // TODO(fgorski): For now it replaces the initialization method. Long term it
  // should have an error or status passed in.
}

void GCMClientImpl::HandleIncomingMessage(const gcm::MCSMessage& message) {
  const mcs_proto::DataMessageStanza& data_message_stanza =
      reinterpret_cast<const mcs_proto::DataMessageStanza&>(
          message.GetProtobuf());
  IncomingMessage incoming_message;
  for (int i = 0; i < data_message_stanza.app_data_size(); ++i) {
    incoming_message.data[data_message_stanza.app_data(i).key()] =
        data_message_stanza.app_data(i).value();
  }

  int64 user_serial_number = data_message_stanza.device_user_id();
  Delegate* delegate =
      user_list_->GetDelegateBySerialNumber(user_serial_number);
  if (delegate) {
    DVLOG(1) << "Found delegate for serial number: " << user_serial_number;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&GCMClientImpl::NotifyDelegateOnMessageReceived,
                   base::Unretained(this),
                   delegate,
                   data_message_stanza.category(),
                   incoming_message));
  } else {
    DVLOG(1) << "Delegate for serial number: " << user_serial_number
             << " not found.";
  }
}

void GCMClientImpl::NotifyDelegateOnMessageReceived(
    GCMClient::Delegate* delegate,
    const std::string& app_id,
    const IncomingMessage& incoming_message) {
  delegate->OnMessageReceived(app_id, incoming_message);
}

}  // namespace gcm
