// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ORIGIN_TRIALS_ORIGIN_TRIAL_KEY_MANAGER_H_
#define CHROME_RENDERER_ORIGIN_TRIALS_ORIGIN_TRIAL_KEY_MANAGER_H_

#include "base/strings/string_piece.h"

class OriginTrialKeyManager {
 public:
  base::StringPiece GetPublicKey();
};

#endif  // CHROME_RENDERER_ORIGIN_TRIALS_ORIGIN_TRIAL_KEY_MANAGER_H_
