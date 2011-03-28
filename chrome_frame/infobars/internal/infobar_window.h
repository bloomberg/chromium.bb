// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_INFOBARS_INTERNAL_INFOBAR_WINDOW_H_
#define CHROME_FRAME_INFOBARS_INTERNAL_INFOBAR_WINDOW_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"

#include "chrome_frame/infobars/infobar_content.h"
#include "chrome_frame/infobars/infobar_manager.h"

struct FunctionStub;

// Manages the display of an InfobarContent instance within a container window.
// Positions the infobar content by displacing other "natural" content of the
// window (see ReserveSpace). Allows positioning either above or below the
// natural content.
class InfobarWindow {
 public:
  // Integrates the InfobarWindow with its environment.
  class Host {
   public:
    virtual ~Host() {}

    // Returns a handle to the window within which infobar content should be
    // created. All windows associated with the infobar content should be
    // descendants of the container window.
    virtual HWND GetContainerWindow() = 0;

    // Triggers an immediate re-evaluation of the dimensions of the displaced
    // content. InfobarWindow::ReserveSpace will be called with the natural
    // dimensions of the displaced content.
    virtual void UpdateLayout() = 0;
  };  // class Host

  explicit InfobarWindow(InfobarType type);
  ~InfobarWindow();

  void SetHost(Host* host);

  // Shows the supplied content in this InfobarWindow. Normally,
  // InfobarContent::InstallInFrame will be called with an InfobarContent::Frame
  // instance the content may use to interact with the InfobarWindow.
  //
  // InfobarContent is deleted when the InfobarWindow is finished with the
  // content (either through failure or when successfully hidden).
  bool Show(InfobarContent* content);

  // Hides the infobar.
  void Hide();

  // Receives the total space requested by the displaced content and subtracts
  // any space required by this infobar. Passes the reserved dimensions to
  // InfobarContent::SetDimensions.
  void ReserveSpace(RECT* rect);

 private:
  // Provides InfobarContent with access to this InfobarWindow.
  class FrameImpl : public InfobarContent::Frame {
   public:
    explicit FrameImpl(InfobarWindow* infobar_window);

    // InfobarContent::Frame implementation
    virtual HWND GetFrameWindow();
    virtual void CloseInfobar();

   private:
    InfobarWindow* infobar_window_;
    DISALLOW_COPY_AND_ASSIGN(FrameImpl);
  };  // class FrameImpl

  // Sets up our state to show or hide and calls Host::UpdateLayout to cause a
  // call to ReserveSpace. Sets up a timer to periodically call UpdateLayout.
  void StartSlidingTowards(int height);

  // Based on the initial height, how long (and if) we have been sliding, and
  // the target height, decides what the current height should be.
  int CalculateHeight();

  // Manage a timer that repeatedly calls Host::UpdateLayout
  bool StartTimer();
  bool StopTimer();

  scoped_ptr<InfobarContent> content_;
  Host* host_;
  FrameImpl frame_impl_;
  InfobarType type_;
  int current_width_;
  int current_height_;

  // These variables control our state for sliding and are used to calculate
  // the desired height at any given time.
  base::Time slide_start_;  // When we started sliding, or the null time if we
                            // are not sliding.
  int initial_height_;      // Where we started sliding from
  int target_height_;       // Where we are sliding to

  // ID and thunk for the slide-effect timer
  int timer_id_;
  FunctionStub* timer_stub_;

  DISALLOW_COPY_AND_ASSIGN(InfobarWindow);
};  // class InfobarWindow

#endif  // CHROME_FRAME_INFOBARS_INTERNAL_INFOBAR_WINDOW_H_
