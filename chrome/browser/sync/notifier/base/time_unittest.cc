// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/time.h"

namespace notifier {

TEST_NOTIFIER_F(TimeTest);

TEST_F(TimeTest, UseLocalTimeAsString) {
  // Just call it to ensure that it doesn't assert.
  GetLocalTimeAsString();
}

}  // namespace notifier
