// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_TOOLBAR_BUTTON_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/toolbar/wrench_icon_painter.h"
#include "ui/views/controls/button/menu_button.h"

namespace views {
class LabelButtonBorder;
}

class ToolbarView;

// TODO(gbillock): Rename this? No longer a wrench.
class WrenchToolbarButton : public views::MenuButton,
                            public WrenchIconPainter::Delegate {
 public:
  explicit WrenchToolbarButton(ToolbarView* toolbar_view);
  ~WrenchToolbarButton() override;

  void SetSeverity(WrenchIconPainter::Severity severity, bool animate);

  // views::MenuButton:
  gfx::Size GetPreferredSize() const override;

  // WrenchIconPainter::Delegate:
  void ScheduleWrenchIconPaint() override;

  // Sets |overflowed_toolbar_action_wants_to_run_| and schedules a paint.
  void SetOverflowedToolbarActionWantsToRun(bool wants_to_run);

  bool overflowed_toolbar_action_wants_to_run_for_testing() const {
    return overflowed_toolbar_action_wants_to_run_for_testing_;
  }

  // Opens the wrench menu immediately during a drag-and-drop operation.
  // Used only in testing.
  static bool g_open_wrench_immediately_for_testing;

 private:
  // views::MenuButton:
  const char* GetClassName() const override;
  bool GetDropFormats(
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // Show the extension action overflow menu (which is in the app menu).
  void ShowOverflowMenu();

  scoped_ptr<WrenchIconPainter> wrench_icon_painter_;

  // Our owning toolbar view.
  ToolbarView* toolbar_view_;

  // Whether or not we should allow dragging extension icons onto this button
  // (in order to open the overflow in the app/wrench menu).
  bool allow_extension_dragging_;

  // A flag for whether or not any overflowed toolbar actions want to run.
  // Only needed for testing.
  bool overflowed_toolbar_action_wants_to_run_for_testing_;

  // Used to spawn weak pointers for delayed tasks to open the overflow menu.
  base::WeakPtrFactory<WrenchToolbarButton> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WrenchToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_TOOLBAR_BUTTON_H_
