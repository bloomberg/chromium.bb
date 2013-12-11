// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_MCS_CLIENT_H_
#define GOOGLE_APIS_GCM_ENGINE_MCS_CLIENT_H_

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/engine/connection_handler.h"
#include "google_apis/gcm/engine/rmq_store.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace mcs_proto {
class LoginRequest;
}

namespace gcm {

class ConnectionFactory;
struct ReliablePacketInfo;

// An MCS client. This client is in charge of all communications with an
// MCS endpoint, and is capable of reliably sending/receiving GCM messages.
// NOTE: Not thread safe. This class should live on the same thread as that
// network requests are performed on.
class GCM_EXPORT MCSClient {
 public:
  enum State {
    UNINITIALIZED,    // Uninitialized.
    LOADING,          // Waiting for RMQ load to finish.
    LOADED,           // RMQ Load finished, waiting to connect.
    CONNECTING,       // Connection in progress.
    CONNECTED,        // Connected and running.
  };

  // Callback for informing MCSClient status. It is valid for this to be
  // invoked more than once if a permanent error is encountered after a
  // successful login was initiated.
  typedef base::Callback<
      void(bool success,
           uint64 restored_android_id,
           uint64 restored_security_token)> InitializationCompleteCallback;
  // Callback when a message is received.
  typedef base::Callback<void(const MCSMessage& message)>
      OnMessageReceivedCallback;
  // Callback when a message is sent (and receipt has been acknowledged by
  // the MCS endpoint).
  // TODO(zea): pass some sort of structure containing more details about
  // send failures.
  typedef base::Callback<void(const std::string& message_id)>
      OnMessageSentCallback;

