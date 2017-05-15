// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MEDIA_ANALYTICS_CLIENT_H_
#define CHROMEOS_DBUS_MEDIA_ANALYTICS_CLIENT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/media_perception/media_perception.pb.h"

namespace chromeos {

// MediaAnalyticsClient is used to communicate with a media analytics process
// running outside of Chrome.
class CHROMEOS_EXPORT MediaAnalyticsClient : public DBusClient {
 public:
  // Callback type for a State proto message received from the media analytics
  // process.
  using StateCallback =
      base::Callback<void(bool succeeded, const mri::State& state)>;
  // Handler type for signal received from media analytics process.
  using MediaPerceptionSignalHandler =
      base::Callback<void(const mri::MediaPerception& media_perception)>;
  // Callback type for a Diagnostics proto message received from the media
  // analytics process.
  using DiagnosticsCallback =
      base::Callback<void(bool succeeded, const mri::Diagnostics& diagnostics)>;

  ~MediaAnalyticsClient() override;

  // Gets the media analytics process state.
  virtual void GetState(const StateCallback& callback) = 0;

  // Sets the media analytics process state. |state.status| is expected to be
  // set.
  virtual void SetState(const mri::State& state,
                        const StateCallback& callback) = 0;

  // Register event handler for the MediaPerception protos received as signal
  // from the media analytics process.
  virtual void SetMediaPerceptionSignalHandler(
      const MediaPerceptionSignalHandler& handler) = 0;

  // Resets the signal handler previously set by
  // SetMediaPerceptionSignalHandler.
  virtual void ClearMediaPerceptionSignalHandler() = 0;

  // API for getting diagnostic information from the media analytics process
  // over D-Bus as a Diagnostics proto message.
  virtual void GetDiagnostics(const DiagnosticsCallback& callback) = 0;

  // Factory function, creates new instance and returns ownership.
  // For normal usage, access the singleton via DbusThreadManager::Get().
  static MediaAnalyticsClient* Create();

 protected:
  MediaAnalyticsClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaAnalyticsClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MEDIA_ANALYTICS_CLIENT_H_
