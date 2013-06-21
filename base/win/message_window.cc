// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/message_window.h"

#include "base/logging.h"
#include "base/process_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/win/wrapped_window_proc.h"

const wchar_t kClassNameFormat[] = L"Chrome_MessageWindow_%p";

namespace base {
namespace win {

MessageWindow::MessageWindow()
    : atom_(0),
      window_(NULL) {
}

MessageWindow::~MessageWindow() {
  DCHECK(CalledOnValidThread());

  if (window_ != NULL) {
    BOOL result = DestroyWindow(window_);
    DCHECK(result);
  }

  if (atom_ != 0) {
    BOOL result = UnregisterClass(
        MAKEINTATOM(atom_),
        base::GetModuleFromAddress(&MessageWindow::WindowProc));
    DCHECK(result);
  }
}

bool MessageWindow::Create(Delegate* delegate, const wchar_t* window_name) {
  DCHECK(CalledOnValidThread());
  DCHECK(!atom_);
  DCHECK(!window_);

  // Register a separate window class for each instance of |MessageWindow|.
  string16 class_name = base::StringPrintf(kClassNameFormat, this);
  HINSTANCE instance = base::GetModuleFromAddress(&MessageWindow::WindowProc);

  WNDCLASSEX window_class;
  window_class.cbSize = sizeof(window_class);
  window_class.style = 0;
  window_class.lpfnWndProc = &base::win::WrappedWindowProc<WindowProc>;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = instance;
  window_class.hIcon = NULL;
  window_class.hCursor = NULL;
  window_class.hbrBackground = NULL;
  window_class.lpszMenuName = NULL;
  window_class.lpszClassName = class_name.c_str();
  window_class.hIconSm = NULL;
  atom_ = RegisterClassEx(&window_class);
  if (atom_ == 0) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to register the window class for a message-only window";
    return false;
  }

  window_ = CreateWindow(MAKEINTATOM(atom_), window_name, 0, 0, 0, 0, 0,
                         HWND_MESSAGE, 0, instance, delegate);
  if (!window_) {
    LOG_GETLASTERROR(ERROR) << "Failed to create a message-only window";
    return false;
  }

  return true;
}

// static
LRESULT CALLBACK MessageWindow::WindowProc(HWND hwnd,
                                           UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) {
  Delegate* delegate = reinterpret_cast<Delegate*>(
      GetWindowLongPtr(hwnd, GWLP_USERDATA));

  switch (message) {
    // Set up the delegate before handling WM_CREATE.
    case WM_CREATE: {
      CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
      delegate = reinterpret_cast<Delegate*>(cs->lpCreateParams);

      // Store pointer to the delegate to the window's user data.
      SetLastError(ERROR_SUCCESS);
      LONG_PTR result = SetWindowLongPtr(
          hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(delegate));
      CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
      break;
    }

    // Clear the pointer to stop calling the delegate once WM_DESTROY is
    // received.
    case WM_DESTROY: {
      SetLastError(ERROR_SUCCESS);
      LONG_PTR result = SetWindowLongPtr(hwnd, GWLP_USERDATA, NULL);
      CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
      break;
    }
  }

  // Handle the message.
  if (delegate) {
    LRESULT message_result;
    if (delegate->HandleMessage(hwnd, message, wparam, lparam, &message_result))
      return message_result;
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

}  // namespace win
}  // namespace base
