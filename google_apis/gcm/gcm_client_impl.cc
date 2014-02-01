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
#include "base/time/default_clock.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/checkin_request.h"
#include "google_apis/gcm/engine/connection_factory_impl.h"
#include "google_apis/gcm/engine/gcm_store_impl.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/engine/registration_request.h"
#include "google_apis/gcm/engine/user_list.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/http/http_network_session.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace gcm {

namespace {

// Indicates a message type of the received message.
enum MessageType {
  UNKNOWN,           // Undetermined type.
  DATA_MESSAGE,      // Regular data message.
  DELETED_MESSAGES,  // Messages were deleted on the server.
  SEND_ERROR,        // Error sending a message.
};

const char kMCSEndpoint[] = "https://mtalk.google.com:5228";
const char kMessageTypeDataMessage[] = "gcm";
const char kMessageTypeDeletedMessagesKey[] = "deleted_messages";
const char kMessageTypeKey[] = "message_type";
const char kMessageTypeSendErrorKey[] = "send_error";
const char kSendErrorMessageIdKey[] = "google.message_id";
const char kSendMessageFromValue[] = "gcm@chrome.com";

GCMClient::Result ToGCMClientResult(MCSClient::MessageSendStatus status) {
  switch (status) {
    case MCSClient::QUEUED:
      return GCMClient::SUCCESS;
    case MCSClient::QUEUE_SIZE_LIMIT_REACHED:
      return GCMClient::NETWORK_ERROR;
    case MCSClient::APP_QUEUE_SIZE_LIMIT_REACHED:
      return GCMClient::NETWORK_ERROR;
    case MCSClient::MESSAGE_TOO_LARGE:
      return GCMClient::INVALID_PARAMETER;
    case MCSClient::NO_CONNECTION_ON_ZERO_TTL:
      return GCMClient::NETWORK_ERROR;
    case MCSClient::TTL_EXCEEDED:
      return GCMClient::NETWORK_ERROR;
    case MCSClient::SENT:
    default:
      NOTREACHED();
      break;
  }
  return GCMClientImpl::UNKNOWN_ERROR;
}

MessageType DecodeMessageType(const std::string& value) {
  if (kMessageTypeDeletedMessagesKey == value)
    return DELETED_MESSAGES;
  if (kMessageTypeSendErrorKey == value)
    return SEND_ERROR;
  if (kMessageTypeDataMessage == value)
    return DATA_MESSAGE;
  return UNKNOWN;
}

}  // namespace

GCMClientImpl::PendingRegistrationKey::PendingRegistrationKey(
    const std::string& username, const std::string& app_id)
    : username(username),
      app_id(app_id) {
}

GCMClientImpl::PendingRegistrationKey::~PendingRegistrationKey() {
}

bool GCMClientImpl::PendingRegistrationKey::operator<(
    const PendingRegistrationKey& rhs) const {
  if (username < rhs.username)
    return true;
  if (username > rhs.username)
    return false;
  return app_id < rhs.app_id;
}

GCMClientImpl::GCMClientImpl()
    : state_(UNINITIALIZED),
      clock_(new base::DefaultClock()),
      url_request_context_getter_(NULL),
      pending_checkins_deleter_(&pending_checkins_),
      pending_registrations_deleter_(&pending_registrations_),
      weak_ptr_factory_(this) {
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
                              weak_ptr_factory_.GetWeakPtr()));
  user_list_.reset(new UserList(gcm_store_.get()));

  // |mcs_client_| might already be set for testing at this point. No need to
  // create a |connection_factory_|.
  if (!mcs_client_.get()) {
    const net::HttpNetworkSession::Params* network_session_params =
        url_request_context_getter->GetURLRequestContext()->
            GetNetworkSessionParams();
    DCHECK(network_session_params);
    network_session_ = new net::HttpNetworkSession(*network_session_params);
    connection_factory_.reset(new ConnectionFactoryImpl(
        GURL(kMCSEndpoint), network_session_, net_log_.net_log()));
    mcs_client_.reset(new MCSClient(clock_.get(),
                                    connection_factory_.get(),
                                    gcm_store_.get()));
  }

  state_ = LOADING;
}

void GCMClientImpl::OnLoadCompleted(scoped_ptr<GCMStore::LoadResult> result) {
  DCHECK_EQ(LOADING, state_);

  if (!result->success) {
    ResetState();
    return;
  }

  user_list_->Initialize(result->serial_number_mappings);

  device_checkin_info_.android_id = result->device_android_id;
  device_checkin_info_.secret = result->device_security_token;
  InitializeMCSClient(result.Pass());
  if (!device_checkin_info_.IsValid()) {
    device_checkin_info_.Reset();
    state_ = INITIAL_DEVICE_CHECKIN;
    StartCheckin(0, device_checkin_info_);
    return;
  }

  OnReady();
}

