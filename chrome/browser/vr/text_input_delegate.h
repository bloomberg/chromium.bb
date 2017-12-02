// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEXT_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_TEXT_INPUT_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"

namespace vr {

struct TextInputInfo;

class TextInputDelegate {
 public:
  TextInputDelegate();
  virtual ~TextInputDelegate();

  // RequestFocusCallback gets called when an element request's focus.
  typedef base::RepeatingCallback<void(int)> RequestFocusCallback;
  // UpdateInputCallback gets called when the text input info changes for the
  // element being edited.
  typedef base::RepeatingCallback<void(const TextInputInfo&)>
      UpdateInputCallback;

  void SetRequestFocusCallback(const RequestFocusCallback& callback);
  void SetUpdateInputCallback(const UpdateInputCallback& callback);

  virtual void RequestFocus(int element_id);
  virtual void UpdateInput(const TextInputInfo& info);

 private:
  RequestFocusCallback request_focus_callback_;
  UpdateInputCallback update_input_callback_;

  DISALLOW_COPY_AND_ASSIGN(TextInputDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEXT_INPUT_DELEGATE_H_
