// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#if !defined(OS_ANDROID)
// channel_common.proto defines ANDROID constant that conflicts with Android
// build. At the same time TiclInvalidationService is not used on Android so it
// is safe to exclude these protos from Android build.
#include "google/cacheinvalidation/android_channel.pb.h"
#include "google/cacheinvalidation/channel_common.pb.h"
#include "google/cacheinvalidation/types.pb.h"
#endif
#include "components/invalidation/gcm_network_channel.h"
#include "components/invalidation/gcm_network_channel_delegate.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace syncer {

namespace {

const char kCacheInvalidationEndpointUrl[] =
    "https://clients4.google.com/invalidation/android/request/";
const char kCacheInvalidationPackageName[] = "com.google.chrome.invalidations";

// Register backoff policy.
const net::BackoffEntry::Policy kRegisterBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  2000, // 2 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.2, // 20%.

  // Maximum amount of time we are willing to delay our request in ms.
  1000 * 3600 * 4, // 4 hours.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

// Incoming message status values for UMA_HISTOGRAM.
enum IncomingMessageStatus {
  INCOMING_MESSAGE_SUCCESS,
  MESSAGE_EMPTY,     // GCM message's content is missing or empty.
  INVALID_ENCODING,  // Base64Decode failed.
  INVALID_PROTO,     // Parsing protobuf failed.

  // This enum is used in UMA_HISTOGRAM_ENUMERATION. Insert new values above
  // this line.
  INCOMING_MESSAGE_STATUS_COUNT
};

// Outgoing message status values for UMA_HISTOGRAM.
enum OutgoingMessageStatus {
  OUTGOING_MESSAGE_SUCCESS,
  MESSAGE_DISCARDED,     // New message started before old one was sent.
  ACCESS_TOKEN_FAILURE,  // Requeting access token failed.
  POST_FAILURE,          // HTTP Post failed.

  // This enum is used in UMA_HISTOGRAM_ENUMERATION. Insert new values above
  // this line.
  OUTGOING_MESSAGE_STATUS_COUNT
};

const char kIncomingMessageStatusHistogram[] =
    "GCMInvalidations.IncomingMessageStatus";
const char kOutgoingMessageStatusHistogram[] =
    "GCMInvalidations.OutgoingMessageStatus";

void RecordIncomingMessageStatus(IncomingMessageStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kIncomingMessageStatusHistogram,
                            status,
                            INCOMING_MESSAGE_STATUS_COUNT);
}

void RecordOutgoingMessageStatus(OutgoingMessageStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kOutgoingMessageStatusHistogram,
                            status,
                            OUTGOING_MESSAGE_STATUS_COUNT);
}

}  // namespace

GCMNetworkChannel::GCMNetworkChannel(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_ptr<GCMNetworkChannelDelegate> delegate)
    : request_context_getter_(request_context_getter),
      delegate_(delegate.Pass()),
      register_backoff_entry_(new net::BackoffEntry(&kRegisterBackoffPolicy)),
      gcm_channel_online_(false),
      http_channel_online_(false),
      diagnostic_info_(this),
      weak_factory_(this) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  delegate_->Initialize(base::Bind(&GCMNetworkChannel::OnConnectionStateChanged,
                                   weak_factory_.GetWeakPtr()));
  Register();
}

GCMNetworkChannel::~GCMNetworkChannel() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void GCMNetworkChannel::Register() {
  delegate_->Register(base::Bind(&GCMNetworkChannel::OnRegisterComplete,
                                 weak_factory_.GetWeakPtr()));
}

void GCMNetworkChannel::OnRegisterComplete(
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  DCHECK(CalledOnValidThread());
  if (result == gcm::GCMClient::SUCCESS) {
    DCHECK(!registration_id.empty());
    DVLOG(2) << "Got registration_id";
    register_backoff_entry_->Reset();
    registration_id_ = registration_id;
    if (!cached_message_.empty())
      RequestAccessToken();
  } else {
    DVLOG(2) << "Register failed: " << result;
    // Retry in case of transient error.
    switch (result) {
      case gcm::GCMClient::NETWORK_ERROR:
      case gcm::GCMClient::SERVER_ERROR:
      case gcm::GCMClient::TTL_EXCEEDED:
      case gcm::GCMClient::UNKNOWN_ERROR: {
        register_backoff_entry_->InformOfRequest(false);
        base::MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&GCMNetworkChannel::Register,
                       weak_factory_.GetWeakPtr()),
            register_backoff_entry_->GetTimeUntilRelease());
        break;
      }
      default:
        break;
    }
  }
  diagnostic_info_.registration_id_ = registration_id_;
  diagnostic_info_.registration_result_ = result;
}

