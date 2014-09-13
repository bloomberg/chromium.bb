// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_FAKE_GCM_DRIVER_H_
#define COMPONENTS_GCM_DRIVER_FAKE_GCM_DRIVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/gcm_driver/gcm_driver.h"

namespace gcm {

class FakeGCMDriver : public GCMDriver {
 public:
  FakeGCMDriver();
  virtual ~FakeGCMDriver();

  // GCMDriver overrides:
  virtual void Shutdown() OVERRIDE;
  virtual void AddAppHandler(const std::string& app_id,
                             GCMAppHandler* handler) OVERRIDE;
  virtual void RemoveAppHandler(const std::string& app_id) OVERRIDE;
  virtual void OnSignedIn() OVERRIDE;
  virtual void OnSignedOut() OVERRIDE;
  virtual void Purge() OVERRIDE;
  virtual void AddConnectionObserver(GCMConnectionObserver* observer) OVERRIDE;
  virtual void RemoveConnectionObserver(
      GCMConnectionObserver* observer) OVERRIDE;
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
  DISALLOW_COPY_AND_ASSIGN(FakeGCMDriver);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_FAKE_GCM_DRIVER_H_
