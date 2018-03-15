// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_

#include <memory>
#include <queue>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/macros.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/text_edit_action.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
}  // namespace blink

namespace gfx {
class PointF;
}  // namespace gfx

namespace vr {

typedef typename base::OnceCallback<void(const base::string16&)>
    TextStateUpdateCallback;

class ContentInputForwarder {
 public:
  virtual ~ContentInputForwarder() {}
  virtual void ForwardEvent(std::unique_ptr<blink::WebInputEvent> event,
                            int content_id) = 0;
  virtual void ForwardDialogEvent(
      std::unique_ptr<blink::WebInputEvent> event) = 0;

  // Text input specific.
  virtual void ClearFocusedElement() = 0;
  virtual void OnWebInputEdited(const TextEdits& edits) = 0;
  virtual void SubmitWebInput() = 0;
  virtual void RequestWebInputText(TextStateUpdateCallback callback) = 0;
};

class PlatformController;

// Receives interaction events with the web content.
class ContentInputDelegate {
 public:
  ContentInputDelegate();
  explicit ContentInputDelegate(ContentInputForwarder* content);
  virtual ~ContentInputDelegate();

  virtual void OnContentEnter(const gfx::PointF& normalized_hit_point);
  virtual void OnContentLeave();
  virtual void OnContentMove(const gfx::PointF& normalized_hit_point);
  virtual void OnContentDown(const gfx::PointF& normalized_hit_point);
  virtual void OnContentUp(const gfx::PointF& normalized_hit_point);

  // Text Input specific.
  void OnFocusChanged(bool focused);
  void OnWebInputEdited(const EditedText& info, bool commit);

  // The following functions are virtual so that they may be overridden in the
  // MockContentInputDelegate.
  VIRTUAL_FOR_MOCKS void OnContentFlingCancel(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point);
  VIRTUAL_FOR_MOCKS void OnContentScrollBegin(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point);
  VIRTUAL_FOR_MOCKS void OnContentScrollUpdate(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point);
  VIRTUAL_FOR_MOCKS void OnContentScrollEnd(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point);

  void OnSwapContents(int new_content_id);

  void OnContentBoundsChanged(int width, int height);

  // This should be called in reponse to selection and composition changes.
  // The given callback will may be called asynchronously with the updated text
  // state. This is because we may have to query content for the text after the
  // index change.
  VIRTUAL_FOR_MOCKS void OnWebInputIndicesChanged(
      int selection_start,
      int selection_end,
      int composition_start,
      int compositon_end,
      base::OnceCallback<void(const TextInputInfo&)> callback);

  void OnPlatformControllerInitialized(PlatformController* controller) {
    controller_ = controller;
  }

  void SetContentInputForwarderForTest(ContentInputForwarder* content) {
    content_ = content;
  }

  void OnWebInputTextChangedForTest(const base::string16& text) {
    OnWebInputTextChanged(text);
  }

 private:
  void UpdateGesture(const gfx::PointF& normalized_content_hit_point,
                     blink::WebGestureEvent& gesture);
  void SendGestureToContent(std::unique_ptr<blink::WebInputEvent> event);
  std::unique_ptr<blink::WebMouseEvent> MakeMouseEvent(
      blink::WebInputEvent::Type type,
      const gfx::PointF& normalized_web_content_location);
  bool ContentGestureIsLocked(blink::WebInputEvent::Type type);
  void OnWebInputTextChanged(const base::string16& text);

  int content_tex_css_width_ = 0;
  int content_tex_css_height_ = 0;
  int content_id_ = 0;
  int locked_content_id_ = 0;

  ContentInputForwarder* content_ = nullptr;
  PlatformController* controller_ = nullptr;

  EditedText last_keyboard_edit_;
  TextInputInfo pending_text_input_info_;
  std::queue<base::OnceCallback<void(const TextInputInfo&)>>
      update_state_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ContentInputDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_
