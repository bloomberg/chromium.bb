// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ORIGIN_TRIALS_ORIGIN_TRIAL_KEY_MANAGER_H_
#define CHROME_COMMON_ORIGIN_TRIALS_ORIGIN_TRIAL_KEY_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"

// This class is instantiated on the main/ui thread, but its methods can be
// accessed from any thread.
class OriginTrialKeyManager {
 public:
  OriginTrialKeyManager();
  ~OriginTrialKeyManager();

  bool SetPublicKeyFromASCIIString(const std::string& ascii_public_key);
  base::StringPiece GetPublicKey() const;

 private:
  std::string public_key_;

  DISALLOW_COPY_AND_ASSIGN(OriginTrialKeyManager);
};

#endif  // CHROME_COMMON_ORIGIN_TRIALS_ORIGIN_TRIAL_KEY_MANAGER_H_
