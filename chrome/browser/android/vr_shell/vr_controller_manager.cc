// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/android/vr_shell/vr_controller_manager.h"
#include "content/public/browser/browser_thread.h"

namespace vr_shell {

VrControllerManager::VrControllerManager() {}

VrControllerManager::~VrControllerManager() {}

void VrControllerManager::SetWebContents(content::WebContents* content_wc_ptr,
                                         content::WebContents* ui_wc_ptr) {
  vr_content_ptr_ = new VrInputManager(content_wc_ptr);
  vr_ui_ptr_ = new VrInputManager(ui_wc_ptr);
}

std::unique_ptr<VrController> VrControllerManager::InitializeController(
    gvr_context* gvr_context) {
  return std::unique_ptr<VrController>(new VrController(gvr_context));
}

void VrControllerManager::ProcessUpdatedContentGesture(VrGesture gesture) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VrControllerManager::SendGesture, this, gesture,
                   vr_content_ptr_));
  } else {
    SendGesture(gesture, vr_content_ptr_);
  }
}

void VrControllerManager::ProcessUpdatedUIGesture(VrGesture gesture) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VrControllerManager::SendGesture, this, gesture,
                   vr_ui_ptr_));
  } else {
    SendGesture(gesture, vr_ui_ptr_);
  }
}

void VrControllerManager::SendGesture(VrGesture gesture,
                                      VrInputManager* vr_input_manager) {
  int64_t event_time = gesture.start_time;
  int64_t event_time_milliseconds = static_cast<int64_t>(event_time / 1000000);

  if (gesture.type == WebInputEvent::GestureScrollBegin ||
      gesture.type == WebInputEvent::GestureScrollUpdate ||
      gesture.type == WebInputEvent::GestureScrollEnd) {
    vr_input_manager->SendScrollEvent(
        event_time_milliseconds, 0.0f, 0.0f, gesture.details.scroll.delta.x,
        gesture.details.scroll.delta.y, gesture.type);
  } else if (gesture.type == WebInputEvent::GestureTap) {
    vr_input_manager->SendClickEvent(event_time_milliseconds,
                                     gesture.details.buttons.pos.x,
                                     gesture.details.buttons.pos.y);
  } else if (gesture.type == WebInputEvent::MouseMove) {
    vr_input_manager->SendMouseMoveEvent(
        event_time_milliseconds, gesture.details.move.delta.x,
        gesture.details.move.delta.y, gesture.details.move.type);
  }
}

}  // namespace vr_shell
