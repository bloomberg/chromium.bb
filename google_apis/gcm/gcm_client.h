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

namespace base {
class TaskRunner;
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
  };

  // The check-in info for the user. Returned by the server.
  struct GCM_EXPORT CheckInInfo {
    CheckInInfo() : android_id(0), secret(0) {}
    bool IsValid() const { return android_id != 0 && secret != 0; }
    void Reset() {
      android_id = 0;
      secret = 0;
    }

    uint64 android_id;
    uint64 secret;
  };

  // A delegate interface that allows the GCMClient instance to interact with
  // its caller, i.e. notifying asynchronous event.
  class Delegate {
   public:
    // Called when the user has been checked in successfully or an error occurs.
    // |checkin_info|: valid if the checkin completed successfully.
    // |result|: the type of the error if an error occured, success otherwise.
    virtual void OnCheckInFinished(const CheckInInfo& checkin_info,
                                   Result result) = 0;

    // Called when the registration completed successfully or an error occurs.
    // |app_id|: application ID.
    // |registration_id|: non-empty if the registration completed successfully.
    // |result|: the type of the error if an error occured, success otherwise.
    virtual void OnRegisterFinished(const std::string& app_id,
                                    const std::string& registration_id,
                                    Result result) = 0;

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

    // Returns the checkin info associated with this user. The delegate class
    // is expected to persist the checkin info that is provided by
    // OnCheckInFinished.
    virtual CheckInInfo GetCheckInInfo() const = 0;

    // Called when the loading from the persistent store is done. The loading
    // is triggered asynchronously when GCMClient is created.
    virtual void OnLoadingCompleted() = 0;

    // Returns a task runner for file operations that may block. This is used
    // in writing to or reading from the persistent store.
    virtual base::TaskRunner* GetFileTaskRunner() = 0;
  };

  // Returns the single instance. Multiple profiles share the same client
  // that makes use of the same MCS connection.
  static GCMClient* Get();

  // Passes a mocked instance for testing purpose.
  static void SetForTesting(GCMClient* client);

  // Checks in the user to use GCM. If the device has not been checked in, it
  // will be done first.
  // |username|: the username (email address) used to check in with the server.
  // |delegate|: the delegate whose methods will be called asynchronously in
  //             response to events and messages.
  virtual void CheckIn(const std::string& username, Delegate* delegate) = 0;

  // Registers the application for GCM. Delegate::OnRegisterFinished will be
  // called asynchronously upon completion.
  // |username|: the username (email address) passed in CheckIn.
  // |app_id|: application ID.
  // |cert|: SHA-1 of public key of the application, in base16 format.
  // |sender_ids|: list of IDs of the servers that are allowed to send the
  //               messages to the application. These IDs are assigned by the
  //               Google API Console.
  virtual void Register(const std::string& username,
                        const std::string& app_id,
                        const std::string& cert,
                        const std::vector<std::string>& sender_ids) = 0;

  // Unregisters the application from GCM when it is uninstalled.
  // Delegate::OnUnregisterFinished will be called asynchronously upon
  // completion.
  // |username|: the username (email address) passed in CheckIn.
  // |app_id|: application ID.
  virtual void Unregister(const std::string& username,
                          const std::string& app_id) = 0;

  // Sends a message to a given receiver. Delegate::OnSendFinished will be
  // called asynchronously upon completion.
  // |username|: the username (email address) passed in CheckIn.
  // |app_id|: application ID.
  // |receiver_id|: registration ID of the receiver party.
  // |message|: message to be sent.
  virtual void Send(const std::string& username,
                    const std::string& app_id,
                    const std::string& receiver_id,
                    const OutgoingMessage& message) = 0;

  // Returns true if the loading from the persistent store is still in progress.
  virtual bool IsLoading() const = 0;

 protected:
  virtual ~GCMClient() {}
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_GCM_CLIENT_H_
