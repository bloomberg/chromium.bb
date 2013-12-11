// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/mcs_client.h"

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/base/socket_stream.h"
#include "google_apis/gcm/engine/connection_factory.h"
#include "google_apis/gcm/engine/rmq_store.h"

using namespace google::protobuf::io;

namespace gcm {

namespace {

typedef scoped_ptr<google::protobuf::MessageLite> MCSProto;

// TODO(zea): get these values from MCS settings.
const int64 kHeartbeatDefaultSeconds = 60 * 15;  // 15 minutes.

// The category of messages intended for the GCM client itself from MCS.
const char kMCSCategory[] = "com.google.android.gsf.gtalkservice";

// The from field for messages originating in the GCM client.
const char kGCMFromField[] = "gcm@android.com";

// MCS status message types.
const char kIdleNotification[] = "IdleNotification";
// TODO(zea): consume the following message types:
// const char kAlwaysShowOnIdle[] = "ShowAwayOnIdle";
// const char kPowerNotification[] = "PowerNotification";
// const char kDataActiveNotification[] = "DataActiveNotification";

// The number of unacked messages to allow before sending a stream ack.
// Applies to both incoming and outgoing messages.
// TODO(zea): make this server configurable.
const int kUnackedMessageBeforeStreamAck = 10;

// The global maximum number of pending messages to have in the send queue.
const size_t kMaxSendQueueSize = 10 * 1024;

// The maximum message size that can be sent to the server.
const int kMaxMessageBytes = 4 * 1024;  // 4KB, like the server.

// Helper for converting a proto persistent id list to a vector of strings.
bool BuildPersistentIdListFromProto(const google::protobuf::string& bytes,
                                    std::vector<std::string>* id_list) {
  mcs_proto::SelectiveAck selective_ack;
  if (!selective_ack.ParseFromString(bytes))
    return false;
  std::vector<std::string> new_list;
  for (int i = 0; i < selective_ack.id_size(); ++i) {
    DCHECK(!selective_ack.id(i).empty());
    new_list.push_back(selective_ack.id(i));
  }
  id_list->swap(new_list);
  return true;
}

}  // namespace

struct ReliablePacketInfo {
  ReliablePacketInfo();
  ~ReliablePacketInfo();

  // The stream id with which the message was sent.
  uint32 stream_id;

  // If reliable delivery was requested, the persistent id of the message.
  std::string persistent_id;

  // The type of message itself (for easier lookup).
  uint8 tag;

