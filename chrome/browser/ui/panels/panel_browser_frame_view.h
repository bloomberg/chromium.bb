// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_FRAME_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

class PanelBrowserView;

class PanelBrowserFrameView : public OpaqueBrowserFrameView {
 public:
  PanelBrowserFrameView(BrowserFrame* frame, PanelBrowserView* browser_view);
  virtual ~PanelBrowserFrameView();

 protected:
  // Overridden from views::View:
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event);

 private:
  // The client view hosted within this non-client frame view that is
  // guaranteed to be freed before the client view.
  // (see comments about view hierarchies in non_client_view.h)
  PanelBrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_FRAME_VIEW_H_
