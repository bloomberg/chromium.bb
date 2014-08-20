// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DRIVER_DESKTOP_H_
#define COMPONENTS_GCM_DRIVER_GCM_DRIVER_DESKTOP_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_driver.h"

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

// GCMDriver implementation for desktop and Chrome OS, using GCMClient.
class GCMDriverDesktop : public GCMDriver {
 public:
  GCMDriverDesktop(
      scoped_ptr<GCMClientFactory> gcm_client_factory,
      const GCMClient::ChromeBuildInfo& chrome_build_info,
      const base::FilePath& store_path,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
      const scoped_refptr<base::SequencedTaskRunner>& io_thread,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);
  virtual ~GCMDriverDesktop();

  // GCMDriver overrides:
  virtual void Shutdown() OVERRIDE;
  virtual void OnSignedIn() OVERRIDE;
  virtual void Purge() OVERRIDE;
  virtual void AddAppHandler(const std::string& app_id,
                             GCMAppHandler* handler) OVERRIDE;
  virtual void RemoveAppHandler(const std::string& app_id) OVERRIDE;
  virtual void Enable() OVERRIDE;
  virtual void Disable() OVERRIDE;
  virtual GCMClient* GetGCMClientForTesting() const OVERRIDE;
  virtual bool IsStarted() const OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual void GetGCMStatistics(const GetGCMStatisticsCallback& callback,
                                bool clear_logs) OVERRIDE;
  virtual void SetGCMRecording(const GetGCMStatisticsCallback& callback,
                               bool recording) OVERRIDE;
  virtual void UpdateAccountMapping(
      const AccountMapping& account_mapping) OVERRIDE;
  virtual void RemoveAccountMapping(const std::string& account_id) OVERRIDE;

  // GCMDriverDesktop specific implementation.
  // Sets a list of accounts with OAuth2 tokens for the next checkin.
  // |account_tokens| maps email addresses to OAuth2 access tokens.
  // |account_removed| indicates that an account has been removed since the
  //     last time the callback was called, which triggers an immediate checkin,
  //     to ensure that association between device and account is removed.
  void SetAccountsForCheckin(
      const std::map<std::string, std::string>& account_tokens);

 protected:
  // GCMDriver implementation:
  virtual GCMClient::Result EnsureStarted() OVERRIDE;
  virtual void RegisterImpl(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids) OVERRIDE;
  virtual void UnregisterImpl(const std::string& app_id) OVERRIDE;
  virtual void SendImpl(const std::string& app_id,
                        const std::string& receiver_id,
                        const GCMClient::OutgoingMessage& message) OVERRIDE;

 private:
  class DelayedTaskController;
  class IOWorker;

  //  Stops the GCM service. It can be restarted by calling EnsureStarted again.
  void Stop();

  // Remove cached data when GCM service is stopped.
  void RemoveCachedData();

  void DoRegister(const std::string& app_id,
                  const std::vector<std::string>& sender_ids);
  void DoUnregister(const std::string& app_id);
  void DoSend(const std::string& app_id,
              const std::string& receiver_id,
              const GCMClient::OutgoingMessage& message);

  // Callbacks posted from IO thread to UI thread.
  void MessageReceived(const std::string& app_id,
                       const GCMClient::IncomingMessage& message);
  void MessagesDeleted(const std::string& app_id);
  void MessageSendError(const std::string& app_id,
                        const GCMClient::SendErrorDetails& send_error_details);
  void SendAcknowledged(const std::string& app_id,
                        const std::string& message_id);
  void GCMClientReady();
  void OnConnected(const net::IPEndPoint& ip_endpoint);
  void OnDisconnected();

  void GetGCMStatisticsFinished(const GCMClient::GCMStatistics& stats);

  // Flag to indicate whether the user is signed in to a GAIA account.
  // TODO(jianli): To be removed when sign-in enforcement is dropped.
  bool signed_in_;

  // Flag to indicate if GCM is started.
  bool gcm_started_;

  // Flag to indicate if GCM is enabled.
  bool gcm_enabled_;

  // Flag to indicate the last known state of the GCM client. Because this
  // flag lives on the UI thread, while the GCM client lives on the IO thread,
  // it may be out of date while connection changes are happening.
  bool connected_;

  scoped_refptr<base::SequencedTaskRunner> ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> io_thread_;

  scoped_ptr<DelayedTaskController> delayed_task_controller_;

  // For all the work occurring on the IO thread. Must be destroyed on the IO
  // thread.
  scoped_ptr<IOWorker> io_worker_;

  // Callback for GetGCMStatistics.
  GetGCMStatisticsCallback request_gcm_statistics_callback_;

  // Used to pass a weak pointer to the IO worker.
  base::WeakPtrFactory<GCMDriverDesktop> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMDriverDesktop);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_DESKTOP_H_