  // The protobuf of the message itself.
  MCSProto protobuf;
};

ReliablePacketInfo::ReliablePacketInfo()
  : stream_id(0), tag(0) {
}
ReliablePacketInfo::~ReliablePacketInfo() {}

MCSClient::MCSClient(
    const base::FilePath& rmq_path,
    ConnectionFactory* connection_factory,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : state_(UNINITIALIZED),
      android_id_(0),
      security_token_(0),
      connection_factory_(connection_factory),
      connection_handler_(NULL),
      last_device_to_server_stream_id_received_(0),
      last_server_to_device_stream_id_received_(0),
      stream_id_out_(0),
      stream_id_in_(0),
      rmq_store_(rmq_path, blocking_task_runner),
      heartbeat_interval_(
          base::TimeDelta::FromSeconds(kHeartbeatDefaultSeconds)),
      heartbeat_timer_(true, true),
      blocking_task_runner_(blocking_task_runner),
      weak_ptr_factory_(this) {
}

MCSClient::~MCSClient() {
}

void MCSClient::Initialize(
    const InitializationCompleteCallback& initialization_callback,
    const OnMessageReceivedCallback& message_received_callback,
    const OnMessageSentCallback& message_sent_callback) {
  DCHECK_EQ(state_, UNINITIALIZED);
  initialization_callback_ = initialization_callback;
  message_received_callback_ = message_received_callback;
  message_sent_callback_ = message_sent_callback;

  state_ = LOADING;
  rmq_store_.Load(base::Bind(&MCSClient::OnRMQLoadFinished,
                             weak_ptr_factory_.GetWeakPtr()));

  connection_factory_->Initialize(
      base::Bind(&MCSClient::ResetStateAndBuildLoginRequest,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&MCSClient::HandlePacketFromWire,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&MCSClient::MaybeSendMessage,
                 weak_ptr_factory_.GetWeakPtr()));
  connection_handler_ = connection_factory_->GetConnectionHandler();
}

void MCSClient::Login(uint64 android_id, uint64 security_token) {
  DCHECK_EQ(state_, LOADED);
  if (android_id != android_id_ && security_token != security_token_) {
    DCHECK(android_id);
    DCHECK(security_token);
    DCHECK(restored_unackeds_server_ids_.empty());
    android_id_ = android_id;
    security_token_ = security_token;
    rmq_store_.SetDeviceCredentials(android_id_,
                                    security_token_,
                                    base::Bind(&MCSClient::OnRMQUpdateFinished,
                                               weak_ptr_factory_.GetWeakPtr()));
  }

  state_ = CONNECTING;
  connection_factory_->Connect();
}

void MCSClient::SendMessage(const MCSMessage& message, bool use_rmq) {
  DCHECK_EQ(state_, CONNECTED);
  if (to_send_.size() > kMaxSendQueueSize) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(message_sent_callback_, "Message queue full."));
    return;
  }
  if (message.size() > kMaxMessageBytes) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(message_sent_callback_, "Message too large."));
    return;
  }

  ReliablePacketInfo* packet_info = new ReliablePacketInfo();
  packet_info->protobuf = message.CloneProtobuf();

  if (use_rmq) {
    PersistentId persistent_id = GetNextPersistentId();
    DVLOG(1) << "Setting persistent id to " << persistent_id;
    packet_info->persistent_id = persistent_id;
    SetPersistentId(persistent_id,
                    packet_info->protobuf.get());
    rmq_store_.AddOutgoingMessage(persistent_id,
                                  MCSMessage(message.tag(),
                                             *(packet_info->protobuf)),
                                  base::Bind(&MCSClient::OnRMQUpdateFinished,
                                             weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Check that there is an active connection to the endpoint.
    if (!connection_handler_->CanSendMessage()) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(message_sent_callback_, "Unable to reach endpoint"));
      return;
    }
  }
  to_send_.push_back(make_linked_ptr(packet_info));
  MaybeSendMessage();
}

void MCSClient::Destroy() {
  rmq_store_.Destroy(base::Bind(&MCSClient::OnRMQUpdateFinished,
                                weak_ptr_factory_.GetWeakPtr()));
}

void MCSClient::ResetStateAndBuildLoginRequest(
    mcs_proto::LoginRequest* request) {
  DCHECK(android_id_);
  DCHECK(security_token_);
  stream_id_in_ = 0;
  stream_id_out_ = 1;
  last_device_to_server_stream_id_received_ = 0;
  last_server_to_device_stream_id_received_ = 0;

  // TODO(zea): expire all messages older than their TTL.

  // Add any pending acknowledgments to the list of ids.
  for (StreamIdToPersistentIdMap::const_iterator iter =
           unacked_server_ids_.begin();
       iter != unacked_server_ids_.end(); ++iter) {
    restored_unackeds_server_ids_.push_back(iter->second);
  }
  unacked_server_ids_.clear();

  // Any acknowledged server ids which have not been confirmed by the server
  // are treated like unacknowledged ids.
  for (std::map<StreamId, PersistentIdList>::const_iterator iter =
           acked_server_ids_.begin();
       iter != acked_server_ids_.end(); ++iter) {
    restored_unackeds_server_ids_.insert(restored_unackeds_server_ids_.end(),
                                         iter->second.begin(),
                                         iter->second.end());
  }
  acked_server_ids_.clear();

  // Then build the request, consuming all pending acknowledgments.
  request->Swap(BuildLoginRequest(android_id_, security_token_).get());
  for (PersistentIdList::const_iterator iter =
           restored_unackeds_server_ids_.begin();
       iter != restored_unackeds_server_ids_.end(); ++iter) {
    request->add_received_persistent_id(*iter);
  }
  acked_server_ids_[stream_id_out_] = restored_unackeds_server_ids_;
  restored_unackeds_server_ids_.clear();

  // Push all unacknowledged messages to front of send queue. No need to save
  // to RMQ, as all messages that reach this point should already have been
  // saved as necessary.
  while (!to_resend_.empty()) {
    to_send_.push_front(to_resend_.back());
    to_resend_.pop_back();
  }
  DVLOG(1) << "Resetting state, with " << request->received_persistent_id_size()
           << " incoming acks pending, and " << to_send_.size()
           << " pending outgoing messages.";

  heartbeat_timer_.Stop();

  state_ = CONNECTING;
}

