// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_observer.h"

bool TabContentsObserver::OnMessageReceived(const IPC::Message& message) {
  return false;
}
