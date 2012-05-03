// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/protector_utils.h"

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/protector/keys.h"
#include "chrome/common/chrome_switches.h"
#include "crypto/hmac.h"

namespace protector {

std::string SignSetting(const std::string& value) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(kProtectorSigningKey)) {
    LOG(WARNING) << "Failed to initialize HMAC algorithm for signing";
    return std::string();
  }

  std::vector<unsigned char> digest(hmac.DigestLength());
  if (!hmac.Sign(value, &digest[0], digest.size())) {
    LOG(WARNING) << "Failed to sign setting";
    return std::string();
  }

  return std::string(&digest[0], &digest[0] + digest.size());
}

bool IsSettingValid(const std::string& value, const std::string& signature) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(kProtectorSigningKey)) {
    LOG(WARNING) << "Failed to initialize HMAC algorithm for verification.";
    return false;
  }
  return hmac.Verify(value, signature);
}

bool IsEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kProtector);
}

}  // namespace protector
