// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NATIVE_WEB_KEYBOARD_EVENT_H_
#define CONTENT_COMMON_NATIVE_WEB_KEYBOARD_EVENT_H_
#pragma once

#include "base/basictypes.h"
#include "build/build_config.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent;
#endif  // __OBJC__
#elif defined(OS_POSIX)
typedef struct _GdkEventKey GdkEventKey;
#endif

// Owns a platform specific event; used to pass own and pass event through
// platform independent code.
struct NativeWebKeyboardEvent : public WebKit::WebKeyboardEvent {
  NativeWebKeyboardEvent();

#if defined(OS_WIN)
  NativeWebKeyboardEvent(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
#elif defined(OS_MACOSX)
  explicit NativeWebKeyboardEvent(NSEvent *event);
  NativeWebKeyboardEvent(wchar_t character,
                         int state,
                         double time_stamp_seconds);
#elif defined(TOOLKIT_USES_GTK)
  // TODO(suzhe): Limit these constructors to Linux native Gtk port.
  // For Linux Views port, after using RenderWidgetHostViewViews to replace
  // RenderWidgetHostViewGtk, we can use constructors for TOOLKIT_VIEWS defined
  // below.
  explicit NativeWebKeyboardEvent(const GdkEventKey* event);
  NativeWebKeyboardEvent(wchar_t character,
                         int state,
                         double time_stamp_seconds);
#endif

  NativeWebKeyboardEvent(const NativeWebKeyboardEvent& event);
  ~NativeWebKeyboardEvent();

  NativeWebKeyboardEvent& operator=(const NativeWebKeyboardEvent& event);

#if defined(OS_WIN)
  MSG os_event;
#elif defined(OS_MACOSX)
  NSEvent* os_event;
#elif defined(TOOLKIT_USES_GTK)
  GdkEventKey* os_event;
#endif

  // True if the browser should ignore this event if it's not handled by the
  // renderer. This happens for RawKeyDown events that are created while IME is
  // active and is necessary to prevent backspace from doing "history back" if
  // it is hit in ime mode.
  // Currently, it's only used by Linux and Mac ports.
  bool skip_in_browser;

#if defined(TOOLKIT_USES_GTK)
  // True if the key event matches an edit command. In order to ensure the edit
  // command always work in web page, the browser should not pre-handle this key
  // event as a reserved accelerator. See http://crbug.com/54573
  bool match_edit_command;
#endif
};

#endif  // CONTENT_COMMON_NATIVE_WEB_KEYBOARD_EVENT_H_
