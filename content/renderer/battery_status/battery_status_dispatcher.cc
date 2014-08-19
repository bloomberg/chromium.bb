// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "battery_status_dispatcher.h"

#include "content/common/battery_status_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebBatteryStatusListener.h"

namespace content {

BatteryStatusDispatcher::BatteryStatusDispatcher(RenderThread* thread)
    : PlatformEventObserver<blink::WebBatteryStatusListener>(thread) {
}

BatteryStatusDispatcher::~BatteryStatusDispatcher() {
}

bool BatteryStatusDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BatteryStatusDispatcher, message)
    IPC_MESSAGE_HANDLER(BatteryStatusMsg_DidChange, OnDidChange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BatteryStatusDispatcher::SendStartMessage() {
  RenderThread::Get()->Send(new BatteryStatusHostMsg_Start());
}

void BatteryStatusDispatcher::SendStopMessage() {
  RenderThread::Get()->Send(new BatteryStatusHostMsg_Stop());
}

void BatteryStatusDispatcher::OnDidChange(
    const blink::WebBatteryStatus& status) {
  if (listener())
    listener()->updateBatteryStatus(status);
}

void BatteryStatusDispatcher::SendFakeDataForTesting(void* fake_data) {
  OnDidChange(*static_cast<blink::WebBatteryStatus*>(fake_data));
}

}  // namespace content
