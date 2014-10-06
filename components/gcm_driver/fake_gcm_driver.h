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
  virtual void Shutdown() override;
  virtual void AddAppHandler(const std::string& app_id,
                             GCMAppHandler* handler) override;
  virtual void RemoveAppHandler(const std::string& app_id) override;
  virtual void OnSignedIn() override;
  virtual void Purge() override;
  virtual void AddConnectionObserver(GCMConnectionObserver* observer) override;
  virtual void RemoveConnectionObserver(
      GCMConnectionObserver* observer) override;
  virtual void Enable() override;
  virtual void Disable() override;
  virtual GCMClient* GetGCMClientForTesting() const override;
  virtual bool IsStarted() const override;
  virtual bool IsConnected() const override;
  virtual void GetGCMStatistics(const GetGCMStatisticsCallback& callback,
                                bool clear_logs) override;
  virtual void SetGCMRecording(const GetGCMStatisticsCallback& callback,
                               bool recording) override;
  virtual void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens) override;
  virtual void UpdateAccountMapping(
      const AccountMapping& account_mapping) override;
  virtual void RemoveAccountMapping(const std::string& account_id) override;

 protected:
  // GCMDriver implementation:
  virtual GCMClient::Result EnsureStarted() override;
  virtual void RegisterImpl(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids) override;
  virtual void UnregisterImpl(const std::string& app_id) override;
  virtual void SendImpl(const std::string& app_id,
                        const std::string& receiver_id,
                        const GCMClient::OutgoingMessage& message) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGCMDriver);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_FAKE_GCM_DRIVER_H_
