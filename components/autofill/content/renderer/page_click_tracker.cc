// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/page_click_tracker.h"

#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/page_click_listener.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebTextAreaElement.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebElement;
using blink::WebGestureEvent;
using blink::WebInputElement;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebNode;
using blink::WebTextAreaElement;

namespace {

// Casts |node| to a WebInputElement.
// Returns an empty (isNull()) WebInputElement if |node| is not a text field.
const WebInputElement GetTextWebInputElement(const WebNode& node) {
  if (!node.isElementNode())
    return WebInputElement();
  const WebElement element = node.toConst<WebElement>();
  if (!element.hasHTMLTagName("input"))
    return WebInputElement();
  const WebInputElement* input = blink::toWebInputElement(&element);
  if (!autofill::IsTextInput(input))
    return WebInputElement();
  return *input;
}

// Casts |node| to a WebTextAreaElement.
// Returns an empty (isNull()) WebTextAreaElement if |node| is not a
// textarea field.
const WebTextAreaElement GetWebTextAreaElement(const WebNode& node) {
  if (!node.isElementNode())
    return WebTextAreaElement();
  const WebElement element = node.toConst<WebElement>();
  if (!element.hasHTMLTagName("textarea"))
    return WebTextAreaElement();
  return element.toConst<WebTextAreaElement>();
}

}  // namespace

namespace autofill {

PageClickTracker::PageClickTracker(content::RenderFrame* render_frame,
                                   PageClickListener* listener)
    : content::RenderFrameObserver(render_frame),
      focused_node_was_last_clicked_(false),
      was_focused_before_now_(false),
      listener_(listener),
      legacy_(this) {
}

PageClickTracker::~PageClickTracker() {
}

void PageClickTracker::DidHandleMouseEvent(const WebMouseEvent& event) {
  if (event.type != WebInputEvent::MouseDown ||
      event.button != WebMouseEvent::ButtonLeft) {
    return;
  }

  PotentialActivationAt(event.x, event.y);
}

void PageClickTracker::DidHandleGestureEvent(const WebGestureEvent& event) {
  if (event.type != WebGestureEvent::GestureTap)
    return;

  PotentialActivationAt(event.x, event.y);
}

void PageClickTracker::FocusedNodeChanged(const WebNode& node) {
  was_focused_before_now_ = false;
}

void PageClickTracker::FocusChangeComplete() {
  blink::WebNode focused_node = render_frame()->GetFocusedElement();
  if (focused_node_was_last_clicked_ && !focused_node.isNull()) {
    const WebInputElement input_element = GetTextWebInputElement(focused_node);
    if (!input_element.isNull()) {
      listener_->FormControlElementClicked(input_element,
                                           was_focused_before_now_);
    } else {
      const WebTextAreaElement textarea_element =
          GetWebTextAreaElement(focused_node);
      if (!textarea_element.isNull()) {
        listener_->FormControlElementClicked(textarea_element,
                                             was_focused_before_now_);
      }
    }
  }

  was_focused_before_now_ = true;
  focused_node_was_last_clicked_ = false;
}

void PageClickTracker::PotentialActivationAt(int x, int y) {
  blink::WebElement focused_element = render_frame()->GetFocusedElement();
  if (focused_element.isNull())
    return;

  if (!GetScaledBoundingBox(
           render_frame()->GetRenderView()->GetWebView()->pageScaleFactor(),
           &focused_element).Contains(x, y)) {
    return;
  }

  focused_node_was_last_clicked_ = true;
}

// PageClickTracker::Legacy ----------------------------------------------------

PageClickTracker::Legacy::Legacy(PageClickTracker* tracker)
    : content::RenderViewObserver(tracker->render_frame()->GetRenderView()),
      tracker_(tracker) {
}

void PageClickTracker::Legacy::OnDestruct() {
  // No-op. Don't delete |this|.
}

void PageClickTracker::Legacy::DidHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  tracker_->DidHandleMouseEvent(event);
}

void PageClickTracker::Legacy::DidHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  tracker_->DidHandleGestureEvent(event);
}

void PageClickTracker::Legacy::FocusChangeComplete() {
  tracker_->FocusChangeComplete();
}

}  // namespace autofill
