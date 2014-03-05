// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_GCM_CLIENT_H_
#define GOOGLE_APIS_GCM_GCM_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "google_apis/gcm/base/gcm_export.h"

template <class T> class scoped_refptr;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace checkin_proto {
class ChromeBuildProto;
}

namespace net {
class URLRequestContextGetter;
}

namespace gcm {

// Interface that encapsulates the network communications with the Google Cloud
// Messaging server. This interface is not supposed to be thread-safe.
class GCM_EXPORT GCMClient {
 public:
  enum Result {
    // Successful operation.
    SUCCESS,
    // Invalid parameter.
    INVALID_PARAMETER,
    // Profile not signed in.
    NOT_SIGNED_IN,
    // Previous asynchronous operation is still pending to finish. Certain
    // operation, like register, is only allowed one at a time.
    ASYNC_OPERATION_PENDING,
    // Network socket error.
    NETWORK_ERROR,
    // Problem at the server.
    SERVER_ERROR,
    // Exceeded the specified TTL during message sending.
    TTL_EXCEEDED,
    // Other errors.
    UNKNOWN_ERROR
  };

  // Message data consisting of key-value pairs.
  typedef std::map<std::string, std::string> MessageData;

  // Message to be delivered to the other party.
  struct GCM_EXPORT OutgoingMessage {
    OutgoingMessage();
    ~OutgoingMessage();

    // Message ID.
    std::string id;
    // In seconds.
    int time_to_live;
    MessageData data;
  };

  // Message being received from the other party.
  struct GCM_EXPORT IncomingMessage {
    IncomingMessage();
    ~IncomingMessage();

    MessageData data;
    std::string collapse_key;
  };

  // A delegate interface that allows the GCMClient instance to interact with
  // its caller, i.e. notifying asynchronous event.
  class Delegate {
   public:
    // Called when the registration completed successfully or an error occurs.
    // |app_id|: application ID.
    // |registration_id|: non-empty if the registration completed successfully.
    // |result|: the type of the error if an error occured, success otherwise.
    virtual void OnRegisterFinished(const std::string& app_id,
                                    const std::string& registration_id,
                                    Result result) = 0;

    // Called when the unregistration completed.
    // |app_id|: application ID.
    // |success|: indicates whether unregistration request was successful.
    virtual void OnUnregisterFinished(const std::string& app_id,
                                      bool success) = 0;

    // Called when the message is scheduled to send successfully or an error
    // occurs.
    // |app_id|: application ID.
    // |message_id|: ID of the message being sent.
    // |result|: the type of the error if an error occured, success otherwise.
    virtual void OnSendFinished(const std::string& app_id,
                                const std::string& message_id,
                                Result result) = 0;

    // Called when a message has been received.
    // |app_id|: application ID.
    // |message|: message received.
    virtual void OnMessageReceived(const std::string& app_id,
                                   const IncomingMessage& message) = 0;

    // Called when some messages have been deleted from the server.
    // |app_id|: application ID.
    virtual void OnMessagesDeleted(const std::string& app_id) = 0;

    // Called when a message failed to send to the server.
    // |app_id|: application ID.
    // |message_id|: ID of the message being sent.
    // |result|: the type of the error if an error occured, success otherwise.
    virtual void OnMessageSendError(const std::string& app_id,
                                    const std::string& message_id,
                                    Result result) = 0;

    // Called when the GCM becomes ready. To get to this state, GCMClient
    // finished loading from the GCM store and retrieved the device check-in
    // from the server if it hadn't yet.
    virtual void OnGCMReady() = 0;
  };

  GCMClient();
  virtual ~GCMClient();

  // Begins initialization of the GCM Client. This will not trigger a
  // connection.
  // |chrome_build_proto|: chrome info, i.e., version, channel and etc.
  // |store_path|: path to the GCM store.
  // |account_ids|: account IDs to be related to the device when checking in.
  // |blocking_task_runner|: for running blocking file tasks.
  // |url_request_context_getter|: for url requests.
  // |delegate|: the delegate whose methods will be called asynchronously in
  //             response to events and messages.
  virtual void Initialize(
      const checkin_proto::ChromeBuildProto& chrome_build_proto,
      const base::FilePath& store_path,
      const std::vector<std::string>& account_ids,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      Delegate* delegate) = 0;

  // Loads the data from the persistent store. This will automatically kick off
  // the check-in if the check-in info is not found in the store.
  // TODO(jianli): consider renaming this name to Start.
  virtual void Load() = 0;

  // Stops using the GCM service. This will not erase the persisted data.
  virtual void Stop() = 0;

  // Checks out of the GCM service. This will erase all the cached and persisted
  // data.
  virtual void CheckOut() = 0;

  // Registers the application for GCM. Delegate::OnRegisterFinished will be
  // called asynchronously upon completion.
  // |app_id|: application ID.
  // |sender_ids|: list of IDs of the servers that are allowed to send the
  //               messages to the application. These IDs are assigned by the
  //               Google API Console.
  virtual void Register(const std::string& app_id,
                        const std::vector<std::string>& sender_ids) = 0;

  // Unregisters the application from GCM when it is uninstalled.
  // Delegate::OnUnregisterFinished will be called asynchronously upon
  // completion.
  // |app_id|: application ID.
  virtual void Unregister(const std::string& app_id) = 0;

  // Sends a message to a given receiver. Delegate::OnSendFinished will be
  // called asynchronously upon completion.
  // |app_id|: application ID.
  // |receiver_id|: registration ID of the receiver party.
  // |message|: message to be sent.
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const OutgoingMessage& message) = 0;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_GCM_CLIENT_H_