void MCSClient::SendHeartbeat() {
  SendMessage(MCSMessage(kHeartbeatPingTag, mcs_proto::HeartbeatPing()),
              false);
}

void MCSClient::OnRMQLoadFinished(const RMQStore::LoadResult& result) {
  if (!result.success) {
    state_ = UNINITIALIZED;
    LOG(ERROR) << "Failed to load/create RMQ state. Not connecting.";
    initialization_callback_.Run(false, 0, 0);
    return;
  }
  state_ = LOADED;
  stream_id_out_ = 1;  // Login request is hardcoded to id 1.

  if (result.device_android_id == 0 || result.device_security_token == 0) {
    DVLOG(1) << "No device credentials found, assuming new client.";
    initialization_callback_.Run(true, 0, 0);
    return;
  }

  android_id_ = result.device_android_id;
  security_token_ = result.device_security_token;

  DVLOG(1) << "RMQ Load finished with " << result.incoming_messages.size()
           << " incoming acks pending and " << result.outgoing_messages.size()
           << " outgoing messages pending.";

  restored_unackeds_server_ids_ = result.incoming_messages;

  // First go through and order the outgoing messages by recency.
  std::map<uint64, google::protobuf::MessageLite*> ordered_messages;
  for (std::map<PersistentId, google::protobuf::MessageLite*>::const_iterator
           iter = result.outgoing_messages.begin();
       iter != result.outgoing_messages.end(); ++iter) {
    uint64 timestamp = 0;
    if (!base::StringToUint64(iter->first, &timestamp)) {
      LOG(ERROR) << "Invalid restored message.";
      return;
    }
    ordered_messages[timestamp] = iter->second;
  }

  // Now go through and add the outgoing messages to the send queue in their
  // appropriate order (oldest at front, most recent at back).
  for (std::map<uint64, google::protobuf::MessageLite*>::const_iterator
           iter = ordered_messages.begin();
       iter != ordered_messages.end(); ++iter) {
    ReliablePacketInfo* packet_info = new ReliablePacketInfo();
    packet_info->protobuf.reset(iter->second);
    packet_info->persistent_id = base::Uint64ToString(iter->first);
    to_send_.push_back(make_linked_ptr(packet_info));
  }

  initialization_callback_.Run(true, android_id_, security_token_);
}

void MCSClient::OnRMQUpdateFinished(bool success) {
  LOG_IF(ERROR, !success) << "RMQ Update failed!";
  // TODO(zea): Rebuild the store from scratch in case of persistence failure?
}

void MCSClient::MaybeSendMessage() {
  if (to_send_.empty())
    return;

  if (!connection_handler_->CanSendMessage())
    return;

  // TODO(zea): drop messages older than their TTL.

  DVLOG(1) << "Pending output message found, sending.";
  MCSPacketInternal packet = to_send_.front();
  to_send_.pop_front();
  if (!packet->persistent_id.empty())
    to_resend_.push_back(packet);
  SendPacketToWire(packet.get());
}

