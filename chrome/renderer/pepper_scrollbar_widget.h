// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_SCROLLBAR_WIDGET_H_
#define CHROME_RENDERER_PEPPER_SCROLLBAR_WIDGET_H_

#include "base/timer.h"
#include "build/build_config.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "chrome/renderer/pepper_widget.h"

// An implementation of a horizontal/vertical scrollbar.
// TODO(jam): this code is copied from WebKit/Chromium, use WebCore::Scrollbar
// instead.
class PepperScrollbarWidget : public PepperWidget {
 public:
  PepperScrollbarWidget();
  ~PepperScrollbarWidget();

  virtual void Destroy();
  virtual void Paint(Graphics2DDeviceContext* context, const NPRect& dirty);
  virtual bool HandleEvent(const NPPepperEvent& event);
  virtual void GetProperty(NPWidgetProperty property, void* value);
  virtual void SetProperty(NPWidgetProperty property, void* value);

#if defined(OS_LINUX)
  static void SetScrollbarColors(unsigned inactive_color,
                                 unsigned active_color,
                                 unsigned track_color);
#endif

 private:
  enum ScrollGranularity {
    SCROLL_BY_LINE,
    SCROLL_BY_PAGE,
    SCROLL_BY_DOCUMENT,
    SCROLL_BY_PIXELS,
  };

  enum Part {
    DOWN_ARROW,
    LEFT_ARROW,
    RIGHT_ARROW,
    UP_ARROW,
    HORIZONTAL_THUMB,
    VERTICAL_THUMB,
    HORIZONTAL_TRACK,
    VERTICAL_TRACK,
  };

#if defined(OS_WIN)
  int GetThemeState(Part part) const;
  int GetThemeArrowState(Part part) const;
  int GetClassicThemeState(Part part) const;
  bool IsVistaOrNewer() const;
#endif

  // Length is the size in pixels of the scroll bar, while total_length is the
  // length of the scrollable document.
  void SetLocation(const gfx::Point& location, int length, int total_length);
  gfx::Rect GetLocation() const;

  // Returns the offset into the scrollbar document.
  int GetPosition() const;

  // Called when the timer is fired.
  void OnTimerFired();

  // Scroll the document by the given amount.  pixels is only used when the
  // granularity is SCROLL_BY_PIXELS.
  void Scroll(bool backwards, ScrollGranularity granularity, int pixels);

  // Scroll to the given location in document pixels.
  void ScrollTo(int position);

  // Calculate one-time measurements on the components of the scroll bar.
  void GenerateMeasurements();

  // Mouse/keyboard event handlers.
  bool OnMouseDown(const NPPepperEvent& event);
  bool OnMouseUp(const NPPepperEvent& event);
  bool OnMouseMove(const NPPepperEvent& event);
  bool OnMouseLeave(const NPPepperEvent& event);
  bool OnMouseWheel(const NPPepperEvent& event);
  bool OnKeyDown(const NPPepperEvent& event);
  bool OnKeyUp(const NPPepperEvent& event);

  // If the given location is over a part, return its id, otherwise -1.
  int HitTest(const gfx::Point& location) const;

  // When dragging the thumb, on some platforms if the user moves the mouse too
  // far then the thumb snaps back to its original location.
  bool ShouldSnapBack(const gfx::Point& location) const;

  // Returns true if we should center the thumb on the track.
  bool ShouldCenterOnThumb(const NPPepperEvent& event) const;

  // Given a coordinate, returns the position as an offset from the beginning of
  // the track.
  int ThumbPosition(const gfx::Point& location) const;

  // Moves the thumb and sets its location to the given position.
  void MoveThumb(int new_position);

  // Sets the position and optionally calls the client to inform them of a new
  // position.
  void SetPosition(float position, bool notify_client);

  // Handle scrolling when the mouse is pressed over the track/arrow.
  void AutoScroll(float delay);

