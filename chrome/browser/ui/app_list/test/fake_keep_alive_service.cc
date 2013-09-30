// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/fake_keep_alive_service.h"

FakeKeepAliveService::FakeKeepAliveService()
  : is_keeping_alive_(false) {
}

void FakeKeepAliveService::EnsureKeepAlive() {
  is_keeping_alive_ = true;
}

void FakeKeepAliveService::FreeKeepAlive() {
  is_keeping_alive_ = false;
}
