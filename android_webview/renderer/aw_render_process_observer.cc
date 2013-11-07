// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_process_observer.h"

#include "android_webview/common/render_view_messages.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebNetworkStateNotifier.h"

namespace android_webview {

AwRenderProcessObserver::AwRenderProcessObserver()
  : webkit_initialized_(false) {
}

AwRenderProcessObserver::~AwRenderProcessObserver() {
}

bool AwRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AwRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(AwViewMsg_ClearCache, OnClearCache)
    IPC_MESSAGE_HANDLER(AwViewMsg_SetJsOnlineProperty, OnSetJsOnlineProperty)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AwRenderProcessObserver::WebKitInitialized() {
  webkit_initialized_ = true;
}

void AwRenderProcessObserver::OnClearCache() {
  if (webkit_initialized_)
    blink::WebCache::clear();
}

void AwRenderProcessObserver::OnSetJsOnlineProperty(bool network_up) {
  if (webkit_initialized_)
    blink::WebNetworkStateNotifier::setOnLine(network_up);
}

}  // nanemspace android_webview
