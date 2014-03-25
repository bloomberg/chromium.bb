// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_CLICK_LISTENER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_CLICK_LISTENER_H_

namespace blink {
class WebFormControlElement;
}

namespace autofill {

// Interface that should be implemented by classes interested in getting
// notifications for clicks on a page.
// Register on the PageListenerTracker object.
class PageClickListener {
 public:
  // Notification that |element| was clicked.
  // |was_focused| is true if |element| had focus BEFORE the click.
  // |is_focused| is true if |element| has focus AFTER the click was processed.
  virtual void FormControlElementClicked(
      const blink::WebFormControlElement& element,
      bool was_focused) = 0;

  // If the previously focused element was an input field or a textarea,
  // listeners are informed that the text field has lost its focus.
  virtual void FormControlElementLostFocus() = 0;

 protected:
  virtual ~PageClickListener() {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_CLICK_LISTENER_H_
