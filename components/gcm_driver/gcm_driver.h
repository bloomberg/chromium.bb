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
#include "base/threading/thread_checker.h"
#include "components/gcm_driver/default_gcm_app_handler.h"
#include "google_apis/gcm/gcm_client.h"

namespace gcm {

class GCMAppHandler;

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

  // This method must be called before destroying the GCMDriver. Once it has
  // been called, no other GCMDriver methods may be used.
  virtual void Shutdown();

  // Adds a handler for a given app.
  virtual void AddAppHandler(const std::string& app_id, GCMAppHandler* handler);

  // Remove the handler for a given app.
  virtual void RemoveAppHandler(const std::string& app_id);

  // Enables/disables GCM service.
  virtual void Enable() = 0;
  virtual void Disable() = 0;

  // Registers |sender_id| for an app. A registration ID will be returned by
  // the GCM server.
  // |app_id|: application ID.
  // |sender_ids|: list of IDs of the servers that are allowed to send the
  //               messages to the application. These IDs are assigned by the
  //               Google API Console.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void Register(const std::string& app_id,
                        const std::vector<std::string>& sender_ids,
                        const RegisterCallback& callback) = 0;

  // Unregisters an app from using GCM.
  // |app_id|: application ID.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void Unregister(const std::string& app_id,
                          const UnregisterCallback& callback) = 0;

  // Sends a message to a given receiver.
  // |app_id|: application ID.
  // |receiver_id|: registration ID of the receiver party.
  // |message|: message to be sent.
  // |callback|: to be called once the asynchronous operation is done.
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const GCMClient::OutgoingMessage& message,
                    const SendCallback& callback) = 0;

  // For testing purpose. Always NULL on Android.
  virtual GCMClient* GetGCMClientForTesting() const = 0;

  // Returns true if the service was started.
  virtual bool IsStarted() const = 0;

  // Returns true if the gcm client is ready.
  virtual bool IsGCMClientReady() const = 0;

  // Get GCM client internal states and statistics.
  // If clear_logs is true then activity logs will be cleared before the stats
  // are returned.
  virtual void GetGCMStatistics(const GetGCMStatisticsCallback& callback,
                                bool clear_logs) = 0;

  // Enables/disables GCM activity recording, and then returns the stats.
  virtual void SetGCMRecording(const GetGCMStatisticsCallback& callback,
                               bool recording) = 0;

  // Returns the user name if the profile is signed in. Empty string otherwise.
  virtual std::string SignedInUserName() const = 0;

  const GCMAppHandlerMap& app_handlers() const { return app_handlers_; }

 protected:
  // Returns the handler for the given app.
  GCMAppHandler* GetAppHandler(const std::string& app_id);

 private:
  // App handler map (from app_id to handler pointer).
  // The handler is not owned.
  GCMAppHandlerMap app_handlers_;

  // The default handler when no app handler can be found in the map.
  DefaultGCMAppHandler default_app_handler_;

  DISALLOW_COPY_AND_ASSIGN(GCMDriver);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_H_
