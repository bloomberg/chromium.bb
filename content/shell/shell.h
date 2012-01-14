// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_H_
#define CONTENT_SHELL_SHELL_H_

#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_LINUX)
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkToolItem GtkToolItem;
#endif

class GURL;
class SiteInstance;
class TabContents;

namespace content {

class BrowserContext;

// This represents one window of the Content Shell, i.e. all the UI including
// buttons and url bar, as well as the web content area.
class Shell : public WebContentsDelegate {
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

#if defined(OS_MACOSX)
  // Public to be called by an ObjC bridge object.
  void ActionPerformed(int control);
  void URLEntered(std::string url_string);
#endif

 private:
  enum UIControl {
    BACK_BUTTON,
    FORWARD_BUTTON,
    STOP_BUTTON
  };

  Shell();

  // Helper to create a new Shell given a newly created TabContents.
  static Shell* CreateShell(TabContents* tab_contents);

  // All the methods that begin with Platform need to be implemented by the
  // platform specific Shell implementation.
  // Called from the destructor to let each platform do any necessary cleanup.
  void PlatformCleanUp();
  // Creates the main window GUI.
  void PlatformCreateWindow(int width, int height);
  // Links the TabContents into the newly created window.
  void PlatformSetContents();
  // Resizes the main window to the given dimensions.
  void PlatformSizeTo(int width, int height);
  // Resize the content area and GUI.
  void PlatformResizeSubViews();
  // Enable/disable a button.
  void PlatformEnableUIControl(UIControl control, bool is_enabled);
  // Updates the url in the url bar.
  void PlatformSetAddressBarURL(const GURL& url);

  gfx::NativeView GetContentView();

  // content::WebContentsDelegate
  virtual void LoadingStateChanged(WebContents* source) OVERRIDE;
  virtual void WebContentsCreated(WebContents* source_contents,
                                  int64 source_frame_id,
                                  const GURL& target_url,
                                  WebContents* new_contents) OVERRIDE;
  virtual void DidNavigateMainFramePostCommit(WebContents* tab) OVERRIDE;
  virtual void UpdatePreferredSize(WebContents* source,
                                   const gfx::Size& pref_size) OVERRIDE;

#if defined(OS_WIN)
  static ATOM RegisterWindowClass();
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  static LRESULT CALLBACK EditWndProc(HWND, UINT, WPARAM, LPARAM);
#elif defined(OS_LINUX)
  CHROMEGTK_CALLBACK_0(Shell, void, OnBackButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnForwardButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnReloadButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnStopButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnURLEntryActivate);
#endif

  scoped_ptr<TabContents> tab_contents_;

  gfx::NativeWindow window_;
  gfx::NativeEditView url_edit_view_;

#if defined(OS_WIN)
  WNDPROC default_edit_wnd_proc_;
  static HINSTANCE instance_handle_;
#elif defined(OS_LINUX)
  GtkWidget* vbox_;

  GtkToolItem* back_button_;
  GtkToolItem* forward_button_;
  GtkToolItem* reload_button_;
  GtkToolItem* stop_button_;

  int content_width_;
  int content_height_;
#endif

  // A container of all the open windows. We use a vector so we can keep track
  // of ordering.
  static std::vector<Shell*> windows_;
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_H_
