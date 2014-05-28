// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/fake_gcm_profile_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "content/public/browser/browser_context.h"

namespace gcm {

namespace {

class FakeGCMDriver : public GCMDriver {
 public:
  explicit FakeGCMDriver(FakeGCMProfileService* service);
  virtual ~FakeGCMDriver();

  // GCMDriver overrides.
  virtual void Shutdown() OVERRIDE;
  virtual void AddAppHandler(const std::string& app_id,
                             GCMAppHandler* handler) OVERRIDE;
  virtual void RemoveAppHandler(const std::string& app_id) OVERRIDE;
  virtual void Register(const std::string& app_id,
                        const std::vector<std::string>& sender_ids,
                        const RegisterCallback& callback) OVERRIDE;
  virtual void Unregister(const std::string& app_id,
                          const UnregisterCallback& callback) OVERRIDE;
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const GCMClient::OutgoingMessage& message,
                    const SendCallback& callback) OVERRIDE;

 private:
  FakeGCMProfileService* service_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMDriver);
};

FakeGCMDriver::FakeGCMDriver(FakeGCMProfileService* service)
    : service_(service) {
}

FakeGCMDriver::~FakeGCMDriver() {
}

void FakeGCMDriver::Shutdown() {
}

void FakeGCMDriver::AddAppHandler(const std::string& app_id,
                                  GCMAppHandler* handler) {
}

void FakeGCMDriver::RemoveAppHandler(const std::string& app_id) {
}

void FakeGCMDriver::Register(const std::string& app_id,
                             const std::vector<std::string>& sender_ids,
                             const RegisterCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMProfileService::RegisterFinished,
                 base::Unretained(service_),
                 app_id,
                 sender_ids,
                 callback));
}

void FakeGCMDriver::Unregister(const std::string& app_id,
                               const UnregisterCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(
          &FakeGCMProfileService::UnregisterFinished,
          base::Unretained(service_),
          app_id,
          callback));
}

void FakeGCMDriver::Send(const std::string& app_id,
                         const std::string& receiver_id,
                         const GCMClient::OutgoingMessage& message,
                         const SendCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMProfileService::SendFinished,
                 base::Unretained(service_),
                 app_id,
                 receiver_id,
                 message,
                 callback));
}

}  // namespace

// static
KeyedService* FakeGCMProfileService::Build(content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  FakeGCMProfileService* service = new FakeGCMProfileService(profile);
  service->SetDriverForTesting(new FakeGCMDriver(service));
  return service;
}

FakeGCMProfileService::FakeGCMProfileService(Profile* profile)
    : collect_(false) {}

FakeGCMProfileService::~FakeGCMProfileService() {}

void FakeGCMProfileService::RegisterFinished(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const GCMDriver::RegisterCallback& callback) {
  if (collect_) {
    last_registered_app_id_ = app_id;
    last_registered_sender_ids_ = sender_ids;
  }

  callback.Run(base::UintToString(sender_ids.size()), GCMClient::SUCCESS);
}

void FakeGCMProfileService::UnregisterFinished(
    const std::string& app_id,
    const GCMDriver::UnregisterCallback& callback) {
  GCMClient::Result result = GCMClient::SUCCESS;
  if (!unregister_responses_.empty()) {
    result = unregister_responses_.front();
    unregister_responses_.pop_front();
  }

  callback.Run(result);
}

void FakeGCMProfileService::SendFinished(
    const std::string& app_id,
    const std::string& receiver_id,
    const GCMClient::OutgoingMessage& message,
    const GCMDriver::SendCallback& callback) {
  if (collect_) {
    last_sent_message_ = message;
    last_receiver_id_ = receiver_id;
  }

  callback.Run(message.id, GCMClient::SUCCESS);
}

void FakeGCMProfileService::AddExpectedUnregisterResponse(
    GCMClient::Result result) {
  unregister_responses_.push_back(result);
}

}  // namespace gcm
