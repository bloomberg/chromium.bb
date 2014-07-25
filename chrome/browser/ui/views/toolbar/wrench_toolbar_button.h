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

class ToolbarView;

// TODO(gbillock): Rename this? No longer a wrench.
class WrenchToolbarButton : public views::MenuButton,
                            public WrenchIconPainter::Delegate {
 public:
  explicit WrenchToolbarButton(ToolbarView* toolbar_view);
  virtual ~WrenchToolbarButton();

  void SetSeverity(WrenchIconPainter::Severity severity, bool animate);

  // views::MenuButton:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // WrenchIconPainter::Delegate:
  virtual void ScheduleWrenchIconPaint() OVERRIDE;

  // Opens the wrench menu immediately during a drag-and-drop operation.
  // Used only in testing.
  static bool g_open_wrench_immediately_for_testing;

 private:
  // views::View:
  virtual bool GetDropFormats(int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool AreDropTypesRequired() OVERRIDE;
  virtual bool CanDrop(const ui::OSExchangeData& data) OVERRIDE;
  virtual void OnDragEntered(const ui::DropTargetEvent& event) OVERRIDE;
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragExited() OVERRIDE;
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE;

  // Show the extension action overflow menu (which is in the app menu).
  void ShowOverflowMenu();

  scoped_ptr<WrenchIconPainter> wrench_icon_painter_;

  // Our owning toolbar view.
  ToolbarView* toolbar_view_;

  // Whether or not we should allow dragging extension icons onto this button
  // (in order to open the overflow in the app/wrench menu).
  bool allow_extension_dragging_;

  // Used to spawn weak pointers for delayed tasks to open the overflow menu.
  base::WeakPtrFactory<WrenchToolbarButton> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WrenchToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_TOOLBAR_BUTTON_H_
