// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SELECTION_POPUP_CONTROLLER_H_
#define CONTENT_BROWSER_ANDROID_SELECTION_POPUP_CONTROLLER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/touch_selection/selection_event_type.h"

namespace content {

class RenderWidgetHostViewAndroid;

class SelectionPopupController : public RenderWidgetHostConnector {
 public:
  SelectionPopupController(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           WebContents* web_contents);

  // RendetWidgetHostConnector implementation.
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

  // Called from native -> java
  void OnSelectionEvent(ui::SelectionEventType event,
                        const gfx::RectF& selection_rect);
  void OnSelectionChanged(const std::string& text);

 private:
  ~SelectionPopupController() override {}
  JavaObjectWeakGlobalRef java_obj_;
};

bool RegisterSelectionPopupController(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SELECTION_POPUP_CONTROLLER_H_
