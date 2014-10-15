// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/client.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/proximity_auth/base64url.h"
#include "components/proximity_auth/client_observer.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/remote_status_update.h"
#include "components/proximity_auth/secure_context.h"
#include "components/proximity_auth/wire_message.h"

namespace proximity_auth {
namespace {

// The key names of JSON fields for messages sent between the devices.
const char kTypeKey[] = "type";
const char kNameKey[] = "name";
const char kDataKey[] = "data";
const char kEncryptedDataKey[] = "encrypted_data";

// The types of messages that can be sent and received.
const char kMessageTypeLocalEvent[] = "event";
const char kMessageTypeRemoteStatusUpdate[] = "status_update";
const char kMessageTypeDecryptRequest[] = "decrypt_request";
const char kMessageTypeDecryptResponse[] = "decrypt_response";
const char kMessageTypeUnlockRequest[] = "unlock_request";
const char kMessageTypeUnlockResponse[] = "unlock_response";

// The name for an unlock event originating from the local device.
const char kUnlockEventName[] = "easy_unlock";

// Serializes the |value| to a JSON string and returns the result.
std::string SerializeValueToJson(const base::Value& value) {
  std::string json;
  base::JSONWriter::Write(&value, &json);
  return json;
}

// Returns the message type represented by the |message|. This is a convenience
// wrapper that should only be called when the |message| is known to specify its
// message type, i.e. this should not be called for untrusted input.
std::string GetMessageType(const base::DictionaryValue& message) {
  std::string type;
  message.GetString(kTypeKey, &type);
  return type;
}

}  // namespace

Client::Client(scoped_ptr<Connection> connection,
               scoped_ptr<SecureContext> secure_context)
    : connection_(connection.Pass()), secure_context_(secure_context.Pass()) {
  DCHECK(connection_->IsConnected());
  connection_->AddObserver(this);
}

Client::~Client() {
  if (connection_)
    connection_->RemoveObserver(this);
}

void Client::AddObserver(ClientObserver* observer) {
  observers_.AddObserver(observer);
}

void Client::RemoveObserver(ClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Client::SupportsSignIn() const {
  return (secure_context_->GetProtocolVersion() ==
          SecureContext::PROTOCOL_VERSION_THREE_ONE);
}

void Client::DispatchUnlockEvent() {
  base::DictionaryValue message;
  message.SetString(kTypeKey, kMessageTypeLocalEvent);
  message.SetString(kNameKey, kUnlockEventName);
  queued_messages_.push_back(PendingMessage(message));
  ProcessMessageQueue();
}

void Client::RequestDecryption(const std::string& challenge) {
  if (!SupportsSignIn()) {
    VLOG(1) << "[Client] Dropping decryption request, as remote device "
            << "does not support protocol v3.1.";
    FOR_EACH_OBSERVER(ClientObserver,
                      observers_,
                      OnDecryptResponse(scoped_ptr<std::string>()));
    return;
  }

  // TODO(isherman): Compute the encrypted message data for realz.
  const std::string encrypted_message_data = challenge;
  std::string encrypted_message_data_base64;
  Base64UrlEncode(encrypted_message_data, &encrypted_message_data_base64);

  base::DictionaryValue message;
  message.SetString(kTypeKey, kMessageTypeDecryptRequest);
  message.SetString(kEncryptedDataKey, encrypted_message_data_base64);
  queued_messages_.push_back(PendingMessage(message));
  ProcessMessageQueue();
}

void Client::RequestUnlock() {
  if (!SupportsSignIn()) {
    VLOG(1) << "[Client] Dropping unlock request, as remote device does not "
            << "support protocol v3.1.";
    FOR_EACH_OBSERVER(ClientObserver, observers_, OnUnlockResponse(false));
    return;
  }

  base::DictionaryValue message;
  message.SetString(kTypeKey, kMessageTypeUnlockRequest);
  queued_messages_.push_back(PendingMessage(message));
  ProcessMessageQueue();
}

Client::PendingMessage::PendingMessage() {
}

Client::PendingMessage::PendingMessage(const base::DictionaryValue& message)
    : json_message(SerializeValueToJson(message)),
      type(GetMessageType(message)) {
}

Client::PendingMessage::~PendingMessage() {
}

void Client::ProcessMessageQueue() {
  if (pending_message_ || queued_messages_.empty() ||
      connection_->is_sending_message())
    return;

  pending_message_.reset(new PendingMessage(queued_messages_.front()));
  queued_messages_.pop_front();

  connection_->SendMessage(make_scoped_ptr(new WireMessage(
      std::string(), secure_context_->Encode(pending_message_->json_message))));
}

void Client::HandleRemoteStatusUpdateMessage(
    const base::DictionaryValue& message) {
  scoped_ptr<RemoteStatusUpdate> status_update =
      RemoteStatusUpdate::Deserialize(message);
  if (!status_update) {
    VLOG(1) << "[Client] Unexpected remote status update: " << message;
    return;
  }

  FOR_EACH_OBSERVER(
      ClientObserver, observers_, OnRemoteStatusUpdate(*status_update));
}

void Client::HandleDecryptResponseMessage(
    const base::DictionaryValue& message) {
  std::string base64_data;
  std::string decrypted_data;
  scoped_ptr<std::string> response;
  if (!message.GetString(kDataKey, &base64_data) || base64_data.empty()) {
    VLOG(1) << "[Client] Decrypt response missing '" << kDataKey << "' value.";
  } else if (!Base64UrlDecode(base64_data, &decrypted_data)) {
    VLOG(1) << "[Client] Unable to base64-decode decrypt response.";
  } else {
    response.reset(new std::string(decrypted_data));
  }
  FOR_EACH_OBSERVER(
      ClientObserver, observers_, OnDecryptResponse(response.Pass()));
}

void Client::HandleUnlockResponseMessage(const base::DictionaryValue& message) {
  FOR_EACH_OBSERVER(ClientObserver, observers_, OnUnlockResponse(true));
}

void Client::OnConnectionStatusChanged(const Connection& connection,
                                       Connection::Status old_status,
                                       Connection::Status new_status) {
  DCHECK_EQ(&connection, connection_.get());
  if (new_status != Connection::CONNECTED) {
    VLOG(1) << "[Client] Secure channel disconnected...";
    connection_->RemoveObserver(this);
    connection_.reset();
    FOR_EACH_OBSERVER(ClientObserver, observers_, OnDisconnected());
    // TODO(isherman): Determine whether it's also necessary/appropriate to fire
    // this notification from the destructor.
  }
}

void Client::OnMessageReceived(const Connection& connection,
                               const WireMessage& wire_message) {
  std::string json_message = secure_context_->Decode(wire_message.payload());
  scoped_ptr<base::Value> message_value(base::JSONReader::Read(json_message));
  if (!message_value || !message_value->IsType(base::Value::TYPE_DICTIONARY)) {
    VLOG(1) << "[Client] Unable to parse message as JSON: " << json_message
            << ".";
    return;
  }

  base::DictionaryValue* message;
  bool success = message_value->GetAsDictionary(&message);
  DCHECK(success);

  std::string type;
  if (!message->GetString(kTypeKey, &type)) {
    VLOG(1) << "[Client] Missing '" << kTypeKey
            << "' key in message: " << json_message << ".";
    return;
  }

  // Remote status updates can be received out of the blue.
  if (type == kMessageTypeRemoteStatusUpdate) {
    HandleRemoteStatusUpdateMessage(*message);
    return;
  }

  // All other messages should only be received in response to a message that
  // the client sent.
  if (!pending_message_) {
    VLOG(1) << "[Client] Unexpected message received: " << json_message;
    return;
  }

  std::string expected_type;
  if (pending_message_->type == kMessageTypeDecryptRequest)
    expected_type = kMessageTypeDecryptResponse;
  else if (pending_message_->type == kMessageTypeUnlockRequest)
    expected_type = kMessageTypeUnlockResponse;
  else
    NOTREACHED();  // There are no other message types that expect a response.

  if (type != expected_type) {
    VLOG(1) << "[Client] Unexpected '" << kTypeKey << "' value in message. "
            << "Expected '" << expected_type << "' but received '" << type
            << "'.";
    return;
  }

  if (type == kMessageTypeDecryptResponse)
    HandleDecryptResponseMessage(*message);
  else if (type == kMessageTypeUnlockResponse)
    HandleUnlockResponseMessage(*message);
  else
    NOTREACHED();  // There are no other message types that expect a response.

  pending_message_.reset();
  ProcessMessageQueue();
}

void Client::OnSendCompleted(const Connection& connection,
                             const WireMessage& wire_message,
                             bool success) {
  if (!pending_message_) {
    VLOG(1) << "[Client] Unexpected message sent.";
    return;
  }

  // In the common case, wait for a response from the remote device.
  // Don't wait if the message could not be sent, as there won't ever be a
  // response in that case. Likewise, don't wait for a response to local
  // event messages, as there is no response for such messages.
  if (success && pending_message_->type != kMessageTypeLocalEvent)
    return;

  // Notify observer of failure if sending the message fails.
  // For local events, we don't expect a response, so on success, we
  // notify observers right away.
  if (pending_message_->type == kMessageTypeDecryptRequest) {
    FOR_EACH_OBSERVER(ClientObserver,
                      observers_,
                      OnDecryptResponse(scoped_ptr<std::string>()));
  } else if (pending_message_->type == kMessageTypeUnlockRequest) {
    FOR_EACH_OBSERVER(ClientObserver, observers_, OnUnlockResponse(false));
  } else if (pending_message_->type == kMessageTypeLocalEvent) {
    FOR_EACH_OBSERVER(ClientObserver, observers_, OnUnlockEventSent(success));
  } else {
    VLOG(1) << "[Client] Message of unknown type '" << pending_message_->type
            << "sent.";
  }

  pending_message_.reset();
  ProcessMessageQueue();
}

}  // namespace proximity_auth
