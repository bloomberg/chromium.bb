// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/detached_panel_strip.h"

#include <algorithm>
#include "base/logging.h"
#include "chrome/browser/ui/panels/panel_manager.h"

DetachedPanelStrip::DetachedPanelStrip(PanelManager* panel_manager)
    : PanelStrip(PanelStrip::DETACHED),
      panel_manager_(panel_manager) {
}

DetachedPanelStrip::~DetachedPanelStrip() {
  DCHECK(panels_.empty());
}

void DetachedPanelStrip::SetDisplayArea(const gfx::Rect& display_area) {
  if (display_area_ == display_area)
    return;
  display_area_ = display_area;

  if (panels_.empty())
    return;

  RefreshLayout();
}

void DetachedPanelStrip::RefreshLayout() {
  NOTIMPLEMENTED();
}

void DetachedPanelStrip::AddPanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  panels_.insert(panel);
}

bool DetachedPanelStrip::RemovePanel(Panel* panel) {
  if (panels_.erase(panel)) {
    panel_manager_->OnPanelRemoved(panel);
    return true;
  }
  return false;
}

void DetachedPanelStrip::CloseAll() {
  // Make a copy as closing panels can modify the iterator.
  Panels panels_copy = panels_;

  for (Panels::const_iterator iter = panels_copy.begin();
       iter != panels_copy.end(); ++iter)
    (*iter)->Close();
}

void DetachedPanelStrip::OnPanelAttentionStateChanged(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  NOTIMPLEMENTED();
}

void DetachedPanelStrip::ResizePanelWindow(
    Panel* panel, const gfx::Size& preferred_window_size) {
  NOTIMPLEMENTED();
}

void DetachedPanelStrip::ActivatePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  NOTIMPLEMENTED();
}

void DetachedPanelStrip::MinimizePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  NOTIMPLEMENTED();
}

void DetachedPanelStrip::RestorePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  NOTIMPLEMENTED();
}

