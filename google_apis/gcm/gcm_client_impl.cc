// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/gcm_client_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/checkin_request.h"
#include "google_apis/gcm/engine/connection_factory_impl.h"
#include "google_apis/gcm/engine/gcm_store_impl.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/http/http_network_session.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace gcm {

namespace {

// Backoff policy. Shared across reconnection logic and checkin/(un)registration
// retries.
// Note: In order to ensure a minimum of 20 seconds between server errors (for
// server reasons), we have a 30s +- 10s (33%) jitter initial backoff.
// TODO(zea): consider sharing/synchronizing the scheduling of backoff retries
// themselves.
const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  30 * 1000,  // 30 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.33,  // 33%.

  // Maximum amount of time we are willing to delay our request in ms.
  10 * 60 * 1000, // 10 minutes.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

// Indicates a message type of the received message.
enum MessageType {
  UNKNOWN,           // Undetermined type.
  DATA_MESSAGE,      // Regular data message.
  DELETED_MESSAGES,  // Messages were deleted on the server.
  SEND_ERROR,        // Error sending a message.
};

// MCS endpoints. SSL Key pinning is done automatically due to the *.google.com
// pinning rule.
// Note: modifying the endpoints will affect the ability to compare the
// GCM.CurrentEnpoint histogram across versions.
const char kMCSEndpointMain[] = "https://mtalk.google.com:5228";
const char kMCSEndpointFallback[] = "https://mtalk.google.com:443";

const int64 kDefaultCheckinInterval = 2 * 24 * 60 * 60LL;  // seconds = 2 days.
const int kMaxRegistrationRetries = 5;
const char kMessageTypeDataMessage[] = "gcm";
const char kMessageTypeDeletedMessagesKey[] = "deleted_messages";
const char kMessageTypeKey[] = "message_type";
const char kMessageTypeSendErrorKey[] = "send_error";
const char kSendErrorMessageIdKey[] = "google.message_id";
const char kSendMessageFromValue[] = "gcm@chrome.com";
const int64 kDefaultUserSerialNumber = 0LL;

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

GCMInternalsBuilder::GCMInternalsBuilder() {}
GCMInternalsBuilder::~GCMInternalsBuilder() {}

scoped_ptr<base::Clock> GCMInternalsBuilder::BuildClock() {
  return make_scoped_ptr<base::Clock>(new base::DefaultClock());
}

scoped_ptr<GCMStore> GCMInternalsBuilder::BuildGCMStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner) {
  return make_scoped_ptr<GCMStore>(
      new GCMStoreImpl(path, blocking_task_runner));
}

scoped_ptr<MCSClient> GCMInternalsBuilder::BuildMCSClient(
    const std::string& version,
    base::Clock* clock,
    ConnectionFactory* connection_factory,
    GCMStore* gcm_store) {
  return make_scoped_ptr<MCSClient>(
      new MCSClient(version,
                    clock,
                    connection_factory,
                    gcm_store));
}

scoped_ptr<ConnectionFactory> GCMInternalsBuilder::BuildConnectionFactory(
      const std::vector<GURL>& endpoints,
      const net::BackoffEntry::Policy& backoff_policy,
      scoped_refptr<net::HttpNetworkSession> network_session,
      net::NetLog* net_log) {
  return make_scoped_ptr<ConnectionFactory>(
      new ConnectionFactoryImpl(endpoints,
                                backoff_policy,
                                network_session,
                                net_log));
}

GCMClientImpl::GCMClientImpl(scoped_ptr<GCMInternalsBuilder> internals_builder)
    : internals_builder_(internals_builder.Pass()),
      state_(UNINITIALIZED),
      clock_(internals_builder_->BuildClock()),
      url_request_context_getter_(NULL),
      pending_registration_requests_deleter_(&pending_registration_requests_),
      pending_unregistration_requests_deleter_(
          &pending_unregistration_requests_),
      weak_ptr_factory_(this) {
}

GCMClientImpl::~GCMClientImpl() {
}