void GCMClientImpl::InitializeMCSClient(
    scoped_ptr<GCMStore::LoadResult> result) {
  mcs_client_->Initialize(
      base::Bind(&GCMClientImpl::OnMCSError, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&GCMClientImpl::OnMessageReceivedFromMCS,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&GCMClientImpl::OnMessageSentToMCS,
                 weak_ptr_factory_.GetWeakPtr()),
      result.Pass());
}

void GCMClientImpl::OnFirstTimeDeviceCheckinCompleted(
    const CheckinInfo& checkin_info) {
  DCHECK(!device_checkin_info_.IsValid());

  device_checkin_info_.android_id = checkin_info.android_id;
  device_checkin_info_.secret = checkin_info.secret;
  gcm_store_->SetDeviceCredentials(
      checkin_info.android_id, checkin_info.secret,
      base::Bind(&GCMClientImpl::SetDeviceCredentialsCallback,
                 weak_ptr_factory_.GetWeakPtr()));

  OnReady();
}

void GCMClientImpl::OnReady() {
  state_ = READY;
  StartMCSLogin();

  std::vector<Delegate*> delegates = user_list_->GetAllDelegates();
  for (size_t i = 0; i < delegates.size(); ++i)
    delegates[i]->OnGCMReady();
}

void GCMClientImpl::StartMCSLogin() {
  DCHECK_EQ(READY, state_);
  DCHECK(device_checkin_info_.IsValid());
  std::vector<int64> user_serial_numbers =
      user_list_->GetAllActiveUserSerialNumbers();
  mcs_client_->Login(device_checkin_info_.android_id,
                     device_checkin_info_.secret,
                     user_serial_numbers);
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
                     weak_ptr_factory_.GetWeakPtr(),
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

  Delegate* delegate =
      user_list_->GetDelegateBySerialNumber(user_serial_number);

  // Check if the user was removed while checkin was in progress.
  if (!delegate) {
    DVLOG(1) << "Delegate for serial number: " << user_serial_number
             << " not found after checkin completed.";
    return;
  }

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
      base::Bind(&GCMClientImpl::SetDelegateCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

void GCMClientImpl::SetDelegateCompleted(const std::string& username,
                                         int64 user_serial_number) {
  Delegate* delegate = user_list_->GetDelegateByUsername(username);
  DCHECK(delegate);
  if (state_ == READY) {
    delegate->OnGCMReady();
    return;
  }
}

void GCMClientImpl::CheckIn(const std::string& username) {
  DCHECK_EQ(state_, READY);
  Delegate* delegate = user_list_->GetDelegateByUsername(username);
  DCHECK(delegate);
  int64 serial_number = user_list_->GetSerialNumberForUsername(username);
  DCHECK_NE(serial_number, kInvalidSerialNumber);
  StartCheckin(serial_number, delegate->GetCheckinInfo());
}

void GCMClientImpl::Register(const std::string& username,
                             const std::string& app_id,
                             const std::string& cert,
                             const std::vector<std::string>& sender_ids) {
  DCHECK_EQ(state_, READY);
  Delegate* delegate = user_list_->GetDelegateByUsername(username);
  DCHECK(delegate);
  int64 user_serial_number = user_list_->GetSerialNumberForUsername(username);
  DCHECK(user_serial_number);
  RegistrationRequest::RequestInfo request_info(
      device_checkin_info_.android_id,
      device_checkin_info_.secret,
      delegate->GetCheckinInfo().android_id,
      user_serial_number,
      app_id,
      cert,
      sender_ids);
  PendingRegistrationKey registration_key(username, app_id);
  DCHECK_EQ(0u, pending_registrations_.count(registration_key));

  RegistrationRequest* registration_request =
      new RegistrationRequest(request_info,
                              base::Bind(&GCMClientImpl::OnRegisterCompleted,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         registration_key),
                              url_request_context_getter_);
  pending_registrations_[registration_key] = registration_request;
  registration_request->Start();
}

void GCMClientImpl::OnRegisterCompleted(
    const PendingRegistrationKey& registration_key,
    const std::string& registration_id) {
  Delegate* delegate = user_list_->GetDelegateByUsername(
      registration_key.username);
  // Check if the user was removed while registration was in progress.
  if (!delegate) {
    DVLOG(1) << "Delegate for username: " << registration_key.username
             << " not found after registration completed.";
    return;
  }

  std::string app_id = registration_key.app_id;

  if (registration_id.empty()) {
    delegate->OnRegisterFinished(
        app_id, std::string(), GCMClient::SERVER_ERROR);
    return;
  }

  PendingRegistrations::iterator iter =
      pending_registrations_.find(registration_key);
  if (iter == pending_registrations_.end()) {
    delegate->OnRegisterFinished(
        app_id, std::string(), GCMClient::UNKNOWN_ERROR);
    return;
  }

  delete iter->second;
  pending_registrations_.erase(iter);

  delegate->OnRegisterFinished(app_id, registration_id, GCMClient::SUCCESS);
}

void GCMClientImpl::Unregister(const std::string& username,
                               const std::string& app_id) {
}

void GCMClientImpl::Send(const std::string& username,
                         const std::string& app_id,
                         const std::string& receiver_id,
                         const OutgoingMessage& message) {
  DCHECK_EQ(state_, READY);
  int64 serial_number = user_list_->GetSerialNumberForUsername(username);
  DCHECK_NE(serial_number, kInvalidSerialNumber);

  mcs_proto::DataMessageStanza stanza;
  stanza.set_ttl(message.time_to_live);
  stanza.set_sent(clock_->Now().ToInternalValue() /
                  base::Time::kMicrosecondsPerSecond);
  stanza.set_id(message.id);
  stanza.set_device_user_id(serial_number);
  stanza.set_from(kSendMessageFromValue);
  stanza.set_to(receiver_id);
  stanza.set_category(app_id);

  for (MessageData::const_iterator iter = message.data.begin();
       iter != message.data.end();
       ++iter) {
    mcs_proto::AppData* app_data = stanza.add_app_data();
    app_data->set_key(iter->first);
    app_data->set_value(iter->second);
  }

  MCSMessage mcs_message(stanza);
  DVLOG(1) << "MCS message size: " << mcs_message.size();
  mcs_client_->SendMessage(mcs_message);
}

bool GCMClientImpl::IsReady() const {
  return state_ == READY;
}

void GCMClientImpl::OnMessageReceivedFromMCS(const gcm::MCSMessage& message) {
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
  Delegate* delegate =
      user_list_->GetDelegateBySerialNumber(user_serial_number);

  // Check if the user was removed while message sending was in progress.
  if (!delegate) {
    DVLOG(1) << "Delegate for serial number: " << user_serial_number
             << " not found after checkin completed.";
    return;
  }

  // TTL_EXCEEDED is singled out here, because it can happen long time after the
  // message was sent. That is why it comes as |OnMessageSendError| event rather
  // than |OnSendFinished|. All other errors will be raised immediately, through
  // asynchronous callback.
  // It is expected that TTL_EXCEEDED will be issued for a message that was
  // previously issued |OnSendFinished| with status SUCCESS.
  // For now, we do not report that the message has been sent and acked
  // successfully.
  // TODO(jianli): Consider adding UMA for this status.
  if (status == MCSClient::TTL_EXCEEDED)
    delegate->OnMessageSendError(app_id, message_id, GCMClient::TTL_EXCEEDED);
  else if (status != MCSClient::SENT)
    delegate->OnSendFinished(app_id, message_id, ToGCMClientResult(status));
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
  MessageType message_type = DATA_MESSAGE;
  for (int i = 0; i < data_message_stanza.app_data_size(); ++i) {
    std::string key = data_message_stanza.app_data(i).key();
    if (key == kMessageTypeKey)
      message_type = DecodeMessageType(data_message_stanza.app_data(i).value());
    else
      incoming_message.data[key] = data_message_stanza.app_data(i).value();
  }

  int64 user_serial_number = data_message_stanza.device_user_id();
  Delegate* delegate =
      user_list_->GetDelegateBySerialNumber(user_serial_number);
  if (!delegate) {
    DVLOG(1) << "Delegate for serial number: " << user_serial_number
             << " not found.";
    return;
  }

  DVLOG(1) << "Found delegate for serial number: " << user_serial_number;
  switch (message_type) {
    case DATA_MESSAGE:
      delegate->OnMessageReceived(data_message_stanza.category(),
                                  incoming_message);
      break;
    case DELETED_MESSAGES:
      delegate->OnMessagesDeleted(data_message_stanza.category());
      break;
    case SEND_ERROR:
      NotifyDelegateOnMessageSendError(
          delegate, data_message_stanza.category(), incoming_message);
      break;
    case UNKNOWN:
    default:  // Treat default the same as UNKNOWN.
      DVLOG(1) << "Unknown message_type received. Message ignored. "
               << "App ID: " << data_message_stanza.category() << ", "
               << "User serial number: " << user_serial_number << ".";
      break;
  }
}

void GCMClientImpl::NotifyDelegateOnMessageSendError(
    GCMClient::Delegate* delegate,
    const std::string& app_id,
    const IncomingMessage& incoming_message) {
  MessageData::const_iterator iter =
      incoming_message.data.find(kSendErrorMessageIdKey);
  std::string message_id;
  if (iter != incoming_message.data.end())
    message_id = iter->second;
  delegate->OnMessageSendError(app_id, message_id, SERVER_ERROR);
}

void GCMClientImpl::SetMCSClientForTesting(scoped_ptr<MCSClient> mcs_client) {
  mcs_client_ = mcs_client.Pass();
}

}  // namespace gcm
