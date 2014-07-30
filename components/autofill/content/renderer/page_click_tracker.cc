// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/page_click_tracker.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/page_click_listener.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDOMMouseEvent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebTextAreaElement.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebDOMEvent;
using blink::WebDOMMouseEvent;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFrame;
using blink::WebGestureEvent;
using blink::WebInputElement;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebNode;
using blink::WebString;
using blink::WebTextAreaElement;
using blink::WebView;

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

PageClickTracker::PageClickTracker(content::RenderView* render_view,
                                   PageClickListener* listener)
    : content::RenderViewObserver(render_view),
      was_focused_before_now_(false),
      listener_(listener),
      weak_ptr_factory_(this) {
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

void PageClickTracker::DidHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  if (event.type != blink::WebGestureEvent::GestureTap)
    return;

  PotentialActivationAt(event.x, event.y);
}

void PageClickTracker::FocusedNodeChanged(const blink::WebNode& node) {
  was_focused_before_now_ = false;
  // If the focus change was a result of handling a click or tap, we'll soon get
  // an associated event. Reset |was_focused_before_now_| to true only after the
  // message loop unwinds.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PageClickTracker::SetWasFocused,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PageClickTracker::PotentialActivationAt(int x, int y) {
  blink::WebNode focused_node = render_view()->GetFocusedElement();
  if (focused_node.isNull())
    return;

  if (!render_view()->NodeContainsPoint(focused_node, gfx::Point(x, y)))
    return;

  const WebInputElement input_element = GetTextWebInputElement(focused_node);
  if (!input_element.isNull()) {
    listener_->FormControlElementClicked(input_element,
                                         was_focused_before_now_);
    return;
  }

  const WebTextAreaElement textarea_element =
      GetWebTextAreaElement(focused_node);
  if (!textarea_element.isNull()) {
    listener_->FormControlElementClicked(textarea_element,
                                         was_focused_before_now_);
  }
}

void PageClickTracker::SetWasFocused() {
  was_focused_before_now_ = true;
}

}  // namespace autofill
