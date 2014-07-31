// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_easy_unlock_client.h"

namespace chromeos {

FakeEasyUnlockClient::FakeEasyUnlockClient() {}

FakeEasyUnlockClient::~FakeEasyUnlockClient() {}

void FakeEasyUnlockClient::Init(dbus::Bus* bus) {}

void FakeEasyUnlockClient::GenerateEcP256KeyPair(
    const KeyPairCallback& callback) {
}

void FakeEasyUnlockClient::PerformECDHKeyAgreement(
    const std::string& private_key,
    const std::string& public_key,
    const DataCallback& callback) {
}

void FakeEasyUnlockClient::CreateSecureMessage(
    const std::string& payload,
    const std::string& secret_key,
    const std::string& associated_data,
    const std::string& public_metadata,
    const std::string& verification_key_id,
    const std::string& encryption_type,
    const std::string& signature_type,
    const DataCallback& callback) {
}

void FakeEasyUnlockClient::UnwrapSecureMessage(
    const std::string& message,
    const std::string& secret_key,
    const std::string& associated_data,
    const std::string& encryption_type,
    const std::string& signature_type,
    const DataCallback& callback) {
}

}  // namespace chromeos
