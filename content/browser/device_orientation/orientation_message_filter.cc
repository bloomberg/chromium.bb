// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/orientation_message_filter.h"

#include "content/browser/device_orientation/device_data.h"
#include "content/common/device_orientation_messages.h"
#include "content/public/browser/browser_thread.h"

namespace content {

OrientationMessageFilter::OrientationMessageFilter()
    :  DeviceOrientationMessageFilter(DeviceData::kTypeOrientation) {
}

OrientationMessageFilter::~OrientationMessageFilter() {
}

bool OrientationMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                 bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(OrientationMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(DeviceOrientationHostMsg_StartUpdating, OnStartUpdating)
    IPC_MESSAGE_HANDLER(DeviceOrientationHostMsg_StopUpdating, OnStopUpdating)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace content
