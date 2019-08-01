// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_H_

#include "base/strings/string_piece_forward.h"
#include "url/gurl.h"

namespace password_manager {

// The base class for requests for checking if {username, password} pair was
// leaked in the internet.
class LeakDetectionCheck {
 public:
  LeakDetectionCheck() = default;
  virtual ~LeakDetectionCheck() = default;

  // Not copyable or movable
  LeakDetectionCheck(const LeakDetectionCheck&) = delete;
  LeakDetectionCheck& operator=(const LeakDetectionCheck&) = delete;
  LeakDetectionCheck(LeakDetectionCheck&&) = delete;
  LeakDetectionCheck& operator=(LeakDetectionCheck&&) = delete;

  // Starts checking |username| and |password| pair asynchronously.
  // |url| is used later for presentation in the UI but not for actual business
  // logic.
  virtual void Start(const GURL& url,
                     base::StringPiece16 username,
                     base::StringPiece16 password) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_H_
