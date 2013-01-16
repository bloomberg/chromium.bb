// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/stacked_panel_collection.h"

#include <algorithm>
#include "base/logging.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"

StackedPanelCollection::StackedPanelCollection(PanelManager* panel_manager)
    : PanelCollection(PanelCollection::STACKED),
      panel_manager_(panel_manager) {
}

StackedPanelCollection::~StackedPanelCollection() {
  DCHECK(panels_.empty());
}

void StackedPanelCollection::OnDisplayAreaChanged(
    const gfx::Rect& old_display_area) {
}

void StackedPanelCollection::RefreshLayout() {
}

void StackedPanelCollection::AddPanel(Panel* panel,
                                      PositioningMask positioning_mask) {
  DCHECK_NE(this, panel->collection());
  panel->set_collection(this);
  panels_.push_back(panel);

  RefreshLayout();
}

void StackedPanelCollection::RemovePanel(Panel* panel) {
  panel->set_collection(NULL);
  panels_.remove(panel);

  RefreshLayout();
}

void StackedPanelCollection::CloseAll() {
  // Make a copy as closing panels can modify the iterator.
  Panels panels_copy = panels_;

  for (Panels::const_iterator iter = panels_copy.begin();
       iter != panels_copy.end(); ++iter)
    (*iter)->Close();
}

void StackedPanelCollection::OnPanelAttentionStateChanged(Panel* panel) {
}

void StackedPanelCollection::OnPanelTitlebarClicked(
    Panel* panel, panel::ClickModifier modifier) {
}

void StackedPanelCollection::ResizePanelWindow(
    Panel* panel,
    const gfx::Size& preferred_window_size) {
}

void StackedPanelCollection::ActivatePanel(Panel* panel) {
}

void StackedPanelCollection::MinimizePanel(Panel* panel) {
}

void StackedPanelCollection::RestorePanel(Panel* panel) {
}

void StackedPanelCollection::MinimizeAll() {
}

void StackedPanelCollection::RestoreAll() {
}

bool StackedPanelCollection::CanMinimizePanel(const Panel* panel) const {
  return true;
}

bool StackedPanelCollection::IsPanelMinimized(const Panel* panel) const {
  return false;
}

void StackedPanelCollection::SavePanelPlacement(Panel* panel) {
}

void StackedPanelCollection::RestorePanelToSavedPlacement() {
}

void StackedPanelCollection::DiscardSavedPanelPlacement() {
}

panel::Resizability StackedPanelCollection::GetPanelResizability(
    const Panel* panel) const {
  return panel::RESIZABLE_ALL_SIDES;
}

void StackedPanelCollection::OnPanelResizedByMouse(
    Panel* panel, const gfx::Rect& new_bounds) {
}

bool StackedPanelCollection::HasPanel(Panel* panel) const {
  return std::find(panels_.begin(), panels_.end(), panel) != panels_.end();
}

void StackedPanelCollection::UpdatePanelOnCollectionChange(Panel* panel) {
}

void StackedPanelCollection::OnPanelActiveStateChanged(Panel* panel) {
}

Panel* StackedPanelCollection::GetPanelAbove(Panel* panel) const {
  DCHECK(panel);

  if (panels_.size() < 2)
    return NULL;
  Panels::const_iterator iter = panels_.begin();
  Panel* above_panel = *iter;
  for (; iter != panels_.end(); ++iter) {
    if (*iter == panel)
      return above_panel;
    above_panel = *iter;
  }
  return NULL;
}