  // If the mouse is pressed over an arrow or track area, starts a timer so that
  // we scroll another time.
  void StartTimerIfNeeded(float delay);

  // Cancel the timer if it's running.
  void StopTimerIfNeeded();

  // Getters for location and size of arrows, thumb, and track.
  gfx::Rect BackArrowRect() const;  // Up/left arrow.
  gfx::Rect ForwardArrowRect() const;  // Down/right arrow.
  gfx::Rect BackTrackRect() const;  // Track before the thumb.
  gfx::Rect ForwardTrackRect() const;  // Track after the thumb.
  gfx::Rect TrackRect() const;  // Entire track.
  gfx::Rect ThumbRect() const;  // Thumb.

  // Returns true if the back part of the track is pressed.  Returning false
  // does not necessarily mean the forward part is pressed.
  bool IsBackTrackPressed() const;

  // Returns true if the the trackbar was pressed and the thumb has moved enough
  // that it's now over the pressed position.
  bool DidThumbCatchUpWithMouse();

  // Returns true if the thumb is under the mouse.
  bool IsThumbUnderMouse() const;

  // Returns true if the up/left/back track are pressed.
  bool IsBackscrolling() const;

  // How many pixels is the track length (i.e. without the arrows).
  int TrackLength() const;

  // The length of the thumb in pixels.
  int ThumbLength() const;

  // Minimium number of pixels for the thumb.
  int MinimumThumbLength() const;

  // Maximum offst of the thumb in pixels from the beginning of the track.
  int MaximumThumbPosition() const;

  // Converts between pixels in the document to pixels of the thumb position.
  float ThumbPixelsFromDocumentPixels(int pixels) const;

  // Helpers for mapping between theme part IDs and their functionality.
  static bool IsArrow(int part);
  static bool IsTrackbar(int part);
  static bool IsThumb(int part);

  bool vertical_;
  bool enabled_;
  gfx::Point location_;
  // Length of the scroll bar.
  int length_;
  // Length of the document being scrolled.
  int total_length_;
  // Calculated value, based on the length of the scrollbar and document, of how
  // many pixels to advance when the user clicks on the trackbar.
  int pixels_per_page_;

  // Values we calculated on construction.
  // How wide/high a vertical/horizontal scrollbar is.
  int thickness_;
  int arrow_length_;

  // -1 if no parts are hovered or pressed.  Otherwise a value from Part.
  int hovered_part_;
  int pressed_part_;

  // How many pixels from the beginning of the track the thumb starts.
  // This is a float because in case of a very large document, clicking the
  // arrow button might not be enough to advance the thumb by one pixel.  If
  // this was an int, then clicking it repeatedly wouldn't move the thumb.
  float thumb_position_;

  // True if we have mouse capture.
  bool have_mouse_capture_;
  // If we have mouse capture and the user clicked on a trackbar, where they
  // started their click relative to the beginning of the track.
  float drag_origin_;
  // The current offset relative to the beginning of the track of the last
  // mouse down.  drag_origin_ isn't enough because when the user is dragging
  // the thumb, we get events even when they stop moving the mouse, and so we
  // need to know if the mouse moved since the last time that we moved the
  // thumb.
  int pressed_position_;

  // Used when holding down arrows or tracks.
  base::OneShotTimer<PepperScrollbarWidget> timer_;

  gfx::Rect dirty_rect_;

  // How many pixels to advance in the document when pressing the arrows.
  static const int kPixelsPerLine;

  // Used to calculate how many pixels to advance in the document when clicking
  // the trackbar.
  static const int kMaxOverlapBetweenPages;
  static const float kMinFractionToStepWhenPaging;

  // Used when holding down arrows/track.
  static const float kInitialAutoscrollTimerDelay;
  static const float kAutoscrollTimerDelay;

  DISALLOW_COPY_AND_ASSIGN(PepperScrollbarWidget);
};

#endif  // CHROME_RENDERER_PEPPER_SCROLLBAR_WIDGET_H_
