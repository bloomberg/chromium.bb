// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_UPDATE_ENGINE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_UPDATE_ENGINE_CLIENT_H_

#include <string>

#include "chromeos/dbus/update_engine_client.h"

namespace chromeos {

// A fake implementation of UpdateEngineClient. The user of this class can
// use set_update_engine_client_status() to set a fake last Status and
// GetLastStatus() returns the fake with no modification. Other methods do
// nothing.
class FakeUpdateEngineClient : public UpdateEngineClient {
 public:
  FakeUpdateEngineClient();
  virtual ~FakeUpdateEngineClient();

  // Overrides
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObserver(Observer* observer) OVERRIDE;
  virtual void RequestUpdateCheck(const UpdateCheckCallback& callback) OVERRIDE;
  virtual void RebootAfterUpdate() OVERRIDE;
  virtual void SetReleaseTrack(const std::string& track) OVERRIDE;
  virtual void GetReleaseTrack(const GetReleaseTrackCallback& callback)
      OVERRIDE;
  virtual Status GetLastStatus() OVERRIDE;

  void set_update_engine_client_status(
      const UpdateEngineClient::Status& status);

 private:
  UpdateEngineClient::Status update_engine_client_status_;

};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_UPDATE_ENGINE_CLIENT_H_