void GCMNetworkChannel::SendMessage(const std::string& message) {
  DCHECK(CalledOnValidThread());
  DCHECK(!message.empty());
  DVLOG(2) << "SendMessage";
  diagnostic_info_.sent_messages_count_++;
  if (!cached_message_.empty()) {
    RecordOutgoingMessageStatus(MESSAGE_DISCARDED);
  }
  cached_message_ = message;

  if (!registration_id_.empty()) {
    RequestAccessToken();
  }
}

void GCMNetworkChannel::RequestAccessToken() {
  DCHECK(CalledOnValidThread());
  delegate_->RequestToken(base::Bind(&GCMNetworkChannel::OnGetTokenComplete,
                                     weak_factory_.GetWeakPtr()));
}

void GCMNetworkChannel::OnGetTokenComplete(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  DCHECK(CalledOnValidThread());
  if (cached_message_.empty()) {
    // Nothing to do.
    return;
  }

  if (error.state() != GoogleServiceAuthError::NONE) {
    // Requesting access token failed. Persistent errors will be reported by
    // token service. Just drop this request, cacheinvalidations will retry
    // sending message and at that time we'll retry requesting access token.
    DVLOG(1) << "RequestAccessToken failed: " << error.ToString();
    RecordOutgoingMessageStatus(ACCESS_TOKEN_FAILURE);
    // Message won't get sent. Notify that http channel doesn't work.
    UpdateHttpChannelState(false);
    cached_message_.clear();
    return;
  }
  DCHECK(!token.empty());
  // Save access token in case POST fails and we need to invalidate it.
  access_token_ = token;

  DVLOG(2) << "Got access token, sending message";
  fetcher_.reset(net::URLFetcher::Create(
      BuildUrl(registration_id_), net::URLFetcher::POST, this));
  fetcher_->SetRequestContext(request_context_getter_);
  const std::string auth_header("Authorization: Bearer " + access_token_);
  fetcher_->AddExtraRequestHeader(auth_header);
  if (!echo_token_.empty()) {
    const std::string echo_header("echo-token: " + echo_token_);
    fetcher_->AddExtraRequestHeader(echo_header);
  }
  fetcher_->SetUploadData("application/x-protobuffer", cached_message_);
  fetcher_->Start();
  // Clear message to prevent accidentally resending it in the future.
  cached_message_.clear();
}

void GCMNetworkChannel::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(fetcher_, source);
  // Free fetcher at the end of function.
  scoped_ptr<net::URLFetcher> fetcher = fetcher_.Pass();

  net::URLRequestStatus status = fetcher->GetStatus();
  diagnostic_info_.last_post_response_code_ =
      status.is_success() ? source->GetResponseCode() : status.error();

  if (status.is_success() &&
      fetcher->GetResponseCode() == net::HTTP_UNAUTHORIZED) {
    DVLOG(1) << "URLFetcher failure: HTTP_UNAUTHORIZED";
    delegate_->InvalidateToken(access_token_);
  }

  if (!status.is_success() ||
      (fetcher->GetResponseCode() != net::HTTP_OK &&
       fetcher->GetResponseCode() != net::HTTP_NO_CONTENT)) {
    DVLOG(1) << "URLFetcher failure";
    RecordOutgoingMessageStatus(POST_FAILURE);
    // POST failed. Notify that http channel doesn't work.
    UpdateHttpChannelState(false);
    return;
  }

  RecordOutgoingMessageStatus(OUTGOING_MESSAGE_SUCCESS);
  // Successfully sent message. Http channel works.
  UpdateHttpChannelState(true);
  DVLOG(2) << "URLFetcher success";
}

