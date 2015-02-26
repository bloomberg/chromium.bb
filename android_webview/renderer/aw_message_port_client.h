// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_MESSAGE_PORT_CLIENT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_MESSAGE_PORT_CLIENT_H_

#include <vector>

#include "base/strings/string16.h"
#include "content/public/renderer/render_frame_observer.h"

namespace android_webview {

// Renderer side of Android webview specific message port service. This service
// is used to convert messages from WebSerializedScriptValue to a base value.
class AwMessagePortClient : public content::RenderFrameObserver {
 public:
  explicit AwMessagePortClient(content::RenderFrame* render_frame);

 private:
  ~AwMessagePortClient() override;

  // RenderFrameObserver
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnWebToAppMessage(int message_port_id,
                         const base::string16& message,
                         const std::vector<int>& sent_message_port_ids);
  void OnAppToWebMessage(int message_port_id,
                         const base::string16& message,
                         const std::vector<int>& sent_message_port_ids);
  void OnClosePort(int message_port_id);

  DISALLOW_COPY_AND_ASSIGN(AwMessagePortClient);
};

}

#endif  // ANDROID_WEBVIEW_RENDERER_AW_MESSAGE_PORT_CLIENT_H_
