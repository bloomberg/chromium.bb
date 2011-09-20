// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_H_
#define CONTENT_SHELL_SHELL_H_

#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class TabContents;

namespace base {
class StringPiece;
}

namespace content {

class ShellBrowserContext;

// This represents one window of the Content Shell, i.e. all the UI including
// buttons and url bar, as well as the web content area.
class Shell {
 public:
  ~Shell();

  void LoadURL(const GURL& url);
  void GoBackOrForward(int offset);
  void Reload();
  void Stop();
  void UpdateNavigationControls();

  // Do one time initialization at application startup.
  static void PlatformInitialize();

  // This is called indirectly by the modules that need access resources.
  static base::StringPiece PlatformResourceProvider(int key);

  static Shell* CreateNewWindow(ShellBrowserContext* browser_context);

 private:
  enum UIControl {
    BACK_BUTTON,
    FORWARD_BUTTON,
    STOP_BUTTON
  };

  Shell();

  // All the methods that begin with Platform need to be implemented by the
  // platform specific Shell implementation.
  // Called from the destructor to let each platform do any necessary cleanup.
  void PlatformCleanUp();
  // Creates the main window GUI.
  void PlatformCreateWindow();
  // Resizes the main window to the given dimensions.
  void PlatformSizeTo(int width, int height);
  // Resize the content area and GUI.
  void PlatformResizeSubViews();
  // Enable/disable a button.
  void PlatformEnableUIControl(UIControl control, bool is_enabled);

  gfx::NativeView GetContentView();

#if defined(OS_WIN)
  static ATOM RegisterWindowClass();
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  static LRESULT CALLBACK EditWndProc(HWND, UINT, WPARAM, LPARAM);
#endif

  scoped_ptr<TabContents> tab_contents_;

  gfx::NativeWindow main_window_;
  gfx::NativeEditView edit_window_;

#if defined(OS_WIN)
  WNDPROC default_edit_wnd_proc_;
  static HINSTANCE instance_handle_;
#endif

  static int shell_count_;
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_H_
