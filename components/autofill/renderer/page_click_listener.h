// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_RENDERER_PAGE_CLICK_LISTENER_H_
#define COMPONENTS_AUTOFILL_RENDERER_PAGE_CLICK_LISTENER_H_

namespace WebKit {
class WebInputElement;
}

// Interface that should be implemented by classes interested in getting
// notifications for clicks on a page.
// Register on the PageListenerTracker object.
class PageClickListener {
 public:
  // Notification that |element| was clicked.
  // |was_focused| is true if |element| had focus BEFORE the click.
  // |is_focused| is true if |element| has focus AFTER the click was processed.
  // If this method returns true, the notification will not be propagated to
  // other listeners.
  virtual bool InputElementClicked(const WebKit::WebInputElement& element,
                                   bool was_focused,
                                   bool is_focused) = 0;

  // If the previously focused element was an input field, listeners are
  // informed that the text field has lost its focus.
  // If this method returns true, the notification will not be propagated to
  // other listeners.
  virtual bool InputElementLostFocus() = 0;

 protected:
  virtual ~PageClickListener() {}
};

#endif  // COMPONENTS_AUTOFILL_RENDERER_PAGE_CLICK_LISTENER_H_
