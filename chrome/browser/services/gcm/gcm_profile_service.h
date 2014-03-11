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
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gcm/gcm_client.h"

class Profile;

namespace base {
class Value;
}

namespace extensions {
class Extension;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace gcm {

class GCMClientFactory;
class GCMEventRouter;
class GCMProfileServiceTestConsumer;

// Acts as a bridge between GCM API and GCMClient layer. It is profile based.
class GCMProfileService : public BrowserContextKeyedService,
                          public content::NotificationObserver {
 public:
  typedef base::Callback<void(const std::string& registration_id,
                              GCMClient::Result result)> RegisterCallback;
  typedef base::Callback<void(const std::string& message_id,
                              GCMClient::Result result)> SendCallback;
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

  // For testing purpose.
  class TestingDelegate {
   public:
    virtual GCMEventRouter* GetEventRouter() const = 0;
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

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

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

  void set_testing_delegate(TestingDelegate* testing_delegate) {
    testing_delegate_ = testing_delegate;
  }

  // Returns the user name if the profile is signed in.
  std::string SignedInUserName() const;

  // Returns true if the gcm client is ready.
  bool IsGCMClientReady() const;

  // Get GCM client internal states and statistics. If it has not been created
  // then stats won't be modified.
  void RequestGCMStatistics(RequestGCMStatisticsCallback callback);

 private:
  friend class GCMProfileServiceTestConsumer;

  class DelayedTaskController;
  class IOWorker;

  struct RegistrationInfo {
    RegistrationInfo();
    ~RegistrationInfo();
    bool IsValid() const;

    std::vector<std::string> sender_ids;
    std::string registration_id;
  };

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Ensures that the GCMClient is loaded and the GCM check-in is done when
  // the profile was signed in.
  void EnsureLoaded();

  // Remove cached or persisted data when GCM service is stopped.
  void RemoveCachedData();
  void RemovePersistedData();

  // Checks out of GCM when the profile has been signed out. This will erase
  // all the cached and persisted data.
  void CheckOut();

  // Resets the GCMClient instance. This is called when the profile is being
  // destroyed.
  void ResetGCMClient();

  // Ensures that the app is ready for GCM functions and events.
  void EnsureAppReady(const std::string& app_id);

  // Unregisters an app from using the GCM after it has been uninstalled.
  void Unregister(const std::string& app_id);

  void DoRegister(const std::string& app_id,
                  const std::vector<std::string>& sender_ids);
  void DoSend(const std::string& app_id,
              const std::string& receiver_id,
              const GCMClient::OutgoingMessage& message);

  // Callbacks posted from IO thread to UI thread.
  void RegisterFinished(const std::string& app_id,
                        const std::string& registration_id,
                        GCMClient::Result result);
  void SendFinished(const std::string& app_id,
                    const std::string& message_id,
                    GCMClient::Result result);
  void MessageReceived(const std::string& app_id,
                       GCMClient::IncomingMessage message);
  void MessagesDeleted(const std::string& app_id);
  void MessageSendError(const std::string& app_id,
                        const GCMClient::SendErrorDetails& send_error_details);
  void GCMClientReady();

  // Returns the event router to fire the event for the given app.
  GCMEventRouter* GetEventRouter(const std::string& app_id) const;

  // Used to persist the IDs of registered apps.
  void ReadRegisteredAppIDs();
  void WriteRegisteredAppIDs();

  // Used to persist registration info into the app's state store.
  void DeleteRegistrationInfo(const std::string& app_id);
  void WriteRegistrationInfo(const std::string& app_id);
  void ReadRegistrationInfo(const std::string& app_id);
  void ReadRegistrationInfoFinished(const std::string& app_id,
                                    scoped_ptr<base::Value> value);
  bool ParsePersistedRegistrationInfo(scoped_ptr<base::Value> value,
                                      RegistrationInfo* registration_info);
  void RequestGCMStatisticsFinished(GCMClient::GCMStatistics stats);

  // Returns the key used to identify the registration info saved into the
  // app's state store. Used for testing purpose.
  static const char* GetPersistentRegisterKeyForTesting();

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

  // Callback map (from app_id to callback) for Register.
  std::map<std::string, RegisterCallback> register_callbacks_;

  // Callback map (from <app_id, message_id> to callback) for Send.
  std::map<std::pair<std::string, std::string>, SendCallback> send_callbacks_;

  // Callback for RequestGCMStatistics.
  RequestGCMStatisticsCallback request_gcm_statistics_callback_;

  // Map from app_id to registration info (sender ids & registration ID).
  typedef std::map<std::string, RegistrationInfo> RegistrationInfoMap;
  RegistrationInfoMap registration_info_map_;

  // Event router to talk with JS API.
#if !defined(OS_ANDROID)
  scoped_ptr<GCMEventRouter> js_event_router_;
#endif

  // For testing purpose.
  TestingDelegate* testing_delegate_;

  // Used to pass a weak pointer to the IO worker.
  base::WeakPtrFactory<GCMProfileService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMProfileService);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
