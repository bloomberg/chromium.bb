// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_MANAGER_H_
#define EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/media_perception/media_perception.pb.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/api/media_perception_private.h"

namespace extensions {

class MediaPerceptionAPIManager : public BrowserContextKeyedAPI {
 public:
  enum class CallbackStatus {
    // Request to media analytics process was successful.
    SUCCESS,
    // Request to media analytics process failed at D-Bus layer.
    DBUS_ERROR,
    // The media analytics process is not running.
    PROCESS_IDLE_ERROR,
    // The media analytics process is still being launched via Upstart
    // service.
    PROCESS_LAUNCHING_ERROR
  };

  using APIStateCallback = base::Callback<void(
      CallbackStatus status,
      extensions::api::media_perception_private::State state)>;

  using APIGetDiagnosticsCallback = base::Callback<void(
      CallbackStatus status,
      extensions::api::media_perception_private::Diagnostics diagnostics)>;

  explicit MediaPerceptionAPIManager(content::BrowserContext* context);
  ~MediaPerceptionAPIManager() override;

  // Convenience method to get the MediaPeceptionAPIManager for a
  // BrowserContext.
  static MediaPerceptionAPIManager* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<MediaPerceptionAPIManager>*
  GetFactoryInstance();

  // Public functions for MediaPerceptionPrivateAPI implementation.
  void GetState(const APIStateCallback& callback);
  void SetState(const extensions::api::media_perception_private::State& state,
                const APIStateCallback& callback);
  void GetDiagnostics(const APIGetDiagnosticsCallback& callback);

 private:
  friend class BrowserContextKeyedAPIFactory<MediaPerceptionAPIManager>;

  // BrowserContextKeyedAPI:
  static const char* service_name() { return "MediaPerceptionAPIManager"; }

  enum class AnalyticsProcessState {
    // The process is not running.
    IDLE,
    // The process has been launched via Upstart, but waiting for callback to
    // confirm.
    LAUNCHING,
    // The process is running.
    RUNNING
  };

  // Sets the state of the analytics process.
  void SetStateInternal(const APIStateCallback& callback,
                        const mri::State& state);

  // Event handler for MediaPerception proto messages coming over D-Bus as
  // signal.
  void MediaPerceptionSignalHandler(
      const mri::MediaPerception& media_perception);

  // Callback for State D-Bus method calls to the media analytics process.
  void StateCallback(const APIStateCallback& callback,
                     bool succeeded,
                     const mri::State& state);

  // Callback for GetDiagnostics D-Bus method calls to the media analytics
  // process.
  void GetDiagnosticsCallback(const APIGetDiagnosticsCallback& callback,
                              bool succeeded,
                              const mri::Diagnostics& diagnostics);

  // Callback for Upstart command to start media analytics process.
  void UpstartStartCallback(const APIStateCallback& callback,
                            const mri::State& state,
                            bool succeeded);

  // Callback for Upstart command to restart media analytics process.
  void UpstartRestartCallback(const APIStateCallback& callback, bool succeeded);

  content::BrowserContext* const browser_context_;

  // Keeps track of whether the analytics process is running so that it can be
  // started with an Upstart D-Bus method call if necessary.
  AnalyticsProcessState analytics_process_state_;

  base::WeakPtrFactory<MediaPerceptionAPIManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionAPIManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_MANAGER_H_
