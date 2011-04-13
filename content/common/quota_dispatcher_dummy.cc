// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/quota_dispatcher.h"

#include "base/compiler_specific.h"

// QuotaDispatcher -------------------------------------------------------------

QuotaDispatcher::QuotaDispatcher() {
}

QuotaDispatcher::~QuotaDispatcher() {
}

// QuotaDispatcher implementation ----------------------------------------------

bool QuotaDispatcher::OnMessageReceived(const IPC::Message& message) {
  return false;
}
