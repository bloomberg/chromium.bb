// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DRIVER_H_
#define COMPONENTS_GCM_DRIVER_GCM_DRIVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/default_gcm_app_handler.h"
#include "google_apis/gaia/identity_provider.h"
#include "google_apis/gcm/gcm_client.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace extensions {
class ExtensionGCMAppHandlerTest;
}

namespace net {
class URLRequestContextGetter;
}

namespace gcm {

class GCMAppHandler;
class GCMClientFactory;

// A bridge between the GCM users in Chrome and the GCMClient layer.
class GCMDriver : public IdentityProvider::Observer {
 public:
  typedef std::map<std::string, GCMAppHandler*> GCMAppHandlerMap;
  typedef base::Callback<void(const std::string& registration_id,
                              GCMClient::Result result)> RegisterCallback;
  typedef base::Callback<void(const std::string& message_id,
                              GCMClient::Result result)> SendCallback;
  typedef base::Callback<void(GCMClient::Result result)> UnregisterCallback;
  typedef base::Callback<void(const GCMClient::GCMStatistics& stats)>
      GetGCMStatisticsCallback;

  GCMDriver(
      scoped_ptr<GCMClientFactory> gcm_client_factory,
      scoped_ptr<IdentityProvider> identity_provider,
      const GCMClient::ChromeBuildInfo& chrome_build_info,
      const base::FilePath& store_path,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
      const scoped_refptr<base::SequencedTaskRunner>& io_thread,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);
  virtual ~GCMDriver();

  // Enables/disables GCM service.
  void Enable();
  void Disable();

  // This method must be called before destroying the GCMDriver. Once it has
  // been called, no other GCMDriver methods may be used.
  virtual void Shutdown();

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
                        const RegisterCallback& callback);

  // Unregisters an app from using GCM.
  // |app_id|: application ID.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void Unregister(const std::string& app_id,
                          const UnregisterCallback& callback);

  // Sends a message to a given receiver.
  // |app_id|: application ID.
  // |receiver_id|: registration ID of the receiver party.
  // |message|: message to be sent.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const GCMClient::OutgoingMessage& message,
                    const SendCallback& callback);

  // For testing purpose.
  GCMClient* GetGCMClientForTesting() const;

  // Returns true if the service was started.
  bool IsStarted() const;

  // Returns true if the gcm client is ready.
  bool IsGCMClientReady() const;

  // Get GCM client internal states and statistics.
  // If clear_logs is true then activity logs will be cleared before the stats
  // are returned.
  void GetGCMStatistics(const GetGCMStatisticsCallback& callback,
                        bool clear_logs);

  // Enables/disables GCM activity recording, and then returns the stats.
  void SetGCMRecording(const GetGCMStatisticsCallback& callback,
                       bool recording);

  // Returns the user name if the profile is signed in. Empty string otherwise.
  std::string SignedInUserName() const;

  // IdentityProvider::Observer:
  virtual void OnActiveAccountLogin() OVERRIDE;
  virtual void OnActiveAccountLogout() OVERRIDE;

  const GCMAppHandlerMap& app_handlers() const { return app_handlers_; }

 protected:
  // Used for constructing fake GCMDriver for testing purpose.
  GCMDriver();

 private:
  class DelayedTaskController;
  class IOWorker;

  // Ensures that the GCM service starts when all of the following conditions
  // satisfy:
  // 1) GCM is enabled.
  // 2) The identity provider is able to supply an account ID.
  GCMClient::Result EnsureStarted();

  //  Stops the GCM service. It can be restarted by calling EnsureStarted again.
  void Stop();

  // Remove cached data when GCM service is stopped.
  void RemoveCachedData();

  // Checks out of GCM and erases any cached and persisted data.
  void CheckOut();

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

  void GetGCMStatisticsFinished(GCMClient::GCMStatistics stats);

  // Flag to indicate if GCM is enabled.
  bool gcm_enabled_;

  // Flag to indicate if GCMClient is ready.
  bool gcm_client_ready_;

  // The account ID that this service is responsible for. Empty when the service
  // is not running.
  std::string account_id_;

  scoped_ptr<IdentityProvider> identity_provider_;
  scoped_refptr<base::SequencedTaskRunner> ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> io_thread_;

  scoped_ptr<DelayedTaskController> delayed_task_controller_;

  // For all the work occurring on the IO thread. Must be destroyed on the IO
  // thread.
  scoped_ptr<IOWorker> io_worker_;

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

  // Callback for GetGCMStatistics.
  GetGCMStatisticsCallback request_gcm_statistics_callback_;

  // Used to pass a weak pointer to the IO worker.
  base::WeakPtrFactory<GCMDriver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMDriver);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_H_