void GCMClientImpl::Initialize(
    const checkin_proto::ChromeBuildProto& chrome_build_proto,
    const base::FilePath& path,
    const std::vector<std::string>& account_ids,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    Delegate* delegate) {
  DCHECK_EQ(UNINITIALIZED, state_);
  DCHECK(url_request_context_getter);
  DCHECK(delegate);

  url_request_context_getter_ = url_request_context_getter;
  const net::HttpNetworkSession::Params* network_session_params =
      url_request_context_getter_->GetURLRequestContext()->
          GetNetworkSessionParams();
  DCHECK(network_session_params);
  network_session_ = new net::HttpNetworkSession(*network_session_params);

  chrome_build_proto_.CopyFrom(chrome_build_proto);
  account_ids_ = account_ids;

  gcm_store_ =
      internals_builder_->BuildGCMStore(path, blocking_task_runner).Pass();

  delegate_ = delegate;

  state_ = INITIALIZED;
}

void GCMClientImpl::Load() {
  DCHECK_EQ(INITIALIZED, state_);

  // Once the loading is completed, the check-in will be initiated.
  gcm_store_->Load(base::Bind(&GCMClientImpl::OnLoadCompleted,
                              weak_ptr_factory_.GetWeakPtr()));
  state_ = LOADING;
}

void GCMClientImpl::OnLoadCompleted(scoped_ptr<GCMStore::LoadResult> result) {
  DCHECK_EQ(LOADING, state_);

  if (!result->success) {
    ResetState();
    return;
  }

  registrations_ = result->registrations;
  device_checkin_info_.android_id = result->device_android_id;
  device_checkin_info_.secret = result->device_security_token;
  base::Time last_checkin_time = result->last_checkin_time;
  InitializeMCSClient(result.Pass());

  if (device_checkin_info_.IsValid()) {
    SchedulePeriodicCheckin(last_checkin_time);
    OnReady();
    return;
  }

  state_ = INITIAL_DEVICE_CHECKIN;
  device_checkin_info_.Reset();
  StartCheckin();
}

void GCMClientImpl::InitializeMCSClient(
    scoped_ptr<GCMStore::LoadResult> result) {
  std::vector<GURL> endpoints;
  endpoints.push_back(GURL(kMCSEndpointMain));
  endpoints.push_back(GURL(kMCSEndpointFallback));
  connection_factory_ = internals_builder_->BuildConnectionFactory(
      endpoints,
      kDefaultBackoffPolicy,
      network_session_,
      net_log_.net_log());
  mcs_client_ = internals_builder_->BuildMCSClient(
      chrome_build_proto_.chrome_version(),
      clock_.get(),
      connection_factory_.get(),
      gcm_store_.get()).Pass();

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

  delegate_->OnGCMReady();
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

void GCMClientImpl::StartCheckin() {
  checkin_request_.reset(
      new CheckinRequest(base::Bind(&GCMClientImpl::OnCheckinCompleted,
                                    weak_ptr_factory_.GetWeakPtr()),
                         kDefaultBackoffPolicy,
                         chrome_build_proto_,
                         device_checkin_info_.android_id,
                         device_checkin_info_.secret,
                         account_ids_,
                         url_request_context_getter_));
  checkin_request_->Start();
}

void GCMClientImpl::OnCheckinCompleted(uint64 android_id,
                                       uint64 security_token) {
  checkin_request_.reset();

  CheckinInfo checkin_info;
  checkin_info.android_id = android_id;
  checkin_info.secret = security_token;

  if (!checkin_info.IsValid()) {
    // TODO(fgorski): I don't think a retry here will help, we should probalby
    // start over. By checking in with (0, 0).
    return;
  }

  if (state_ == INITIAL_DEVICE_CHECKIN) {
    OnFirstTimeDeviceCheckinCompleted(checkin_info);
  } else {
    // checkin_info is not expected to change after a periodic checkin as it
    // would invalidate the registratoin IDs.
    DCHECK_EQ(READY, state_);
    DCHECK_EQ(device_checkin_info_.android_id, checkin_info.android_id);
    DCHECK_EQ(device_checkin_info_.secret, checkin_info.secret);
  }

  if (device_checkin_info_.IsValid()) {
    base::Time last_checkin_time = clock_->Now();
    gcm_store_->SetLastCheckinTime(
        last_checkin_time,
        base::Bind(&GCMClientImpl::SetLastCheckinTimeCallback,
                   weak_ptr_factory_.GetWeakPtr()));
    SchedulePeriodicCheckin(last_checkin_time);
  }
}

void GCMClientImpl::SchedulePeriodicCheckin(
    const base::Time& last_checkin_time) {
  base::TimeDelta time_to_next_checkin = last_checkin_time +
      base::TimeDelta::FromSeconds(kDefaultCheckinInterval) - clock_->Now();
  if (time_to_next_checkin < base::TimeDelta::FromSeconds(0L))
    time_to_next_checkin = base::TimeDelta::FromSeconds(0L);
  // TODO(fgorski): Make sure that once dynamic events (like accounts list
  // change) trigger checkin we reset the timer.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GCMClientImpl::StartCheckin,
                 weak_ptr_factory_.GetWeakPtr()),
      time_to_next_checkin);
}

