// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/scoped_keep_alive.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"

ScopedKeepAlive::ScopedKeepAlive() {
  // Allow ScopedKeepAlive to be used in unit tests.
  if (g_browser_process)
    chrome::IncrementKeepAliveCount();
}

ScopedKeepAlive::~ScopedKeepAlive() {
  if (g_browser_process)
    chrome::DecrementKeepAliveCount();
}