void GCMNetworkChannel::OnIncomingMessage(const std::string& message,
                                          const std::string& echo_token) {
#if !defined(OS_ANDROID)
  if (!echo_token.empty())
    echo_token_ = echo_token;
  diagnostic_info_.last_message_empty_echo_token_ = echo_token.empty();
  diagnostic_info_.last_message_received_time_ = base::Time::Now();

  if (message.empty()) {
    RecordIncomingMessageStatus(MESSAGE_EMPTY);
    return;
  }
  std::string data;
  if (!Base64DecodeURLSafe(message, &data)) {
    RecordIncomingMessageStatus(INVALID_ENCODING);
    return;
  }
  ipc::invalidation::AddressedAndroidMessage android_message;
  if (!android_message.ParseFromString(data) ||
      !android_message.has_message()) {
    RecordIncomingMessageStatus(INVALID_PROTO);
    return;
  }
  DVLOG(2) << "Deliver incoming message";
  RecordIncomingMessageStatus(INCOMING_MESSAGE_SUCCESS);
  UpdateGcmChannelState(true);
  DeliverIncomingMessage(android_message.message());
#else
  // This code shouldn't be invoked on Android.
  NOTREACHED();
#endif
}

void GCMNetworkChannel::OnConnectionStateChanged(bool online) {
  UpdateGcmChannelState(online);
}

void GCMNetworkChannel::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType connection_type) {
  // Network connection is restored. Let's notify cacheinvalidations so it has
  // chance to retry.
  NotifyNetworkStatusChange(
      connection_type != net::NetworkChangeNotifier::CONNECTION_NONE);
}

void GCMNetworkChannel::UpdateGcmChannelState(bool online) {
  if (gcm_channel_online_ == online)
    return;
  gcm_channel_online_ = online;
  InvalidatorState channel_state = TRANSIENT_INVALIDATION_ERROR;
  if (gcm_channel_online_ && http_channel_online_)
    channel_state = INVALIDATIONS_ENABLED;
  NotifyChannelStateChange(channel_state);
}

void GCMNetworkChannel::UpdateHttpChannelState(bool online) {
  if (http_channel_online_ == online)
    return;
  http_channel_online_ = online;
  InvalidatorState channel_state = TRANSIENT_INVALIDATION_ERROR;
  if (gcm_channel_online_ && http_channel_online_)
    channel_state = INVALIDATIONS_ENABLED;
  NotifyChannelStateChange(channel_state);
}

GURL GCMNetworkChannel::BuildUrl(const std::string& registration_id) {
  DCHECK(!registration_id.empty());

#if !defined(OS_ANDROID)
  ipc::invalidation::EndpointId endpoint_id;
  endpoint_id.set_c2dm_registration_id(registration_id);
  endpoint_id.set_client_key(std::string());
  endpoint_id.set_package_name(kCacheInvalidationPackageName);
  endpoint_id.mutable_channel_version()->set_major_version(
      ipc::invalidation::INITIAL);
  std::string endpoint_id_buffer;
  endpoint_id.SerializeToString(&endpoint_id_buffer);

  ipc::invalidation::NetworkEndpointId network_endpoint_id;
  network_endpoint_id.set_network_address(
      ipc::invalidation::NetworkEndpointId_NetworkAddress_ANDROID);
  network_endpoint_id.set_client_address(endpoint_id_buffer);
  std::string network_endpoint_id_buffer;
  network_endpoint_id.SerializeToString(&network_endpoint_id_buffer);

  std::string base64URLPiece;
  Base64EncodeURLSafe(network_endpoint_id_buffer, &base64URLPiece);

  std::string url(kCacheInvalidationEndpointUrl);
  url += base64URLPiece;
  return GURL(url);
#else
  // This code shouldn't be invoked on Android.
  NOTREACHED();
  return GURL();
#endif
}

void GCMNetworkChannel::Base64EncodeURLSafe(const std::string& input,
                                            std::string* output) {
  base::Base64Encode(input, output);
  // Covert to url safe alphabet.
  base::ReplaceChars(*output, "+", "-", output);
  base::ReplaceChars(*output, "/", "_", output);
  // Trim padding.
  size_t padding_size = 0;
  for (size_t i = output->size(); i > 0 && (*output)[i - 1] == '='; --i)
    ++padding_size;
  output->resize(output->size() - padding_size);
}

