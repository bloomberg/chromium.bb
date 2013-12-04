// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_

#include <map>

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

class GCMEventRouter;
class GCMProfileServiceTest;

// Acts as a bridge between GCM API and GCMClient layer. It is profile based.
class GCMProfileService : public BrowserContextKeyedService,
                          public content::NotificationObserver {
 public:
  typedef base::Callback<void(const std::string& registration_id,
                              GCMClient::Result result)> RegisterCallback;
  typedef base::Callback<void(const std::string& message_id,
                              GCMClient::Result result)> SendCallback;

  // For testing purpose.
  class TestingDelegate {
   public:
    virtual GCMEventRouter* GetEventRouter() const = 0;
    virtual void CheckInFinished(const GCMClient::CheckInInfo& checkin_info,
                                 GCMClient::Result result) = 0;
    virtual void LoadingFromPersistentStoreFinished() = 0;
  };

  // Returns true if the GCM support is enabled.
  static bool IsGCMEnabled();

  // Register profile-specific prefs for GCM.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit GCMProfileService(Profile* profile);
  // Constructor for testing purpose.
  GCMProfileService(Profile* profile, TestingDelegate* testing_delegate);
  virtual ~GCMProfileService();

  // Registers |sender_id| for an app. A registration ID will be returned by
  // the GCM server.
  // |app_id|: application ID.
  // |cert|: SHA-1 of public key of the application, in base16 format.
  // |sender_ids|: list of IDs of the servers that are allowed to send the
  //               messages to the application. These IDs are assigned by the
  //               Google API Console.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void Register(const std::string& app_id,
                        const std::vector<std::string>& sender_ids,
                        const std::string& cert,
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

 protected:
  // Flag that could be set by the testing code to enable GCM. Otherwise,
  // tests from official build will fail.
  static bool enable_gcm_for_testing_;

 private:
  friend class GCMProfileServiceTest;
  friend class GCMProfileServiceRegisterTest;
  FRIEND_TEST_ALL_PREFIXES(GCMProfileServiceTest, CheckInFromPrefsStore);
  FRIEND_TEST_ALL_PREFIXES(GCMProfileServiceTest, CheckOut);
  FRIEND_TEST_ALL_PREFIXES(GCMProfileServiceRegisterTest, Unregister);

  class IOWorker;

  struct RegistrationInfo {
    RegistrationInfo();
    ~RegistrationInfo();

    std::vector<std::string> sender_ids;
    std::string registration_id;
  };

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void Init();

  // Allows a signed-in user to use the GCM. If the check-in info can be found
  // in the prefs store, use it directly. Otherwise, a check-in communication
  // will be made with the GCM.
  void AddUser();

  // Stops the user from using the GCM after the user signs out. This simply
  // removes the cached and persisted check-in info.
  void RemoveUser();

  // Unregisters an app from using the GCM after it has been uninstalled.
  void Unregister(const std::string& app_id);

  void CheckInFinished(GCMClient::CheckInInfo checkin_info,
                       GCMClient::Result result);
  void RegisterFinished(std::string app_id,
                        std::string registration_id,
                        GCMClient::Result result);
  void SendFinished(std::string app_id,
                    std::string message_id,
                    GCMClient::Result result);
  void MessageReceived(std::string app_id,
                       GCMClient::IncomingMessage message);
  void MessagesDeleted(std::string app_id);
  void MessageSendError(std::string app_id,
                        std::string message_id,
                        GCMClient::Result result);

  // Returns the event router to fire the event for the given app.
  GCMEventRouter* GetEventRouter(const std::string& app_id);

  // Used to persist registration info into the app's state store.
  void DeleteRegistrationInfo(const std::string& app_id);
  void WriteRegistrationInfo(const std::string& app_id);
  void ReadRegistrationInfo(const std::string& app_id);
  void ReadRegistrationInfoFinished(std::string app_id,
                                    scoped_ptr<base::Value> value);
  bool ParsePersistedRegistrationInfo(scoped_ptr<base::Value> value,
                                      RegistrationInfo* registration_info);

  // Returns the key used to identify the registration info saved into the
  // app's state store. Used for testing purpose.
  static const char* GetPersistentRegisterKeyForTesting();

  // The profile which owns this object.
  Profile* profile_;

  // The username of the signed-in profile.
  std::string username_;

  content::NotificationRegistrar registrar_;

  // For all the work occured in IO thread.
  scoped_refptr<IOWorker> io_worker_;

  // Callback map (from app_id to callback) for Register.
  std::map<std::string, RegisterCallback> register_callbacks_;

  // Callback map (from <app_id, message_id> to callback) for Send.
  std::map<std::pair<std::string, std::string>, SendCallback> send_callbacks_;

  // Map from app_id to registration info (sender ids & registration ID).
  typedef std::map<std::string, RegistrationInfo> RegistrationInfoMap;
  RegistrationInfoMap registration_info_map_;

  // Event router to talk with JS API.
  scoped_ptr<GCMEventRouter> js_event_router_;

  // For testing purpose.
  TestingDelegate* testing_delegate_;

  // Used to pass a weak pointer to the IO worker.
  base::WeakPtrFactory<GCMProfileService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMProfileService);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