  MCSClient(const base::FilePath& rmq_path,
            ConnectionFactory* connection_factory,
            scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  virtual ~MCSClient();

  // Initialize the client. Will load any previous id/token information as well
  // as unacknowledged message information from the RMQ storage, if it exists,
  // passing the id/token information back via |initialization_callback| along
  // with a |success == true| result. If no RMQ information is present (and
  // this is therefore a fresh client), a clean RMQ store will be created and
  // values of 0 will be returned via |initialization_callback| with
  // |success == true|.
  /// If an error loading the RMQ store is encountered,
  // |initialization_callback| will be invoked with |success == false|.
  void Initialize(const InitializationCompleteCallback& initialization_callback,
                  const OnMessageReceivedCallback& message_received_callback,
                  const OnMessageSentCallback& message_sent_callback);

  // Logs the client into the server. Client must be initialized.
  // |android_id| and |security_token| are optional if this is not a new
  // client, else they must be non-zero.
  // Successful login will result in |message_received_callback| being invoked
  // with a valid LoginResponse.
  // Login failure (typically invalid id/token) will shut down the client, and
  // |initialization_callback| to be invoked with |success = false|.
  void Login(uint64 android_id, uint64 security_token);

  // Sends a message, with or without reliable message queueing (RMQ) support.
  // Will asynchronously invoke the OnMessageSent callback regardless.
  // TODO(zea): support TTL.
  void SendMessage(const MCSMessage& message, bool use_rmq);

  // Disconnects the client and permanently destroys the persistent RMQ store.
  // WARNING: This is permanent, and the client must be recreated with new
  // credentials afterwards.
  void Destroy();

  // Returns the current state of the client.
  State state() const { return state_; }

 private:
  typedef uint32 StreamId;
  typedef std::string PersistentId;
  typedef std::vector<StreamId> StreamIdList;
  typedef std::vector<PersistentId> PersistentIdList;
  typedef std::map<StreamId, PersistentId> StreamIdToPersistentIdMap;
  typedef linked_ptr<ReliablePacketInfo> MCSPacketInternal;

  // Resets the internal state and builds a new login request, acknowledging
  // any pending server-to-device messages and rebuilding the send queue
  // from all unacknowledged device-to-server messages.
  // Should only be called when the connection has been reset.
  void ResetStateAndBuildLoginRequest(mcs_proto::LoginRequest* request);

  // Send a heartbeat to the MCS server.
  void SendHeartbeat();

  // RMQ Store callbacks.
  void OnRMQLoadFinished(const RMQStore::LoadResult& result);
  void OnRMQUpdateFinished(bool success);

  // Attempt to send a message.
  void MaybeSendMessage();

  // Helper for sending a protobuf along with any unacknowledged ids to the
  // wire.
  void SendPacketToWire(ReliablePacketInfo* packet_info);

  // Handle a data message sent to the MCS client system from the MCS server.
  void HandleMCSDataMesssage(
      scoped_ptr<google::protobuf::MessageLite> protobuf);

  // Handle a packet received over the wire.
  void HandlePacketFromWire(scoped_ptr<google::protobuf::MessageLite> protobuf);

  // ReliableMessageQueue acknowledgment helpers.
  // Handle a StreamAck sent by the server confirming receipt of all
  // messages up to the message with stream id |last_stream_id_received|.
  void HandleStreamAck(StreamId last_stream_id_received_);
  // Handle a SelectiveAck sent by the server confirming all messages
  // in |id_list|.
  void HandleSelectiveAck(const PersistentIdList& id_list);
  // Handle server confirmation of a device message, including device's
  // acknowledgment of receipt of messages.
  void HandleServerConfirmedReceipt(StreamId device_stream_id);

  // Generates a new persistent id for messages.
  // Virtual for testing.
  virtual PersistentId GetNextPersistentId();

  // Client state.
  State state_;

  // Callbacks for owner.
  InitializationCompleteCallback initialization_callback_;
  OnMessageReceivedCallback message_received_callback_;
  OnMessageSentCallback message_sent_callback_;

  // The android id and security token in use by this device.
  uint64 android_id_;
  uint64 security_token_;

  // Factory for creating new connections and connection handlers.
  ConnectionFactory* connection_factory_;

  // Connection handler to handle all over-the-wire protocol communication
  // with the mobile connection server.
  ConnectionHandler* connection_handler_;

  // -----  Reliablie Message Queue section -----
  // Note: all queues/maps are ordered from oldest (front/begin) message to
  // most recent (back/end).

  // Send/acknowledge queues.
  std::deque<MCSPacketInternal> to_send_;
  std::deque<MCSPacketInternal> to_resend_;

  // Last device_to_server stream id acknowledged by the server.
  StreamId last_device_to_server_stream_id_received_;
  // Last server_to_device stream id acknowledged by this device.
  StreamId last_server_to_device_stream_id_received_;
  // The stream id for the last sent message. A new message should consume
  // stream_id_out_ + 1.
  StreamId stream_id_out_;
  // The stream id of the last received message. The LoginResponse will always
  // have a stream id of 1, and stream ids increment by 1 for each received
  // message.
  StreamId stream_id_in_;

  // The server messages that have not been acked by the device yet. Keyed by
  // server stream id.
  StreamIdToPersistentIdMap unacked_server_ids_;

  // Those server messages that have been acked. They must remain tracked
  // until the ack message is itself confirmed. The list of all message ids
  // acknowledged are keyed off the device stream id of the message that
  // acknowledged them.
  std::map<StreamId, PersistentIdList> acked_server_ids_;

  // Those server messages from a previous connection that were not fully
  // acknowledged. They do not have associated stream ids, and will be
  // acknowledged on the next login attempt.
  PersistentIdList restored_unackeds_server_ids_;

  // The reliable message queue persistent store.
  RMQStore rmq_store_;

  // ----- Heartbeats -----
  // The current heartbeat interval.
  base::TimeDelta heartbeat_interval_;
  // Timer for triggering heartbeats.
  base::Timer heartbeat_timer_;

  // The task runner for blocking tasks (i.e. persisting RMQ state to disk).
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::WeakPtrFactory<MCSClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MCSClient);
};

} // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_MCS_CLIENT_H_
