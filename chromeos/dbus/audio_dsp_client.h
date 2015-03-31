// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_AUDIO_DSP_CLIENT_H_
#define CHROMEOS_DBUS_AUDIO_DSP_CLIENT_H_

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// A DBus client class for the org.chromium.AudioDsp service.
class CHROMEOS_EXPORT AudioDspClient : public DBusClient {
 public:
  // A callback to handle responses of methods returning a double value.
  typedef base::Callback<void(DBusMethodCallStatus call_status, double result)>
      DoubleDBusMethodCallback;

  // A callback to handle responses of methods returning two string values.
  typedef base::Callback<void(DBusMethodCallStatus call_status,
                              const std::string& result1,
                              const std::string& result2)>
      TwoStringDBusMethodCallback;

  // A callback to handle responses of methods returning three string values.
  typedef base::Callback<void(DBusMethodCallStatus call_status,
                              const std::string& result1,
                              const std::string& result2,
                              const std::string& result3)>
      ThreeStringDBusMethodCallback;

  // Interface for observing DSP events from a DSP client.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the Error signal is received.
    virtual void OnError(int32 error_code) = 0;
  };

  ~AudioDspClient() override;

  // Adds and removes observers for AUDIO_DSP events.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Factory function; creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static AudioDspClient* Create();

  // Calls Initialize method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded, and a bool that is the return value from
  // the DBus method.
  virtual void Initialize(const BoolDBusMethodCallback& callback) = 0;

  // Calls SetStandbyMode method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded.
  virtual void SetStandbyMode(bool standby,
                              const VoidDBusMethodCallback& callback) = 0;

  // Calls SetNightMode method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded.
  virtual void SetNightMode(bool night_mode,
                            const VoidDBusMethodCallback& callback) = 0;

  // Calls GetNightMode method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded, and a bool that is the return value from
  // the DBus method.
  virtual void GetNightMode(const BoolDBusMethodCallback& callback) = 0;

  // Calls SetTreble method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded.
  virtual void SetTreble(double db_fs,
                         const VoidDBusMethodCallback& callback) = 0;

  // Calls GetTreble method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded, and a double that is the return value from
  // the DBus method.
  virtual void GetTreble(const DoubleDBusMethodCallback& callback) = 0;

  // Calls SetBass method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded.
  virtual void SetBass(double db_fs,
                       const VoidDBusMethodCallback& callback) = 0;

  // Calls SetBass method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded, and a double that is the return value from
  // the DBus method.
  virtual void GetBass(const DoubleDBusMethodCallback& callback) = 0;

  // Calls GetCapabilitiesOEM method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded, and three std::strings that constitute
  // the return value from the DBus method.
  virtual void GetCapabilitiesOEM(
      const ThreeStringDBusMethodCallback& callback) = 0;

  // Calls SetCapabilitiesOEM method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded.
  virtual void SetCapabilitiesOEM(uint32 speaker_id,
                                  const std::string& speaker_capabilities,
                                  const std::string& driver_capabilities,
                                  const VoidDBusMethodCallback& callback) = 0;

  // Calls GetFilterConfigOEM method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded, and two std::strings that constitute
  // the return value from the DBus method.
  virtual void GetFilterConfigOEM(
      uint32 speaker_id,
      const TwoStringDBusMethodCallback& callback) = 0;

  // Calls SetFilterConfigOEM method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded.
  virtual void SetFilterConfigOEM(const std::string& speaker_config,
                                  const std::string& driver_config,
                                  const VoidDBusMethodCallback& callback) = 0;

  // Calls SetSourceType method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded.
  virtual void SetSourceType(uint16 source_type,
                             const VoidDBusMethodCallback& callback) = 0;

  // Calls AmplifierVolumeChanged method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded.
  virtual void AmplifierVolumeChanged(
      double db_spl,
      const VoidDBusMethodCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  AudioDspClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDspClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_AUDIO_DSP_CLIENT_H_
