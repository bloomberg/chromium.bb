// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_PLATFORM_SPECIFIC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_PLATFORM_SPECIFIC_H_

class OpaqueBrowserFrameView;
class OpaqueBrowserFrameViewLayout;

// Handles platform specific configuration concepts.
class OpaqueBrowserFrameViewPlatformSpecific {
 public:
  virtual ~OpaqueBrowserFrameViewPlatformSpecific() {}

  // Builds an observer for |view| and |layout|.
  static OpaqueBrowserFrameViewPlatformSpecific* Create(
      OpaqueBrowserFrameView* view,
      OpaqueBrowserFrameViewLayout* layout);

  // Whether we should show the (minimize,maximize,close) buttons. This can
  // depend on the current state of the window (e.g., whether it is maximized).
  // The default implementation always returns true.
  virtual bool ShouldShowCaptionButtons() const;

  // Whether we should show a title bar at all, for browser windows that do not
  // have a tab strip (e.g., developer tools, popups). The default
  // implementation always returns true.
  virtual bool ShouldShowTitleBar() const;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_PLATFORM_SPECIFIC_H_