void GCMClientImpl::SetLastCheckinTimeCallback(bool success) {
  // TODO(fgorski): This is one of the signals that store needs a rebuild.
  DCHECK(success);
}

void GCMClientImpl::SetDeviceCredentialsCallback(bool success) {
  // TODO(fgorski): This is one of the signals that store needs a rebuild.
  DCHECK(success);
}

void GCMClientImpl::UpdateRegistrationCallback(bool success) {
  // TODO(fgorski): This is one of the signals that store needs a rebuild.
  DCHECK(success);
}

void GCMClientImpl::Stop() {
  device_checkin_info_.Reset();
  connection_factory_.reset();
  mcs_client_.reset();
  checkin_request_.reset();
  pending_registration_requests_.clear();
  state_ = INITIALIZED;
  gcm_store_->Close();
}

void GCMClientImpl::CheckOut() {
  Stop();
  gcm_store_->Destroy(base::Bind(&GCMClientImpl::OnGCMStoreDestroyed,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void GCMClientImpl::Register(const std::string& app_id,
                             const std::vector<std::string>& sender_ids) {
  DCHECK_EQ(state_, READY);

  // If the same sender ids is provided, return the cached registration ID
  // directly.
  RegistrationInfoMap::const_iterator registrations_iter =
      registrations_.find(app_id);
  if (registrations_iter != registrations_.end() &&
      registrations_iter->second->sender_ids == sender_ids) {
    delegate_->OnRegisterFinished(
        app_id, registrations_iter->second->registration_id, SUCCESS);
    return;
  }

  RegistrationRequest::RequestInfo request_info(
      device_checkin_info_.android_id,
      device_checkin_info_.secret,
      app_id,
      sender_ids);
  DCHECK_EQ(0u, pending_registration_requests_.count(app_id));

  RegistrationRequest* registration_request =
      new RegistrationRequest(request_info,
                              kDefaultBackoffPolicy,
                              base::Bind(&GCMClientImpl::OnRegisterCompleted,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         app_id,
                                         sender_ids),
                              kMaxRegistrationRetries,
                              url_request_context_getter_);
  pending_registration_requests_[app_id] = registration_request;
  registration_request->Start();
}

void GCMClientImpl::OnRegisterCompleted(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    RegistrationRequest::Status status,
    const std::string& registration_id) {
  DCHECK(delegate_);

  Result result;
  PendingRegistrationRequests::iterator iter =
      pending_registration_requests_.find(app_id);
  if (iter == pending_registration_requests_.end())
    result = UNKNOWN_ERROR;
  else if (status == RegistrationRequest::INVALID_SENDER)
    result = INVALID_PARAMETER;
  else if (registration_id.empty())
    result = SERVER_ERROR;
  else
    result = SUCCESS;

  if (result == SUCCESS) {
    // Cache it.
    linked_ptr<RegistrationInfo> registration(new RegistrationInfo);
    registration->sender_ids = sender_ids;
    registration->registration_id = registration_id;
    registrations_[app_id] = registration;

    // Save it in the persistent store.
    gcm_store_->AddRegistration(
        app_id,
        registration,
        base::Bind(&GCMClientImpl::UpdateRegistrationCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  delegate_->OnRegisterFinished(
      app_id, result == SUCCESS ? registration_id : std::string(), result);

  if (iter != pending_registration_requests_.end()) {
    delete iter->second;
    pending_registration_requests_.erase(iter);
  }
}

void GCMClientImpl::Unregister(const std::string& app_id) {
  DCHECK_EQ(state_, READY);
  if (pending_unregistration_requests_.count(app_id) == 1)
    return;

  // Remove from the cache and persistent store.
  registrations_.erase(app_id);
  gcm_store_->RemoveRegistration(
      app_id,
      base::Bind(&GCMClientImpl::UpdateRegistrationCallback,
                 weak_ptr_factory_.GetWeakPtr()));

  UnregistrationRequest::RequestInfo request_info(
      device_checkin_info_.android_id,
      device_checkin_info_.secret,
      app_id);

  UnregistrationRequest* unregistration_request =
      new UnregistrationRequest(
          request_info,
          kDefaultBackoffPolicy,
          base::Bind(&GCMClientImpl::OnUnregisterCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     app_id),
          url_request_context_getter_);
  pending_unregistration_requests_[app_id] = unregistration_request;
  unregistration_request->Start();
}

void GCMClientImpl::OnUnregisterCompleted(
    const std::string& app_id,
    UnregistrationRequest::Status status) {
  DVLOG(1) << "Unregister completed for app: " << app_id
           << " with " << (status ? "success." : "failure.");
  delegate_->OnUnregisterFinished(
      app_id,
      status == UnregistrationRequest::SUCCESS ? SUCCESS : SERVER_ERROR);

  PendingUnregistrationRequests::iterator iter =
      pending_unregistration_requests_.find(app_id);
  if (iter == pending_unregistration_requests_.end())
    return;

  delete iter->second;
  pending_unregistration_requests_.erase(iter);
}

void GCMClientImpl::OnGCMStoreDestroyed(bool success) {
  DLOG_IF(ERROR, !success) << "GCM store failed to be destroyed!";
  UMA_HISTOGRAM_BOOLEAN("GCM.StoreDestroySucceeded", success);
}

void GCMClientImpl::Send(const std::string& app_id,
                         const std::string& receiver_id,
                         const OutgoingMessage& message) {
  DCHECK_EQ(state_, READY);

  mcs_proto::DataMessageStanza stanza;
  stanza.set_ttl(message.time_to_live);
  stanza.set_sent(clock_->Now().ToInternalValue() /
                  base::Time::kMicrosecondsPerSecond);
  stanza.set_id(message.id);
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

std::string GCMClientImpl::GetStateString() const {
  switch(state_) {
    case GCMClientImpl::INITIALIZED:
      return "INITIALIZED";
    case GCMClientImpl::UNINITIALIZED:
      return "UNINITIALIZED";
    case GCMClientImpl::LOADING:
      return "LOADING";
    case GCMClientImpl::INITIAL_DEVICE_CHECKIN:
      return "INITIAL_DEVICE_CHECKIN";
    case GCMClientImpl::READY:
      return "READY";
    default:
      NOTREACHED();
      return std::string();
  }
}

GCMClient::GCMStatistics GCMClientImpl::GetStatistics() const {
  GCMClient::GCMStatistics stats;
  stats.gcm_client_state = GCMClientImpl::GetStateString();
  stats.connection_client_created = mcs_client_.get() != NULL;
  if (mcs_client_.get()) {
    stats.connection_state = mcs_client_->GetStateString();
    // TODO(juyik): add more statistics such as message metadata list, etc.
  }
  if (device_checkin_info_.android_id > 0)
    stats.android_id = device_checkin_info_.android_id;
  return stats;
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
  DCHECK_EQ(user_serial_number, kDefaultUserSerialNumber);
  DCHECK(delegate_);

  // TTL_EXCEEDED is singled out here, because it can happen long time after the
  // message was sent. That is why it comes as |OnMessageSendError| event rather
  // than |OnSendFinished|. SendErrorDetails.additional_data is left empty.
  // All other errors will be raised immediately, through asynchronous callback.
  // It is expected that TTL_EXCEEDED will be issued for a message that was
  // previously issued |OnSendFinished| with status SUCCESS.
  // For now, we do not report that the message has been sent and acked
  // successfully.
  // TODO(jianli): Consider adding UMA for this status.
  if (status == MCSClient::TTL_EXCEEDED) {
    SendErrorDetails send_error_details;
    send_error_details.message_id = message_id;
    send_error_details.result = GCMClient::TTL_EXCEEDED;
    delegate_->OnMessageSendError(app_id, send_error_details);
  } else if (status != MCSClient::SENT) {
    delegate_->OnSendFinished(app_id, message_id, ToGCMClientResult(status));
  }
}

void GCMClientImpl::OnMCSError() {
  // TODO(fgorski): For now it replaces the initialization method. Long term it
  // should have an error or status passed in.
}

void GCMClientImpl::HandleIncomingMessage(const gcm::MCSMessage& message) {
  DCHECK(delegate_);

  const mcs_proto::DataMessageStanza& data_message_stanza =
      reinterpret_cast<const mcs_proto::DataMessageStanza&>(
          message.GetProtobuf());
  DCHECK_EQ(data_message_stanza.device_user_id(), kDefaultUserSerialNumber);

  // Copying all the data from the stanza to a MessageData object. When present,
  // keys like kMessageTypeKey or kSendErrorMessageIdKey will be filtered out
  // later.
  MessageData message_data;
  for (int i = 0; i < data_message_stanza.app_data_size(); ++i) {
    std::string key = data_message_stanza.app_data(i).key();
    message_data[key] = data_message_stanza.app_data(i).value();
  }

  MessageType message_type = DATA_MESSAGE;
  MessageData::iterator iter = message_data.find(kMessageTypeKey);
  if (iter != message_data.end()) {
    message_type = DecodeMessageType(iter->second);
    message_data.erase(iter);
  }

  switch (message_type) {
    case DATA_MESSAGE:
      HandleIncomingDataMessage(data_message_stanza, message_data);
      break;
    case DELETED_MESSAGES:
      delegate_->OnMessagesDeleted(data_message_stanza.category());
      break;
    case SEND_ERROR:
      HandleIncomingSendError(data_message_stanza, message_data);
      break;
    case UNKNOWN:
    default:  // Treat default the same as UNKNOWN.
      DVLOG(1) << "Unknown message_type received. Message ignored. "
               << "App ID: " << data_message_stanza.category() << ".";
      break;
  }
}

void GCMClientImpl::HandleIncomingDataMessage(
    const mcs_proto::DataMessageStanza& data_message_stanza,
    MessageData& message_data) {
  std::string app_id = data_message_stanza.category();

  // Drop the message when the app is not registered for the sender of the
  // message.
  RegistrationInfoMap::iterator iter = registrations_.find(app_id);
  if (iter == registrations_.end() ||
      std::find(iter->second->sender_ids.begin(),
                iter->second->sender_ids.end(),
                data_message_stanza.from()) == iter->second->sender_ids.end()) {
    return;
  }

  IncomingMessage incoming_message;
  incoming_message.sender_id = data_message_stanza.from();
  if (data_message_stanza.has_token())
    incoming_message.collapse_key = data_message_stanza.token();
  incoming_message.data = message_data;
  delegate_->OnMessageReceived(app_id, incoming_message);
}

void GCMClientImpl::HandleIncomingSendError(
    const mcs_proto::DataMessageStanza& data_message_stanza,
    MessageData& message_data) {
  SendErrorDetails send_error_details;
  send_error_details.additional_data = message_data;
  send_error_details.result = SERVER_ERROR;

  MessageData::iterator iter =
      send_error_details.additional_data.find(kSendErrorMessageIdKey);
  if (iter != send_error_details.additional_data.end()) {
    send_error_details.message_id = iter->second;
    send_error_details.additional_data.erase(iter);
  }

  delegate_->OnMessageSendError(data_message_stanza.category(),
                                send_error_details);
}

}  // namespace gcm
