// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_click_tracker.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/page_click_listener.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMMouseEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDOMEvent;
using WebKit::WebDOMMouseEvent;
using WebKit::WebElement;
using WebKit::WebFormControlElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebNode;
using WebKit::WebString;
using WebKit::WebView;

PageClickTracker::PageClickTracker(RenderView* render_view)
    : RenderViewObserver(render_view),
      was_focused_(false) {
}

PageClickTracker::~PageClickTracker() {
  // Note that even though RenderView calls FrameDetached when notified that
  // a frame was closed, it might not always get that notification from WebKit
  // for all frames.
  // By the time we get here, the frame could have been destroyed so we cannot
  // unregister listeners in frames remaining in tracked_frames_ as they might
  // be invalid.
}

void PageClickTracker::DidHandleMouseEvent(const WebMouseEvent& event) {
  if (event.type != WebInputEvent::MouseDown ||
      last_node_clicked_.isNull()) {
    return;
  }

  // We are only interested in text field clicks.
  if (!last_node_clicked_.isElementNode())
    return;
  const WebElement& element = last_node_clicked_.toConst<WebElement>();
  if (!element.isFormControlElement())
    return;
  const WebFormControlElement& control =
      element.toConst<WebFormControlElement>();
  if (control.formControlType() != WebString::fromUTF8("text"))
    return;

  const WebInputElement& input_element = element.toConst<WebInputElement>();

  bool is_focused = (last_node_clicked_ == GetFocusedNode());
  ObserverListBase<PageClickListener>::Iterator it(listeners_);
  PageClickListener* listener;
  while ((listener = it.GetNext()) != NULL) {
    if (listener->InputElementClicked(input_element, was_focused_, is_focused))
      break;
  }

  last_node_clicked_.reset();
}

void PageClickTracker::AddListener(PageClickListener* listener) {
  listeners_.AddObserver(listener);
}

void PageClickTracker::RemoveListener(PageClickListener* listener) {
  listeners_.RemoveObserver(listener);
}

bool PageClickTracker::OnMessageReceived(const IPC::Message& message) {
  if (message.type() == ViewMsg_HandleInputEvent::ID) {
    void* iter = NULL;
    const char* data;
    int data_length;
    if (message.ReadData(&iter, &data, &data_length)) {
      const WebInputEvent* input_event =
          reinterpret_cast<const WebInputEvent*>(data);
      if (WebInputEvent::isMouseEventType(input_event->type))
        DidHandleMouseEvent(*(static_cast<const WebMouseEvent*>(input_event)));
    }
  }
  return false;
}

void PageClickTracker::DidFinishDocumentLoad(WebKit::WebFrame* frame) {
  tracked_frames_.push_back(frame);
  frame->document().addEventListener("mousedown", this, false);
}

void PageClickTracker::FrameDetached(WebKit::WebFrame* frame) {
  FrameList::iterator iter =
      std::find(tracked_frames_.begin(), tracked_frames_.end(), frame);
  if (iter == tracked_frames_.end()) {
    // Some frames might never load contents so we may not have a listener on
    // them.  Calling removeEventListener() on them would trigger an assert, so
    // we need to keep track of which frames we are listening to.
    return;
  }
  tracked_frames_.erase(iter);
}

void PageClickTracker::handleEvent(const WebDOMEvent& event) {
  last_node_clicked_.reset();

  if (!event.isMouseEvent())
    return;

  const WebDOMMouseEvent mouse_event = event.toConst<WebDOMMouseEvent>();
  DCHECK(mouse_event.buttonDown());
  if (mouse_event.button() != 0)
    return;  // We are only interested in left clicks.

  // Remember which node has focus before the click is processed.
  // We'll get a notification once the mouse event has been processed
  // (DidHandleMouseEvent), we'll notify the listener at that point.
  last_node_clicked_ = mouse_event.target();
  was_focused_ = (GetFocusedNode() == last_node_clicked_);
}

WebNode PageClickTracker::GetFocusedNode() {
  WebView* web_view = render_view()->webview();
  if (!web_view)
    return WebNode();

  WebFrame* web_frame = web_view->focusedFrame();
  if (!web_frame)
    return WebNode();

  return web_frame->document().focusedNode();
}