void MCSClient::SendPacketToWire(ReliablePacketInfo* packet_info) {
  // Reset the heartbeat interval.
  heartbeat_timer_.Reset();
  packet_info->stream_id = ++stream_id_out_;
  DVLOG(1) << "Sending packet of type " << packet_info->protobuf->GetTypeName();

  // Set the proper last received stream id to acknowledge received server
  // packets.
  DVLOG(1) << "Setting last stream id received to "
           << stream_id_in_;
  SetLastStreamIdReceived(stream_id_in_,
                          packet_info->protobuf.get());
  if (stream_id_in_ != last_server_to_device_stream_id_received_) {
    last_server_to_device_stream_id_received_ = stream_id_in_;
    // Mark all acknowledged server messages as such. Note: they're not dropped,
    // as it may be that they'll need to be re-acked if this message doesn't
    // make it.
    PersistentIdList persistent_id_list;
    for (StreamIdToPersistentIdMap::const_iterator iter =
             unacked_server_ids_.begin();
         iter != unacked_server_ids_.end(); ++iter) {
      DCHECK_LE(iter->first, last_server_to_device_stream_id_received_);
      persistent_id_list.push_back(iter->second);
    }
    unacked_server_ids_.clear();
    acked_server_ids_[stream_id_out_] = persistent_id_list;
  }

  connection_handler_->SendMessage(*packet_info->protobuf);
}

void MCSClient::HandleMCSDataMesssage(
    scoped_ptr<google::protobuf::MessageLite> protobuf) {
  mcs_proto::DataMessageStanza* data_message =
      reinterpret_cast<mcs_proto::DataMessageStanza*>(protobuf.get());
  // TODO(zea): implement a proper status manager rather than hardcoding these
  // values.
  scoped_ptr<mcs_proto::DataMessageStanza> response(
      new mcs_proto::DataMessageStanza());
  response->set_from(kGCMFromField);
  bool send = false;
  for (int i = 0; i < data_message->app_data_size(); ++i) {
    const mcs_proto::AppData& app_data = data_message->app_data(i);
    if (app_data.key() == kIdleNotification) {
      // Tell the MCS server the client is not idle.
      send = true;
      mcs_proto::AppData data;
      data.set_key(kIdleNotification);
      data.set_value("false");
      response->add_app_data()->CopyFrom(data);
      response->set_category(kMCSCategory);
    }
  }

  if (send) {
    SendMessage(
        MCSMessage(kDataMessageStanzaTag,
                   response.PassAs<const google::protobuf::MessageLite>()),
        false);
  }
}

