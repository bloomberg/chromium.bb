// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_INFOBARS_INFOBAR_CONTENT_H_
#define CHROME_FRAME_INFOBARS_INFOBAR_CONTENT_H_

#include <windows.h>

// Provides an interface between content to be displayed in an infobar and the
// infobar facility. Pass an instance of your implementation to
// InfobarManager::Show, which will result in a call to InstallInFrame to
// initialize the InfobarContent.
//
// The instance will be deleted by the infobar facility when it is no longer
// being displayed (or immediately, in the case of a failure to display).
class InfobarContent {
 public:
  // Provides access to the content's parent window and allows the
  // InfobarContent to close itself, such as in response to user interaction.
  class Frame {
   public:
    virtual ~Frame() {}

    // Returns the window in which the content should display itself.
    virtual HWND GetFrameWindow() = 0;

    // Initiates closing of the infobar.
    virtual void CloseInfobar() = 0;
  };  // class Frame

  virtual ~InfobarContent() {}

  // Prepares the content to display in the provided frame.
  //
  // The frame pointer remains valid until the InfobarContent instance is
  // deleted.
  virtual bool InstallInFrame(Frame* frame) = 0;

  // Provides the content with the dimensions available to it for display.
  // Dimensions are relative to the origin of the frame window.
  virtual void SetDimensions(const RECT& dimensions) = 0;

  // Finds the desired value for one dimension given a fixed value for the other
  // dimension. The fixed dimension parameter is non-zero whereas the requested
  // dimension parameter will be zero.
  virtual size_t GetDesiredSize(size_t width, size_t height) = 0;
};  // class InfobarContent

#endif  // CHROME_FRAME_INFOBARS_INFOBAR_CONTENT_H_
