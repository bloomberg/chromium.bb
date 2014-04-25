// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_SERVICE_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/services/gcm/default_gcm_app_handler.h"
#include "google_apis/gaia/identity_provider.h"
#include "google_apis/gcm/gcm_client.h"

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
class GCMService : public IdentityProvider::Observer {
 public:
  typedef base::Callback<void(const std::string& registration_id,
                              GCMClient::Result result)> RegisterCallback;
  typedef base::Callback<void(const std::string& message_id,
                              GCMClient::Result result)> SendCallback;
  typedef base::Callback<void(GCMClient::Result result)> UnregisterCallback;
  typedef base::Callback<void(const GCMClient::GCMStatistics& stats)>
      GetGCMStatisticsCallback;

  explicit GCMService(scoped_ptr<IdentityProvider> identity_provider);
  virtual ~GCMService();

  void Initialize(scoped_ptr<GCMClientFactory> gcm_client_factory);

  void Start();

  void Stop();

  // This method must be called before destroying the GCMService. Once it has
  // been called, no other GCMService methods may be used.
  void ShutdownService();

  // Adds a handler for a given app.
  void AddAppHandler(const std::string& app_id, GCMAppHandler* handler);

  // Remove the handler for a given app.
  void RemoveAppHandler(const std::string& app_id);

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

  // Returns true if the service was started.
  bool IsStarted() const;

  // Returns true if the gcm client is ready.
  bool IsGCMClientReady() const;

  // Get GCM client internal states and statistics.
  // If clear_logs is true then activity logs will be cleared before the stats
  // are returned.
  void GetGCMStatistics(GetGCMStatisticsCallback callback, bool clear_logs);

  // Enables/disables GCM activity recording, and then returns the stats.
  void SetGCMRecording(GetGCMStatisticsCallback callback, bool recording);

  // IdentityProvider::Observer:
  virtual void OnActiveAccountLogin() OVERRIDE;
  virtual void OnActiveAccountLogout() OVERRIDE;

 protected:
  virtual bool ShouldStartAutomatically() const = 0;

  virtual base::FilePath GetStorePath() const = 0;

  virtual scoped_refptr<net::URLRequestContextGetter>
      GetURLRequestContextGetter() const = 0;

  scoped_ptr<IdentityProvider> identity_provider_;

 private:
  friend class TestGCMServiceWrapper;
  friend class extensions::ExtensionGCMAppHandlerTest;

  class DelayedTaskController;
  class IOWorker;

  typedef std::map<std::string, GCMAppHandler*> GCMAppHandlerMap;

  // Ensures that the GCMClient is loaded and the GCM check-in is done if the
  // |identity_provider_| is able to supply an account ID.
  void EnsureLoaded();

  // Remove cached data when GCM service is stopped.
  void RemoveCachedData();

  // Checks out of GCM and erases any cached and persisted data.
  void CheckOut();

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

  void GetGCMStatisticsFinished(GCMClient::GCMStatistics stats);

  // Flag to indicate if GCMClient is ready.
  bool gcm_client_ready_;

  // The account ID that this service is responsible for. Empty when the service
  // is not running.
  std::string account_id_;

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
  base::WeakPtrFactory<GCMService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMService);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_SERVICE_H_
