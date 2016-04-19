// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_FAKE_GCM_PROFILE_SERVICE_H_
#define CHROME_BROWSER_SERVICES_GCM_FAKE_GCM_PROFILE_SERVICE_H_

#include <list>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace gcm {

// Acts as a bridge between GCM API and GCMClient layer for testing purposes.
class FakeGCMProfileService : public GCMProfileService {
 public:
  typedef base::Callback<void(const std::string&)> UnregisterCallback;

  // Helper function to be used with
  // KeyedService::SetTestingFactory().
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context);

  explicit FakeGCMProfileService(Profile* profile);
  ~FakeGCMProfileService() override;

  void RegisterFinished(const std::string& app_id,
                        const std::vector<std::string>& sender_ids);
  void UnregisterFinished(const std::string& app_id);
  void SendFinished(const std::string& app_id,
                    const std::string& receiver_id,
                    const OutgoingMessage& message);

  void AddExpectedUnregisterResponse(GCMClient::Result result);

  void SetUnregisterCallback(const UnregisterCallback& callback);

  void DispatchMessage(const std::string& app_id,
                       const IncomingMessage& message);

  const OutgoingMessage& last_sent_message() const {
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

  void set_collect(bool collect) {
    collect_ = collect;
  }

 private:
  // Indicates whether the service will collect paramters of the calls for
  // furter verification in tests.
  bool collect_;
  // Used to give each registration a unique registration id. Does not decrease
  // when unregister is called.
  int registration_count_;
  std::string last_registered_app_id_;
  std::vector<std::string> last_registered_sender_ids_;
  std::list<GCMClient::Result> unregister_responses_;
  OutgoingMessage last_sent_message_;
  std::string last_receiver_id_;
  UnregisterCallback unregister_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMProfileService);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_FAKE_GCM_PROFILE_SERVICE_H_
