// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PAGE_CLICK_TRACKER_H_
#define CHROME_RENDERER_PAGE_CLICK_TRACKER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDOMEventListener.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"

class PageClickListener;
class RenderView;

namespace WebKit {
class WebFrame;
class WebDOMEvent;
class WebMouseEvent;
}

// This class is responsible for tracking clicks on elements in web pages and
// notifiying the associated listener when a node is clicked.
// Compared to a simple WebDOMEventListener, it offers the added capability of
// notifying the listeners of whether the clicked node was already focused
// before it was clicked.
//
// This is useful for password/form autofill where we want to trigger a
// suggestion popup when a text input is clicked.
// It only notifies of WebInputElement that are text inputs being clicked, but
// could easily be changed to report click on any type of WebNode.
//
// There is one PageClickTracker per RenderView.

class PageClickTracker : public WebKit::WebDOMEventListener {
 public:
  explicit PageClickTracker(RenderView* render_view);
  virtual ~PageClickTracker();

  // Starts reporting node clicks for |frame| on the listener previously
  // specified with SetListener().
  void StartTrackingFrame(WebKit::WebFrame* frame);

  // Stops reporting node clicks for |frame|.  |frame_detached| should be true
  // if the frame has already been detached.
  void StopTrackingFrame(WebKit::WebFrame* frame, bool frame_detached);

  // Called after the mouse event |event| has been processed by WebKit.
  void DidHandleMouseEvent(const WebKit::WebMouseEvent& event);

  // Adds/removes a listener for getting notification when an element is
  // clicked.  Note that the order of insertion is important as a listener when
  // notified can decide to stop the propagation of the event (so that listeners
  // inserted after don't get the notification).
  void AddListener(PageClickListener* listener);
  void RemoveListener(PageClickListener* listener);

 private:
  // WebKit::WebDOMEventListener implementation.
  virtual void handleEvent(const WebKit::WebDOMEvent& event);

  // Returns the currently focused node in the associated render view.
  // That node may be null.
  WebKit::WebNode GetFocusedNode();

  // The last node that was clicked and had focus.
  WebKit::WebNode last_node_clicked_;

  // The render view we are associated with.
  RenderView* render_view_;

  // Whether the last clicked node had focused before it was clicked.
  bool was_focused_;

  // The frames we are listening to for mouse events.
  typedef std::vector<WebKit::WebFrame*> FrameList;
  FrameList tracked_frames_;

  // The listener getting the actual notifications.
  ObserverList<PageClickListener> listeners_;

  DISALLOW_COPY_AND_ASSIGN(PageClickTracker);
};

#endif  // CHROME_RENDERER_PAGE_CLICK_TRACKER_H_
