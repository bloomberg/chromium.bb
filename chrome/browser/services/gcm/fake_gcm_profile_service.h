// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_FAKE_GCM_PROFILE_SERVICE_H_
#define CHROME_BROWSER_SERVICES_GCM_FAKE_GCM_PROFILE_SERVICE_H_

#include "chrome/browser/services/gcm/gcm_profile_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace gcm {

// Acts as a bridge between GCM API and GCMClient layer for testing purposes.
class FakeGCMProfileService : public GCMProfileService {
 public:
  // Helper function to be used with
  // BrowserContextKeyedService::SetTestingFactory().
  static BrowserContextKeyedService* Build(content::BrowserContext* context);

  static void EnableGCMForTesting();

  explicit FakeGCMProfileService(Profile* profile);
  virtual ~FakeGCMProfileService();

  // GCMProfileService overrides.
  virtual void Register(const std::string& app_id,
                        const std::vector<std::string>& sender_ids,
                        const std::string& cert,
                        RegisterCallback callback) OVERRIDE;
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const GCMClient::OutgoingMessage& message,
                    SendCallback callback) OVERRIDE;

  void RegisterFinished(const std::string& app_id,
                        const std::vector<std::string>& sender_ids,
                        const std::string& cert,
                        RegisterCallback callback);

  void SendFinished(const std::string& app_id,
                    const std::string& receiver_id,
                    const GCMClient::OutgoingMessage& message,
                    SendCallback callback);

  const GCMClient::OutgoingMessage& last_sent_message() const {
    return last_sent_message_;
  }

  const std::string& last_receiver_id() const {
    return last_receiver_id_;
  }

  const std::string& last_registered_app_id() const {
    return last_registered_app_id_;
  }

  const std::vector<std::string>& last_registered_sender_ids() const {
    return last_registered_sender_ids_;
  }

  const std::string& last_registered_cert() const {
    return last_registered_cert_;
  }

  void set_collect(bool collect) {
    collect_ = collect;
  }

 private:
  // Indicates whether the serivce will collect paramters of the calls for
  // furter verification in tests.
  bool collect_;
  std::string last_registered_app_id_;
  std::vector<std::string> last_registered_sender_ids_;
  std::string last_registered_cert_;
  GCMClient::OutgoingMessage last_sent_message_;
  std::string last_receiver_id_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMProfileService);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_FAKE_GCM_PROFILE_SERVICE_H_