void MCSClient::HandlePacketFromWire(
    scoped_ptr<google::protobuf::MessageLite> protobuf) {
  if (!protobuf.get())
    return;
  uint8 tag = GetMCSProtoTag(*protobuf);
  PersistentId persistent_id = GetPersistentId(*protobuf);
  StreamId last_stream_id_received = GetLastStreamIdReceived(*protobuf);

  if (last_stream_id_received != 0) {
    last_device_to_server_stream_id_received_ = last_stream_id_received;

    // Process device to server messages that have now been acknowledged by the
    // server. Because messages are stored in order, just pop off all that have
    // a stream id lower than server's last received stream id.
    HandleStreamAck(last_stream_id_received);

    // Process server_to_device_messages that the server now knows were
    // acknowledged. Again, they're in order, so just keep going until the
    // stream id is reached.
    StreamIdList acked_stream_ids_to_remove;
    for (std::map<StreamId, PersistentIdList>::iterator iter =
             acked_server_ids_.begin();
         iter != acked_server_ids_.end() &&
             iter->first <= last_stream_id_received; ++iter) {
      acked_stream_ids_to_remove.push_back(iter->first);
    }
    for (StreamIdList::iterator iter = acked_stream_ids_to_remove.begin();
         iter != acked_stream_ids_to_remove.end(); ++iter) {
      acked_server_ids_.erase(*iter);
    }
  }

  ++stream_id_in_;
  if (!persistent_id.empty()) {
    unacked_server_ids_[stream_id_in_] = persistent_id;
    rmq_store_.AddIncomingMessage(persistent_id,
                                  base::Bind(&MCSClient::OnRMQUpdateFinished,
                                             weak_ptr_factory_.GetWeakPtr()));
  }

  DVLOG(1) << "Received message of type " << protobuf->GetTypeName()
           << " with persistent id "
           << (persistent_id.empty() ? "NULL" : persistent_id)
           << ", stream id " << stream_id_in_ << " and last stream id received "
           << last_stream_id_received;

  if (unacked_server_ids_.size() > 0 &&
      unacked_server_ids_.size() % kUnackedMessageBeforeStreamAck == 0) {
    SendMessage(MCSMessage(kIqStanzaTag,
                           BuildStreamAck().
                               PassAs<const google::protobuf::MessageLite>()),
                false);
  }

  switch (tag) {
    case kLoginResponseTag: {
      mcs_proto::LoginResponse* login_response =
          reinterpret_cast<mcs_proto::LoginResponse*>(protobuf.get());
      DVLOG(1) << "Received login response:";
      DVLOG(1) << "  Id: " << login_response->id();
      DVLOG(1) << "  Timestamp: " << login_response->server_timestamp();
      if (login_response->has_error()) {
        state_ = UNINITIALIZED;
        DVLOG(1) << "  Error code: " << login_response->error().code();
        DVLOG(1) << "  Error message: " << login_response->error().message();
        initialization_callback_.Run(false, 0, 0);
        return;
      }

      state_ = CONNECTED;
      stream_id_in_ = 1;  // To account for the login response.
      DCHECK_EQ(1U, stream_id_out_);

      // Pass the login response on up.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(message_received_callback_,
                     MCSMessage(tag,
                                protobuf.PassAs<
                                    const google::protobuf::MessageLite>())));

      // If there are pending messages, attempt to send one.
      if (!to_send_.empty()) {
        base::MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&MCSClient::MaybeSendMessage,
                       weak_ptr_factory_.GetWeakPtr()));
      }

      heartbeat_timer_.Start(FROM_HERE,
                             heartbeat_interval_,
                             base::Bind(&MCSClient::SendHeartbeat,
                                        weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    case kHeartbeatPingTag:
      DCHECK_GE(stream_id_in_, 1U);
      DVLOG(1) << "Received heartbeat ping, sending ack.";
      SendMessage(
          MCSMessage(kHeartbeatAckTag, mcs_proto::HeartbeatAck()), false);
      return;
    case kHeartbeatAckTag:
      DCHECK_GE(stream_id_in_, 1U);
      DVLOG(1) << "Received heartbeat ack.";
      // TODO(zea): add logic to reconnect if no ack received within a certain
      // timeout (with backoff).
      return;
    case kCloseTag:
      LOG(ERROR) << "Received close command, closing connection.";
      state_ = UNINITIALIZED;
      initialization_callback_.Run(false, 0, 0);
      // TODO(zea): should this happen in non-error cases? Reconnect?
      return;
    case kIqStanzaTag: {
      DCHECK_GE(stream_id_in_, 1U);
      mcs_proto::IqStanza* iq_stanza =
          reinterpret_cast<mcs_proto::IqStanza*>(protobuf.get());
      const mcs_proto::Extension& iq_extension = iq_stanza->extension();
      switch (iq_extension.id()) {
        case kSelectiveAck: {
          PersistentIdList acked_ids;
          if (BuildPersistentIdListFromProto(iq_extension.data(),
                                             &acked_ids)) {
            HandleSelectiveAck(acked_ids);
          }
          return;
        }
        case kStreamAck:
          // Do nothing. The last received stream id is always processed if it's
          // present.
          return;
        default:
          LOG(WARNING) << "Received invalid iq stanza extension "
                       << iq_extension.id();
          return;
      }
    }
    case kDataMessageStanzaTag: {
      DCHECK_GE(stream_id_in_, 1U);
      mcs_proto::DataMessageStanza* data_message =
          reinterpret_cast<mcs_proto::DataMessageStanza*>(protobuf.get());
      if (data_message->category() == kMCSCategory) {
        HandleMCSDataMesssage(protobuf.Pass());
        return;
      }

      DCHECK(protobuf.get());
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(message_received_callback_,
                     MCSMessage(tag,
                                protobuf.PassAs<
                                    const google::protobuf::MessageLite>())));
      return;
    }
    default:
      LOG(ERROR) << "Received unexpected message of type "
                 << static_cast<int>(tag);
      return;
  }
}

