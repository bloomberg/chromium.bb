// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_GCM_CLIENT_IMPL_H_
#define GOOGLE_APIS_GCM_GCM_CLIENT_IMPL_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/time/default_clock.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/gcm_client.h"
#include "google_apis/gcm/protocol/android_checkin.pb.h"
#include "net/base/net_log.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {
class HttpNetworkSession;
}

namespace gcm {

class CheckinRequest;
class ConnectionFactory;
class GCMClientImplTest;
class UserList;

// Implements the GCM Client. It is used to coordinate MCS Client (communication
// with MCS) and other pieces of GCM infrastructure like Registration and
// Checkins. It also allows for registering user delegates that host
// applications that send and receive messages.
class GCM_EXPORT GCMClientImpl : public GCMClient {
 public:
  GCMClientImpl();
  virtual ~GCMClientImpl();

  // Begins initialization of the GCM Client.
  void Initialize(
      const checkin_proto::ChromeBuildProto& chrome_build_proto,
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter);

  // Overridden from GCMClient:
  virtual void SetUserDelegate(const std::string& username,
                               Delegate* delegate) OVERRIDE;
  virtual void CheckIn(const std::string& username) OVERRIDE;
  virtual void Register(const std::string& username,
                        const std::string& app_id,
                        const std::string& cert,
                        const std::vector<std::string>& sender_ids) OVERRIDE;
  virtual void Unregister(const std::string& username,
                          const std::string& app_id) OVERRIDE;
  virtual void Send(const std::string& username,
                    const std::string& app_id,
                    const std::string& receiver_id,
                    const OutgoingMessage& message) OVERRIDE;
  virtual bool IsLoading() const OVERRIDE;

 private:
  // State representation of the GCMClient.
  enum State {
    // Uninitialized.
    UNINITIALIZED,
    // GCM store loading is in progress.
    LOADING,
    // Initial device checkin is in progress.
    INITIAL_DEVICE_CHECKIN,
    // Ready to accept requests.
    READY,
  };

  // Collection of pending checkin requests. Keys are serial numbers of the
  // users as assigned by the user_list_. Values are pending checkin requests to
  // obtain android IDs and security tokens for the users.
  typedef std::map<int64, CheckinRequest*> PendingCheckins;

  friend class GCMClientImplTest;

  // Callbacks for the MCSClient.
  // Receives messages and dispatches them to relevant user delegates.
  void OnMessageReceivedFromMCS(const gcm::MCSMessage& message);
  // Receives confirmation of sent messages or information about errors.
  void OnMessageSentToMCS(int64 user_serial_number,
                          const std::string& app_id,
                          const std::string& message_id,
                          MCSClient::MessageSendStatus status);
  // Receives information about mcs_client_ errors.
  void OnMCSError();

  // Runs after GCM Store load is done to trigger continuation of the
  // initialization.
  void OnLoadCompleted(scoped_ptr<GCMStore::LoadResult> result);
  // Initializes mcs_client_, which handles the connection to MCS.
  void InitializeMCSClient(scoped_ptr<GCMStore::LoadResult> result);
  // Complets the first time device checkin.
  void OnFirstTimeDeviceCheckinCompleted(const CheckinInfo& checkin_info);
  // Starts a login on mcs_client_.
  void StartMCSLogin();
  // Resets state to before initialization.
  void ResetState();

  // Startes a checkin request for a user with specified |serial_number|.
  // Checkin info can be invalid, in which case it is considered a first time
  // checkin.
  void StartCheckin(int64 user_serial_number,
                    const CheckinInfo& checkin_info);
  // Completes the checkin request for the specified |serial_number|.
  // |android_id| and |security_token| are expected to be non-zero or an error
  // is triggered. Function also cleans up the pending checkin.
  void OnCheckinCompleted(int64 user_serial_number,
                          uint64 android_id,
                          uint64 security_token);
  // Completes the checkin request for a device (serial number of 0).
  void OnDeviceCheckinCompleted(const CheckinInfo& checkin_info);

  // Callback for persisting device credentials in the |gcm_store_|.
  void SetDeviceCredentialsCallback(bool success);

  // Callback for setting a delegate on a |user_list_|. Informs that the
  // delegate with matching |username| was assigned a |user_serial_number|.
  void SetDelegateCompleted(const std::string& username,
                            int64 user_serial_number);

  // Handles incoming data message and dispatches it the a relevant user
  // delegate.
  void HandleIncomingMessage(const gcm::MCSMessage& message);

  // Fires OnMessageReceived event on |delegate|, with specified |app_id| and
  // |incoming_message|.
  void NotifyDelegateOnMessageReceived(GCMClient::Delegate* delegate,
                                       const std::string& app_id,
                                       const IncomingMessage& incoming_message);

  // State of the GCM Client Implementation.
  State state_;

  // Device checkin info (android ID and security token used by device).
  CheckinInfo device_checkin_info_;

  // Clock used for timing of retry logic.
  base::DefaultClock clock_;

  // Information about the chrome build.
  // TODO(fgorski): Check if it can be passed in constructor and made const.
  checkin_proto::ChromeBuildProto chrome_build_proto_;

  // Persistent data store for keeping device credentials, messages and user to
  // serial number mappings.
  scoped_ptr<GCMStore> gcm_store_;

  // Keeps the mappings of user's serial numbers and assigns new serial numbers
  // once a user delegate is added for the first time.
  scoped_ptr<UserList> user_list_;

  scoped_refptr<net::HttpNetworkSession> network_session_;
  net::BoundNetLog net_log_;
  scoped_ptr<ConnectionFactory> connection_factory_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // Controls receiving and sending of packets and reliable message queueing.
  scoped_ptr<MCSClient> mcs_client_;

  // Currently pending checkins. GCMClientImpl owns the CheckinRequests.
  PendingCheckins pending_checkins_;
  STLValueDeleter<PendingCheckins> pending_checkins_deleter_;

  DISALLOW_COPY_AND_ASSIGN(GCMClientImpl);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_GCM_CLIENT_IMPL_H_
