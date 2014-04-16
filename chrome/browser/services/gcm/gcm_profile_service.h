// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/services/gcm/default_gcm_app_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gcm/gcm_client.h"

class Profile;

namespace base {
class Value;
}

namespace extensions {
class ExtensionGCMAppHandlerTest;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace gcm {

class GCMAppHandler;
class GCMClientFactory;
class GCMProfileServiceTestConsumer;

// Acts as a bridge between GCM API and GCMClient layer. It is profile based.
class GCMProfileService : public KeyedService,
                          public content::NotificationObserver,
                          public SigninManagerBase::Observer {
 public:
  typedef base::Callback<void(const std::string& registration_id,
                              GCMClient::Result result)> RegisterCallback;
  typedef base::Callback<void(const std::string& message_id,
                              GCMClient::Result result)> SendCallback;
  typedef base::Callback<void(GCMClient::Result result)> UnregisterCallback;
  typedef base::Callback<void(const GCMClient::GCMStatistics& stats)>
      RequestGCMStatisticsCallback;

  // Any change made to this enum should have corresponding change in the
  // GetGCMEnabledStateString(...) function.
  enum GCMEnabledState {
    // GCM is always enabled. GCMClient will always load and connect with GCM.
    ALWAYS_ENABLED,
    // GCM is only enabled for apps. GCMClient will start to load and connect
    // with GCM only when GCM API is used.
    ENABLED_FOR_APPS,
    // GCM is always disabled. GCMClient will never load and connect with GCM.
    ALWAYS_DISABLED
  };

  // Returns the GCM enabled state.
  static GCMEnabledState GetGCMEnabledState(Profile* profile);

  // Returns text representation of a GCMEnabledState enum entry.
  static std::string GetGCMEnabledStateString(GCMEnabledState state);

  // Register profile-specific prefs for GCM.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit GCMProfileService(Profile* profile);
  virtual ~GCMProfileService();

  void Initialize(scoped_ptr<GCMClientFactory> gcm_client_factory);

  void Start();

  void Stop();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Adds a handler for a given app.
  virtual void AddAppHandler(const std::string& app_id, GCMAppHandler* handler);

  // Remove the handler for a given app.
  virtual void RemoveAppHandler(const std::string& app_id);

  // Registers |sender_id| for an app. A registration ID will be returned by
  // the GCM server.
  // |app_id|: application ID.
  // |sender_ids|: list of IDs of the servers that are allowed to send the
  //               messages to the application. These IDs are assigned by the
  //               Google API Console.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void Register(const std::string& app_id,
                        const std::vector<std::string>& sender_ids,
                        RegisterCallback callback);

  // Unregisters an app from using GCM.
  // |app_id|: application ID.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void Unregister(const std::string& app_id,
                          UnregisterCallback callback);

  // Sends a message to a given receiver.
  // |app_id|: application ID.
  // |receiver_id|: registration ID of the receiver party.
  // |message|: message to be sent.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const GCMClient::OutgoingMessage& message,
                    SendCallback callback);

  // For testing purpose.
  GCMClient* GetGCMClientForTesting() const;

  // Returns the user name if the profile is signed in.
  std::string SignedInUserName() const;

  // Returns true if the gcm client is ready.
  bool IsGCMClientReady() const;

  // Get GCM client internal states and statistics. If it has not been created
  // then stats won't be modified.
  void RequestGCMStatistics(RequestGCMStatisticsCallback callback);

 private:
  friend class GCMProfileServiceTestConsumer;
  friend class extensions::ExtensionGCMAppHandlerTest;

  class DelayedTaskController;
  class IOWorker;

  typedef std::map<std::string, GCMAppHandler*> GCMAppHandlerMap;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from SigninManagerBase::Observer:
  virtual void GoogleSigninSucceeded(const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void GoogleSignedOut(const std::string& username) OVERRIDE;

  // Ensures that the GCMClient is loaded and the GCM check-in is done when
  // the profile was signed in.
  void EnsureLoaded();

  // Remove cached data when GCM service is stopped.
  void RemoveCachedData();

  // Checks out of GCM when the profile has been signed out. This will erase
  // all the cached and persisted data.
  void CheckOut();

  // Resets the GCMClient instance. This is called when the profile is being
  // destroyed.
  void ResetGCMClient();

  // Ensures that the app is ready for GCM functions and events.
  GCMClient::Result EnsureAppReady(const std::string& app_id);

  // Should be called when an app with |app_id| is trying to un/register.
  // Checks whether another un/registration is in progress.
  bool IsAsyncOperationPending(const std::string& app_id) const;

  void DoRegister(const std::string& app_id,
                  const std::vector<std::string>& sender_ids);
  void DoUnregister(const std::string& app_id);
  void DoSend(const std::string& app_id,
              const std::string& receiver_id,
              const GCMClient::OutgoingMessage& message);

  // Callbacks posted from IO thread to UI thread.
  void RegisterFinished(const std::string& app_id,
                        const std::string& registration_id,
                        GCMClient::Result result);
  void UnregisterFinished(const std::string& app_id, GCMClient::Result result);
  void SendFinished(const std::string& app_id,
                    const std::string& message_id,
                    GCMClient::Result result);
  void MessageReceived(const std::string& app_id,
                       GCMClient::IncomingMessage message);
  void MessagesDeleted(const std::string& app_id);
  void MessageSendError(const std::string& app_id,
                        const GCMClient::SendErrorDetails& send_error_details);
  void GCMClientReady();

  // Returns the handler for the given app.
  GCMAppHandler* GetAppHandler(const std::string& app_id);

  void RequestGCMStatisticsFinished(GCMClient::GCMStatistics stats);

  // The profile which owns this object.
  Profile* profile_;

  // Flag to indicate if GCMClient is ready.
  bool gcm_client_ready_;

  // The username of the signed-in profile.
  std::string username_;

  content::NotificationRegistrar registrar_;

  scoped_ptr<DelayedTaskController> delayed_task_controller_;

  // For all the work occured in IO thread.
  scoped_refptr<IOWorker> io_worker_;

  // App handler map (from app_id to handler pointer).
  // The handler is not owned.
  GCMAppHandlerMap app_handlers_;

  // The default handler when no app handler can be found in the map.
  DefaultGCMAppHandler default_app_handler_;

  // Callback map (from app_id to callback) for Register.
  std::map<std::string, RegisterCallback> register_callbacks_;

  // Callback map (from app_id to callback) for Unregister.
  std::map<std::string, UnregisterCallback> unregister_callbacks_;

  // Callback map (from <app_id, message_id> to callback) for Send.
  std::map<std::pair<std::string, std::string>, SendCallback> send_callbacks_;

  // Callback for RequestGCMStatistics.
  RequestGCMStatisticsCallback request_gcm_statistics_callback_;

  // Used to pass a weak pointer to the IO worker.
  base::WeakPtrFactory<GCMProfileService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMProfileService);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
