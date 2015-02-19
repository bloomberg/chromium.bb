// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DRIVER_H_
#define COMPONENTS_GCM_DRIVER_GCM_DRIVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/gcm_driver/default_gcm_app_handler.h"
#include "components/gcm_driver/gcm_client.h"

namespace gcm {

class GCMAppHandler;
class GCMConnectionObserver;
struct AccountMapping;

// Bridge between GCM users in Chrome and the platform-specific implementation.
class GCMDriver {
 public:
  typedef std::map<std::string, GCMAppHandler*> GCMAppHandlerMap;
  typedef base::Callback<void(const std::string& registration_id,
                              GCMClient::Result result)> RegisterCallback;
  typedef base::Callback<void(const std::string& message_id,
                              GCMClient::Result result)> SendCallback;
  typedef base::Callback<void(GCMClient::Result result)> UnregisterCallback;
  typedef base::Callback<void(const GCMClient::GCMStatistics& stats)>
      GetGCMStatisticsCallback;

  GCMDriver();
  virtual ~GCMDriver();

  // Registers |sender_ids| for an app. A registration ID will be returned by
  // the GCM server. On Android, only a single sender ID is supported, but
  // instead multiple simultaneous registrations are allowed.
  // |app_id|: application ID.
  // |sender_ids|: list of IDs of the servers that are allowed to send the
  //               messages to the application. These IDs are assigned by the
  //               Google API Console.
  // |callback|: to be called once the asynchronous operation is done.
  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids,
                const RegisterCallback& callback);

  // Unregisters all sender_ids for an app. Only works on non-Android.
  // |app_id|: application ID.
  // |callback|: to be called once the asynchronous operation is done.
  void Unregister(const std::string& app_id,
                  const UnregisterCallback& callback);

  // Unregisters an (app_id, sender_id) pair from using GCM. Only works on
  // Android.
  // TODO(jianli): Switch to using GCM's unsubscribe API.
  // |app_id|: application ID.
  // |sender_id|: the sender ID that was passed when registering.
  // |callback|: to be called once the asynchronous operation is done.
  void UnregisterWithSenderId(const std::string& app_id,
                              const std::string& sender_id,
                              const UnregisterCallback& callback);

  // Sends a message to a given receiver.
  // |app_id|: application ID.
  // |receiver_id|: registration ID of the receiver party.
  // |message|: message to be sent.
  // |callback|: to be called once the asynchronous operation is done.
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const GCMClient::OutgoingMessage& message,
            const SendCallback& callback);

  const GCMAppHandlerMap& app_handlers() const { return app_handlers_; }

  // This method must be called before destroying the GCMDriver. Once it has
  // been called, no other GCMDriver methods may be used.
  virtual void Shutdown();

  // Called when the user signs in to or out of a GAIA account.
  virtual void OnSignedIn() = 0;
  virtual void OnSignedOut() = 0;

  // Adds a handler for a given app.
  virtual void AddAppHandler(const std::string& app_id, GCMAppHandler* handler);

  // Remove the handler for a given app.
  virtual void RemoveAppHandler(const std::string& app_id);

  // Returns the handler for the given app.
  GCMAppHandler* GetAppHandler(const std::string& app_id);

  // Adds a connection state observer.
  virtual void AddConnectionObserver(GCMConnectionObserver* observer) = 0;

  // Removes a connection state observer.
  virtual void RemoveConnectionObserver(GCMConnectionObserver* observer) = 0;

  // Enables/disables GCM service.
  virtual void Enable() = 0;
  virtual void Disable() = 0;

  // For testing purpose. Always NULL on Android.
  virtual GCMClient* GetGCMClientForTesting() const = 0;

  // Returns true if the service was started.
  virtual bool IsStarted() const = 0;

  // Returns true if the gcm client has an open and active connection.
  virtual bool IsConnected() const = 0;

  // Get GCM client internal states and statistics.
  // If clear_logs is true then activity logs will be cleared before the stats
  // are returned.
  virtual void GetGCMStatistics(const GetGCMStatisticsCallback& callback,
                                bool clear_logs) = 0;

  // Enables/disables GCM activity recording, and then returns the stats.
  virtual void SetGCMRecording(const GetGCMStatisticsCallback& callback,
                               bool recording) = 0;

  // sets a list of signed in accounts with OAuth2 access tokens, when GCMDriver
  // works in context of a signed in entity (e.g. browser profile where user is
  // signed into sync).
  // |account_tokens|: list of email addresses, account IDs and OAuth2 access
  //                   tokens.
  virtual void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens) = 0;

  // Updates the |account_mapping| information in persistent store.
  virtual void UpdateAccountMapping(const AccountMapping& account_mapping) = 0;

  // Removes the account mapping information reated to |account_id| from
  // persistent store.
  virtual void RemoveAccountMapping(const std::string& account_id) = 0;

  // Getter and setter of last token fetch time.
  virtual base::Time GetLastTokenFetchTime() = 0;
  virtual void SetLastTokenFetchTime(const base::Time& time) = 0;

  // Sets whether or not GCM should try to wake the system from suspend in order
  // to send a heartbeat message.
  virtual void WakeFromSuspendForHeartbeat(bool wake) = 0;

 protected:
  // Ensures that the GCM service starts (if necessary conditions are met).
  virtual GCMClient::Result EnsureStarted(GCMClient::StartMode start_mode) = 0;

  // Platform-specific implementation of Register.
  virtual void RegisterImpl(const std::string& app_id,
                            const std::vector<std::string>& sender_ids) = 0;

  // Platform-specific implementation of Unregister.
  virtual void UnregisterImpl(const std::string& app_id) = 0;

  // Platform-specific implementation of UnregisterWithSenderId.
  virtual void UnregisterWithSenderIdImpl(const std::string& app_id,
                                          const std::string& sender_id);

  // Platform-specific implementation of Send.
  virtual void SendImpl(const std::string& app_id,
                        const std::string& receiver_id,
                        const GCMClient::OutgoingMessage& message) = 0;

  // Runs the Register callback.
  void RegisterFinished(const std::string& app_id,
                        const std::string& registration_id,
                        GCMClient::Result result);

  // Runs the Unregister callback.
  void UnregisterFinished(const std::string& app_id,
                          GCMClient::Result result);

  // Runs the Send callback.
  void SendFinished(const std::string& app_id,
                    const std::string& message_id,
                    GCMClient::Result result);

  bool HasRegisterCallback(const std::string& app_id);

  void ClearCallbacks();

 private:
  // Common code shared by Unregister and UnregisterWithSenderId.
  void UnregisterInternal(const std::string& app_id,
                          const std::string* sender_id,
                          const UnregisterCallback& callback);

  // Called after unregistration completes in order to trigger the pending
  // registration.
  void RegisterAfterUnregister(
      const std::string& app_id,
      const std::vector<std::string>& normalized_sender_ids,
      const UnregisterCallback& unregister_callback,
      GCMClient::Result result);

  // Callback map (from app_id to callback) for Register.
  std::map<std::string, RegisterCallback> register_callbacks_;

  // Callback map (from app_id to callback) for Unregister.
  std::map<std::string, UnregisterCallback> unregister_callbacks_;

  // Callback map (from <app_id, message_id> to callback) for Send.
  std::map<std::pair<std::string, std::string>, SendCallback> send_callbacks_;

  // App handler map (from app_id to handler pointer).
  // The handler is not owned.
  GCMAppHandlerMap app_handlers_;

  // The default handler when no app handler can be found in the map.
  DefaultGCMAppHandler default_app_handler_;

  base::WeakPtrFactory<GCMDriver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMDriver);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_H_
