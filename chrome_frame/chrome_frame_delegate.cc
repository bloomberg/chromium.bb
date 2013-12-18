// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_delegate.h"

#define NO_CODE ((void)0)

bool ChromeFrameDelegateImpl::IsTabMessage(const IPC::Message& message) {
  return false;
}

#undef NO_CODE

bool ChromeFrameDelegateImpl::OnMessageReceived(const IPC::Message& msg) {
  return false;
}
