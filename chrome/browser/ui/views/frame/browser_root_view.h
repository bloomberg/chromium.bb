// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_

#include "ui/views/widget/root_view.h"

class BrowserView;
class TabStrip;

namespace ui {
class OSExchangeData;
}

// RootView implementation used by BrowserFrame. This forwards drop events to
// the TabStrip. Visually the tabstrip extends to the top of the frame, but in
// actually it doesn't. The tabstrip is only as high as a tab. To enable
// dropping above the tabstrip BrowserRootView forwards drop events to the
// TabStrip.
class BrowserRootView : public views::internal::RootView {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // You must call set_tabstrip before this class will accept drops.
  BrowserRootView(BrowserView* browser_view, views::Widget* widget);

  // Overridden from views::View:
  virtual bool GetDropFormats(
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool AreDropTypesRequired() OVERRIDE;
  virtual bool CanDrop(const ui::OSExchangeData& data) OVERRIDE;
  virtual void OnDragEntered(const ui::DropTargetEvent& event) OVERRIDE;
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragExited() OVERRIDE;
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

  // Overridden from internal::RootView:
  virtual void DispatchGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  // Returns true if the event should be forwarded to the tabstrip.
  bool ShouldForwardToTabStrip(const ui::DropTargetEvent& event);

  // Converts the event from the hosts coordinate system to the tabstrips
  // coordinate system.
  ui::DropTargetEvent* MapEventToTabStrip(
      const ui::DropTargetEvent& event,
      const ui::OSExchangeData& data);

  inline TabStrip* tabstrip() const;

  // Returns true if |data| has string contents and the user can "paste and go".
  // If |url| is non-NULL and the user can "paste and go", |url| is set to the
  // desired destination.
  bool GetPasteAndGoURL(const ui::OSExchangeData& data, GURL* url);

  // The BrowserView.
  BrowserView* browser_view_;

  // If true, drag and drop events are being forwarded to the tab strip.
  // This is used to determine when to send OnDragEntered and OnDragExited
  // to the tab strip.
  bool forwarding_to_tab_strip_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRootView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_
