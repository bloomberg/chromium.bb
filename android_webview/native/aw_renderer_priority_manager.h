// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_RENDERER_PRIORITY_USER_DATA_H_
#define ANDROID_WEBVIEW_NATIVE_AW_RENDERER_PRIORITY_USER_DATA_H_

#include "base/supports_user_data.h"

namespace content {
class RenderProcessHost;
}

namespace android_webview {

class AwRendererPriorityManager : public base::SupportsUserData::Data {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview.renderer_priority
  enum RendererPriority {
    RENDERER_PRIORITY_INITIAL = -1,
    RENDERER_PRIORITY_WAIVED = 0,
    RENDERER_PRIORITY_LOW = 1,
    RENDERER_PRIORITY_HIGH = 2
  };

  explicit AwRendererPriorityManager(content::RenderProcessHost* host);

  RendererPriority GetRendererPriority() const { return renderer_priority_; }
  void SetRendererPriority(RendererPriority renderer_priority);

 private:
  static void SetRendererPriorityOnLauncherThread(
      int pid,
      RendererPriority renderer_priority);

  content::RenderProcessHost* host_;
  RendererPriority renderer_priority_;
};

}  // namespace android_webview

#endif  //  ANDROID_WEBVIEW_NATIVE_AW_RENDERER_PRIORITY_USER_DATA_H_
