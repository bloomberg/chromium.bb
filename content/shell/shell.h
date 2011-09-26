// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_H_
#define CONTENT_SHELL_SHELL_H_

#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class SiteInstance;
class TabContents;

namespace base {
class StringPiece;
}

namespace content {

class BrowserContext;

// This represents one window of the Content Shell, i.e. all the UI including
// buttons and url bar, as well as the web content area.
class Shell : public TabContentsDelegate {
 public:
  virtual ~Shell();

  void LoadURL(const GURL& url);
  void GoBackOrForward(int offset);
  void Reload();
  void Stop();
  void UpdateNavigationControls();

  // Do one time initialization at application startup.
  static void PlatformInitialize();

  // This is called indirectly by the modules that need access resources.
  static base::StringPiece PlatformResourceProvider(int key);

  static Shell* CreateNewWindow(content::BrowserContext* browser_context,
                                const GURL& url,
                                SiteInstance* site_instance,
                                int routing_id,
                                TabContents* base_tab_contents);

  // Closes all windows and exits.
  static void PlatformExit();

  TabContents* tab_contents() const { return tab_contents_.get(); }

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
  // Updates the url in the url bar.
  void PlatformSetAddressBarURL(const GURL& url);

  gfx::NativeView GetContentView();

  // TabContentsDelegate
  virtual void LoadingStateChanged(TabContents* source) OVERRIDE;
  virtual void DidNavigateMainFramePostCommit(TabContents* tab) OVERRIDE;
  virtual void UpdatePreferredSize(TabContents* source,
                                   const gfx::Size& pref_size) OVERRIDE;

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

  // A container of all the open windows. We use a vector so we can keep track
  // of ordering.
  static std::vector<Shell*> windows_;
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_H_
