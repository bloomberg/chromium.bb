// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_MESSAGE_WINDOW_H_
#define BASE_WIN_MESSAGE_WINDOW_H_

#include <windows.h>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"

namespace base {
namespace win {

// Implements a message-only window.
class BASE_EXPORT MessageWindow : public base::NonThreadSafe {
 public:
  // Handles incoming window messages.
  class BASE_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    virtual bool HandleMessage(HWND hwnd,
                               UINT message,
                               WPARAM wparam,
                               LPARAM lparam,
                               LRESULT* result) = 0;
  };

  MessageWindow();
  ~MessageWindow();

  // Creates a message-only window. The incoming messages will be handled by
  // |delegate|. |delegate| must outlive |this|. |window_name| is optional and
  // can be NULL.
  bool Create(Delegate* delegate, const wchar_t* window_name);

  HWND hwnd() const { return window_; }

 private:
  // Invoked by the OS to process incoming window messages.
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                                     LPARAM lparam);

  // Atom representing the registered window class.
  ATOM atom_;

  // Handle of the input window.
  HWND window_;

  DISALLOW_COPY_AND_ASSIGN(MessageWindow);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_MESSAGE_WINDOW_H_
