// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/power_save_blocker_factory.h"

#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"

namespace content {

std::unique_ptr<PowerSaveBlocker> CreatePowerSaveBlocker(
    PowerSaveBlocker::PowerSaveBlockerType type,
    PowerSaveBlocker::Reason reason,
    const std::string& description) {
  return PowerSaveBlocker::CreateWithTaskRunners(
      type, reason, description,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
}

}  // namespace content