bool GCMNetworkChannel::Base64DecodeURLSafe(const std::string& input,
                                            std::string* output) {
  // Add padding.
  size_t padded_size = (input.size() + 3) - (input.size() + 3) % 4;
  std::string padded_input(input);
  padded_input.resize(padded_size, '=');
  // Convert to standard base64 alphabet.
  base::ReplaceChars(padded_input, "-", "+", &padded_input);
  base::ReplaceChars(padded_input, "_", "/", &padded_input);
  return base::Base64Decode(padded_input, output);
}

void GCMNetworkChannel::SetMessageReceiver(
    invalidation::MessageCallback* incoming_receiver) {
  delegate_->SetMessageReceiver(base::Bind(
      &GCMNetworkChannel::OnIncomingMessage, weak_factory_.GetWeakPtr()));
  SyncNetworkChannel::SetMessageReceiver(incoming_receiver);
}

void GCMNetworkChannel::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> callback) {
  callback.Run(*diagnostic_info_.CollectDebugData());
}

void GCMNetworkChannel::UpdateCredentials(const std::string& email,
                                          const std::string& token) {
  // Do nothing. We get access token by requesting it for every message.
}

int GCMNetworkChannel::GetInvalidationClientType() {
#if defined(OS_IOS)
  return ipc::invalidation::ClientType::CHROME_SYNC_GCM_IOS;
#else
  return ipc::invalidation::ClientType::CHROME_SYNC_GCM_DESKTOP;
#endif
}

void GCMNetworkChannel::ResetRegisterBackoffEntryForTest(
    const net::BackoffEntry::Policy* policy) {
  register_backoff_entry_.reset(new net::BackoffEntry(policy));
}

GCMNetworkChannelDiagnostic::GCMNetworkChannelDiagnostic(
    GCMNetworkChannel* parent)
    : parent_(parent),
      last_message_empty_echo_token_(false),
      last_post_response_code_(0),
      registration_result_(gcm::GCMClient::UNKNOWN_ERROR),
      sent_messages_count_(0) {}

scoped_ptr<base::DictionaryValue>
GCMNetworkChannelDiagnostic::CollectDebugData() const {
  scoped_ptr<base::DictionaryValue> status(new base::DictionaryValue);
  status->SetString("GCMNetworkChannel.Channel", "GCM");
  std::string reg_id_hash = base::SHA1HashString(registration_id_);
  status->SetString("GCMNetworkChannel.HashedRegistrationID",
                    base::HexEncode(reg_id_hash.c_str(), reg_id_hash.size()));
  status->SetString("GCMNetworkChannel.RegistrationResult",
                    GCMClientResultToString(registration_result_));
  status->SetBoolean("GCMNetworkChannel.HadLastMessageEmptyEchoToken",
                     last_message_empty_echo_token_);
  status->SetString(
      "GCMNetworkChannel.LastMessageReceivedTime",
      base::TimeFormatShortDateAndTime(last_message_received_time_));
  status->SetInteger("GCMNetworkChannel.LastPostResponseCode",
                     last_post_response_code_);
  status->SetInteger("GCMNetworkChannel.SentMessages", sent_messages_count_);
  status->SetInteger("GCMNetworkChannel.ReceivedMessages",
                     parent_->GetReceivedMessagesCount());
  return status.Pass();
}

std::string GCMNetworkChannelDiagnostic::GCMClientResultToString(
    const gcm::GCMClient::Result result) const {
#define ENUM_CASE(x) case x: return #x; break;
  switch (result) {
    ENUM_CASE(gcm::GCMClient::SUCCESS);
    ENUM_CASE(gcm::GCMClient::NETWORK_ERROR);
    ENUM_CASE(gcm::GCMClient::SERVER_ERROR);
    ENUM_CASE(gcm::GCMClient::TTL_EXCEEDED);
    ENUM_CASE(gcm::GCMClient::UNKNOWN_ERROR);
    ENUM_CASE(gcm::GCMClient::NOT_SIGNED_IN);
    ENUM_CASE(gcm::GCMClient::INVALID_PARAMETER);
    ENUM_CASE(gcm::GCMClient::ASYNC_OPERATION_PENDING);
    ENUM_CASE(gcm::GCMClient::GCM_DISABLED);
  }
  NOTREACHED();
  return "";
}

}  // namespace syncer
