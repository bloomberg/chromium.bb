// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/origin_trials/chrome_origin_trial_policy.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

// This is the default public key used for validating signatures.
// TODO(iclelland): Provide a mechanism to allow for multiple signing keys.
// https://crbug.com/584737
static const uint8_t kDefaultPublicKey[] = {
    0x7c, 0xc4, 0xb8, 0x9a, 0x93, 0xba, 0x6e, 0xe2, 0xd0, 0xfd, 0x03,
    0x1d, 0xfb, 0x32, 0x66, 0xc7, 0x3b, 0x72, 0xfd, 0x54, 0x3a, 0x07,
    0x51, 0x14, 0x66, 0xaa, 0x02, 0x53, 0x4e, 0x33, 0xa1, 0x15,
};

ChromeOriginTrialPolicy::ChromeOriginTrialPolicy()
    : public_key_(std::string(reinterpret_cast<const char*>(kDefaultPublicKey),
                              arraysize(kDefaultPublicKey))) {
  // Set the public key for the origin trial key manager, based on the command
  // line flags which were passed to this process. If the flag is not present,
  // or is incorrectly formatted, the default key will remain active.
  if (base::CommandLine::InitializedForCurrentProcess()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kOriginTrialPublicKey)) {
      SetPublicKeyFromASCIIString(
        command_line->GetSwitchValueASCII(switches::kOriginTrialPublicKey));
    }
  }
}

ChromeOriginTrialPolicy::~ChromeOriginTrialPolicy() {}

base::StringPiece ChromeOriginTrialPolicy::GetPublicKey() const {
  return base::StringPiece(public_key_);
}

bool ChromeOriginTrialPolicy::SetPublicKeyFromASCIIString(
    const std::string& ascii_public_key) {
  // Base64-decode the incoming string. Set the key if it is correctly formatted
  std::string new_public_key;
  if (!base::Base64Decode(ascii_public_key, &new_public_key))
    return false;
  if (new_public_key.size() != 32)
    return false;
  public_key_.swap(new_public_key);
  return true;
}
