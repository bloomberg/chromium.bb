// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/tabs/touch_tab_strip.h"

#include "chrome/browser/ui/touch/tabs/touch_tab.h"
#include "ui/views/widget/widget.h"

TouchTabStrip::TouchTabStrip(TabStripController* controller)
    : TabStrip(controller) {
}

TouchTabStrip::~TouchTabStrip() {
}

void TouchTabStrip::OnDragEntered(const views::DropTargetEvent& event) {
  // TODO(saintlou): To implement or to ignore?
  NOTIMPLEMENTED();
}

int TouchTabStrip::OnDragUpdated(const views::DropTargetEvent& event) {
  // TODO(saintlou): To implement or to ignore?
  NOTIMPLEMENTED();
  return 0;
}

void TouchTabStrip::OnDragExited() {
  // TODO(saintlou): To implement or to ignore?
  NOTIMPLEMENTED();
}

int TouchTabStrip::OnPerformDrop(const views::DropTargetEvent& event) {
  // TODO(saintlou): To implement or to ignore?
  NOTIMPLEMENTED();
  return 0;
}

BaseTab* TouchTabStrip::CreateTab() {
  return new TouchTab(this);
}

ui::TouchStatus TouchTabStrip::OnTouchEvent(const views::TouchEvent& event) {
  // TODO(saintlou): To implement.
  NOTIMPLEMENTED();
  return ui::TOUCH_STATUS_UNKNOWN;
}