void MCSClient::HandleStreamAck(StreamId last_stream_id_received) {
  PersistentIdList acked_outgoing_persistent_ids;
  StreamIdList acked_outgoing_stream_ids;
  while (!to_resend_.empty() &&
         to_resend_.front()->stream_id <= last_stream_id_received) {
    const MCSPacketInternal& outgoing_packet = to_resend_.front();
    acked_outgoing_persistent_ids.push_back(outgoing_packet->persistent_id);
    acked_outgoing_stream_ids.push_back(outgoing_packet->stream_id);
    to_resend_.pop_front();
  }

  DVLOG(1) << "Server acked " << acked_outgoing_persistent_ids.size()
           << " outgoing messages, " << to_resend_.size()
           << " remaining unacked";
  rmq_store_.RemoveOutgoingMessages(acked_outgoing_persistent_ids,
                                    base::Bind(&MCSClient::OnRMQUpdateFinished,
                                               weak_ptr_factory_.GetWeakPtr()));

  HandleServerConfirmedReceipt(last_stream_id_received);
}

void MCSClient::HandleSelectiveAck(const PersistentIdList& id_list) {
  // First check the to_resend_ queue. Acknowledgments should always happen
  // in the order they were sent, so if messages are present they should match
  // the acknowledge list.
  PersistentIdList::const_iterator iter = id_list.begin();
  for (; iter != id_list.end() && !to_resend_.empty(); ++iter) {
    const MCSPacketInternal& outgoing_packet = to_resend_.front();
    DCHECK_EQ(outgoing_packet->persistent_id, *iter);

    // No need to re-acknowledge any server messages this message already
    // acknowledged.
    StreamId device_stream_id = outgoing_packet->stream_id;
    HandleServerConfirmedReceipt(device_stream_id);

    to_resend_.pop_front();
  }

  // If the acknowledged ids aren't all there, they might be in the to_send_
  // queue (typically when a StreamAck confirms messages as part of a login
  // response).
  for (; iter != id_list.end() && !to_send_.empty(); ++iter) {
    const MCSPacketInternal& outgoing_packet = to_send_.front();
    DCHECK_EQ(outgoing_packet->persistent_id, *iter);

    // No need to re-acknowledge any server messages this message already
    // acknowledged.
    StreamId device_stream_id = outgoing_packet->stream_id;
    HandleServerConfirmedReceipt(device_stream_id);

    to_send_.pop_front();
  }

  DCHECK(iter == id_list.end());

  DVLOG(1) << "Server acked " << id_list.size()
           << " messages, " << to_resend_.size() << " remaining unacked.";
  rmq_store_.RemoveOutgoingMessages(id_list,
                                    base::Bind(&MCSClient::OnRMQUpdateFinished,
                                               weak_ptr_factory_.GetWeakPtr()));

  // Resend any remaining outgoing messages, as they were not received by the
  // server.
  DVLOG(1) << "Resending " << to_resend_.size() << " messages.";
  while (!to_resend_.empty()) {
    to_send_.push_front(to_resend_.back());
    to_resend_.pop_back();
  }
}

void MCSClient::HandleServerConfirmedReceipt(StreamId device_stream_id) {
  // TODO(zea): use a message id the sender understands.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(message_sent_callback_,
                 "Message " + base::UintToString(device_stream_id) + " sent."));

  PersistentIdList acked_incoming_ids;
  for (std::map<StreamId, PersistentIdList>::iterator iter =
           acked_server_ids_.begin();
       iter != acked_server_ids_.end() &&
           iter->first <= device_stream_id;) {
    acked_incoming_ids.insert(acked_incoming_ids.end(),
                              iter->second.begin(),
                              iter->second.end());
    acked_server_ids_.erase(iter++);
  }

  DVLOG(1) << "Server confirmed receipt of " << acked_incoming_ids.size()
           << " acknowledged server messages.";
  rmq_store_.RemoveIncomingMessages(acked_incoming_ids,
                                    base::Bind(&MCSClient::OnRMQUpdateFinished,
                                               weak_ptr_factory_.GetWeakPtr()));
}

MCSClient::PersistentId MCSClient::GetNextPersistentId() {
  return base::Uint64ToString(base::TimeTicks::Now().ToInternalValue());
}

} // namespace gcm
