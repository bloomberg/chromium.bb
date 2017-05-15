// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_MEDIA_ANALYTICS_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_MEDIA_ANALYTICS_CLIENT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/media_analytics_client.h"
#include "chromeos/media_perception/media_perception.pb.h"

namespace chromeos {

// MediaAnalyticsClient is used to communicate with a media analytics process
// running outside of Chrome.
class CHROMEOS_EXPORT FakeMediaAnalyticsClient : public MediaAnalyticsClient {
 public:
  FakeMediaAnalyticsClient();
  ~FakeMediaAnalyticsClient() override;

  // Inherited from MediaAnalyticsClient.
  void GetState(const StateCallback& callback) override;
  void SetState(const mri::State& state,
                const StateCallback& callback) override;
  void SetMediaPerceptionSignalHandler(
      const MediaPerceptionSignalHandler& handler) override;
  void ClearMediaPerceptionSignalHandler() override;
  void GetDiagnostics(const DiagnosticsCallback& callback) override;

  // Inherited from DBusClient.
  void Init(dbus::Bus* bus) override;

  // Fires a fake media perception event.
  bool FireMediaPerceptionEvent(const mri::MediaPerception& media_perception);

  // Sets the object to be returned from GetDiagnostics.
  void SetDiagnostics(const mri::Diagnostics& diagnostics);

  void set_process_running(bool running) { process_running_ = running; }

  bool process_running() const { return process_running_; }

 private:
  // Echoes back the previously set state.
  void OnState(const StateCallback& callback);

  // Runs callback with the Diagnostics proto provided in SetDiagnostics.
  void OnGetDiagnostics(const DiagnosticsCallback& callback);

  // Runs callback with a MediaPerception proto provided in
  // FireMediaPerceptionEvent.
  void OnMediaPerception(const mri::MediaPerception& media_perception);

  // A handler for receiving MediaPerception proto messages.
  MediaPerceptionSignalHandler media_perception_signal_handler_;

  // A fake current state for the media analytics process.
  mri::State current_state_;

  // A fake diagnostics object to be returned by the GetDiagnostics.
  mri::Diagnostics diagnostics_;

  // Whether the fake media analytics was started (for example by the fake
  // upstart client) - If not set, all requests to this client will fail.
  bool process_running_;

  base::WeakPtrFactory<FakeMediaAnalyticsClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeMediaAnalyticsClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_MEDIA_ANALYTICS_CLIENT_H_
