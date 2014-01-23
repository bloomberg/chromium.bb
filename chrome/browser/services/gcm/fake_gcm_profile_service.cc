// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/fake_gcm_profile_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace gcm {

// static
BrowserContextKeyedService* FakeGCMProfileService::Build(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return new FakeGCMProfileService(profile);
}

// static
void FakeGCMProfileService::EnableGCMForTesting() {
  GCMProfileService::enable_gcm_for_testing_ = true;
}

FakeGCMProfileService::FakeGCMProfileService(Profile* profile)
    : GCMProfileService(profile),
      collect_(false) {}

FakeGCMProfileService::~FakeGCMProfileService() {}

void FakeGCMProfileService::Register(const std::string& app_id,
                                     const std::vector<std::string>& sender_ids,
                                     const std::string& cert,
                                     RegisterCallback callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMProfileService::RegisterFinished,
                 base::Unretained(this),
                 app_id,
                 sender_ids,
                 cert,
                 callback));
}

void FakeGCMProfileService::RegisterFinished(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& cert,
    RegisterCallback callback) {
  if (collect_) {
    last_registered_app_id_ = app_id;
    last_registered_sender_ids_ = sender_ids;
    last_registered_cert_ = cert;
  }

  callback.Run(base::UintToString(sender_ids.size()), GCMClient::SUCCESS);
}

void FakeGCMProfileService::Send(const std::string& app_id,
                                 const std::string& receiver_id,
                                 const GCMClient::OutgoingMessage& message,
                                 SendCallback callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeGCMProfileService::SendFinished,
                 base::Unretained(this),
                 app_id,
                 receiver_id,
                 message,
                 callback));
}

void FakeGCMProfileService::SendFinished(
    const std::string& app_id,
    const std::string& receiver_id,
    const GCMClient::OutgoingMessage& message,
    SendCallback callback) {
  if (collect_) {
    last_sent_message_ = message;
    last_receiver_id_ = receiver_id;
  }

  callback.Run(message.id, GCMClient::SUCCESS);
}

}  // namespace gcm
