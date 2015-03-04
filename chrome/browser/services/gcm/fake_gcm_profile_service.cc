// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/fake_gcm_profile_service.h"

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "content/public/browser/browser_context.h"

namespace gcm {

namespace {

class CustomFakeGCMDriver : public FakeGCMDriver {
 public:
  explicit CustomFakeGCMDriver(FakeGCMProfileService* service);
  ~CustomFakeGCMDriver() override;

  void OnRegisterFinished(const std::string& app_id,
                          const std::string& registration_id,
                          GCMClient::Result result);
  void OnUnregisterFinished(const std::string& app_id,
                            GCMClient::Result result);
  void OnSendFinished(const std::string& app_id,
                      const std::string& message_id,
                      GCMClient::Result result);

 protected:
  // FakeGCMDriver overrides:
  void RegisterImpl(const std::string& app_id,
                    const std::vector<std::string>& sender_ids) override;
  void UnregisterImpl(const std::string& app_id) override;
  void SendImpl(const std::string& app_id,
                const std::string& receiver_id,
                const GCMClient::OutgoingMessage& message) override;

 private:
  FakeGCMProfileService* service_;

  DISALLOW_COPY_AND_ASSIGN(CustomFakeGCMDriver);
};

CustomFakeGCMDriver::CustomFakeGCMDriver(FakeGCMProfileService* service)
    : service_(service) {
}

CustomFakeGCMDriver::~CustomFakeGCMDriver() {
}

void CustomFakeGCMDriver::RegisterImpl(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMProfileService::RegisterFinished,
                 base::Unretained(service_),
                 app_id,
                 sender_ids));
}

void CustomFakeGCMDriver::UnregisterImpl(const std::string& app_id) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(
          &FakeGCMProfileService::UnregisterFinished,
          base::Unretained(service_),
          app_id));
}

void CustomFakeGCMDriver::SendImpl(const std::string& app_id,
                                   const std::string& receiver_id,
                                   const GCMClient::OutgoingMessage& message) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMProfileService::SendFinished,
                 base::Unretained(service_),
                 app_id,
                 receiver_id,
                 message));
}

void CustomFakeGCMDriver::OnRegisterFinished(
    const std::string& app_id,
    const std::string& registration_id,
    GCMClient::Result result) {
  RegisterFinished(app_id, registration_id, result);
}
void CustomFakeGCMDriver::OnUnregisterFinished(const std::string& app_id,
                                               GCMClient::Result result) {
  UnregisterFinished(app_id, result);
}
void CustomFakeGCMDriver::OnSendFinished(const std::string& app_id,
                                         const std::string& message_id,
                                         GCMClient::Result result) {
  SendFinished(app_id, message_id, result);
}

}  // namespace

// static
KeyedService* FakeGCMProfileService::Build(content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  FakeGCMProfileService* service = new FakeGCMProfileService(profile);
  service->SetDriverForTesting(new CustomFakeGCMDriver(service));
  return service;
}

FakeGCMProfileService::FakeGCMProfileService(Profile* profile)
    : collect_(false),
      registration_count_(0) {
}

FakeGCMProfileService::~FakeGCMProfileService() {}

void FakeGCMProfileService::RegisterFinished(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  if (collect_) {
    last_registered_app_id_ = app_id;
    last_registered_sender_ids_ = sender_ids;
  }

  // Generate fake registration IDs, encoding the number of sender IDs (used by
  // GcmApiTest.RegisterValidation), then an incrementing count (even for the
  // same app_id - there's no caching) so tests can distinguish registrations.
  std::string registration_id = base::StringPrintf("%" PRIuS "-%d",
                                                   sender_ids.size(),
                                                   registration_count_);
  ++registration_count_;

  CustomFakeGCMDriver* custom_driver =
      static_cast<CustomFakeGCMDriver*>(driver());
  custom_driver->OnRegisterFinished(
      app_id, registration_id, GCMClient::SUCCESS);
}

void FakeGCMProfileService::UnregisterFinished(const std::string& app_id) {
  GCMClient::Result result = GCMClient::SUCCESS;
  if (!unregister_responses_.empty()) {
    result = unregister_responses_.front();
    unregister_responses_.pop_front();
  }

  CustomFakeGCMDriver* custom_driver =
      static_cast<CustomFakeGCMDriver*>(driver());
  custom_driver->OnUnregisterFinished(app_id, result);

  if (!unregister_callback_.is_null())
    unregister_callback_.Run(app_id);
}

void FakeGCMProfileService::SendFinished(
    const std::string& app_id,
    const std::string& receiver_id,
    const GCMClient::OutgoingMessage& message) {
  if (collect_) {
    last_sent_message_ = message;
    last_receiver_id_ = receiver_id;
  }

  CustomFakeGCMDriver* custom_driver =
      static_cast<CustomFakeGCMDriver*>(driver());
  custom_driver->OnSendFinished(app_id, message.id, GCMClient::SUCCESS);
}

void FakeGCMProfileService::AddExpectedUnregisterResponse(
    GCMClient::Result result) {
  unregister_responses_.push_back(result);
}

void FakeGCMProfileService::SetUnregisterCallback(
    const UnregisterCallback& callback) {
  unregister_callback_ = callback;
}

}  // namespace gcm
