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

  // Builds an observer for |view| and |layout|. This method returns NULL on
  // platforms which don't need configuration.
  static OpaqueBrowserFrameViewPlatformSpecific* Create(
      OpaqueBrowserFrameView* view,
      OpaqueBrowserFrameViewLayout* layout);

  // Whether we should show the (minimize,maximize,close) buttons. This can
  // depend on the current state of the window (e.g., whether it is maximized).
  // The default implementation always returns true.
  virtual bool ShouldShowCaptionButtons() const;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_PLATFORM_SPECIFIC_H_
