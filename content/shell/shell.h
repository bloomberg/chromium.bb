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
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/native_widget_types.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkToolItem GtkToolItem;
#elif defined(OS_ANDROID)
#include "content/shell/android/shell_view.h"
#endif

class GURL;
class WebContents;

namespace content {

class BrowserContext;
class ShellJavaScriptDialogCreator;
class SiteInstance;

// This represents one window of the Content Shell, i.e. all the UI including
// buttons and url bar, as well as the web content area.
class Shell : public WebContentsDelegate,
              public WebContentsObserver {
 public:
  virtual ~Shell();

  void LoadURL(const GURL& url);
  void GoBackOrForward(int offset);
  void Reload();
  void Stop();
  void UpdateNavigationControls();
  void Close();

  // Do one time initialization at application startup.
  static void PlatformInitialize();

  // This is called indirectly by the modules that need access resources.
  static base::StringPiece PlatformResourceProvider(int key);

  static Shell* CreateNewWindow(content::BrowserContext* browser_context,
                                const GURL& url,
                                SiteInstance* site_instance,
                                int routing_id,
                                WebContents* base_web_contents);

  // Returns the Shell object corresponding to the given RenderViewHost.
  static Shell* FromRenderViewHost(RenderViewHost* rvh);

  // Closes all windows and returns. This runs a message loop.
  static void CloseAllWindows();

  // Closes all windows and exits.
  static void PlatformExit();

  WebContents* web_contents() const { return web_contents_.get(); }

  // layoutTestController related methods.
  void set_wait_until_done() { wait_until_done_ = true; }

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

  explicit Shell(WebContents* web_contents);

  // Helper to create a new Shell given a newly created WebContents.
  static Shell* CreateShell(WebContents* web_contents);

  // All the methods that begin with Platform need to be implemented by the
  // platform specific Shell implementation.
  // Called from the destructor to let each platform do any necessary cleanup.
  void PlatformCleanUp();
  // Creates the main window GUI.
  void PlatformCreateWindow(int width, int height);
  // Links the WebContents into the newly created window.
  void PlatformSetContents();
  // Resize the content area and GUI.
  void PlatformResizeSubViews();
  // Enable/disable a button.
  void PlatformEnableUIControl(UIControl control, bool is_enabled);
  // Updates the url in the url bar.
  void PlatformSetAddressBarURL(const GURL& url);
  // Sets whether the spinner is spinning.
  void PlatformSetIsLoading(bool loading);

#if (defined(OS_WIN) && !defined(USE_AURA)) || defined(TOOLKIT_GTK)
  // Resizes the main window to the given dimensions.
  void SizeTo(int width, int height);
#endif

  gfx::NativeView GetContentView();

  // content::WebContentsDelegate
  virtual void LoadingStateChanged(WebContents* source) OVERRIDE;
  virtual void WebContentsCreated(WebContents* source_contents,
                                  int64 source_frame_id,
                                  const GURL& target_url,
                                  WebContents* new_contents) OVERRIDE;
  virtual void DidNavigateMainFramePostCommit(
      WebContents* web_contents) OVERRIDE;
  virtual JavaScriptDialogCreator* GetJavaScriptDialogCreator() OVERRIDE;
#if defined(OS_MACOSX)
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
#endif

  // content::WebContentsObserver
  virtual void DidFinishLoad(int64 frame_id,
                             const GURL& validated_url,
                             bool is_main_frame) OVERRIDE;

#if defined(OS_WIN) && !defined(USE_AURA)
  static ATOM RegisterWindowClass();
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  static LRESULT CALLBACK EditWndProc(HWND, UINT, WPARAM, LPARAM);
#elif defined(TOOLKIT_GTK)
  CHROMEGTK_CALLBACK_0(Shell, void, OnBackButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnForwardButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnReloadButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnStopButtonClicked);
  CHROMEGTK_CALLBACK_0(Shell, void, OnURLEntryActivate);
  CHROMEGTK_CALLBACK_0(Shell, gboolean, OnWindowDestroyed);

  CHROMEG_CALLBACK_3(Shell, gboolean, OnCloseWindowKeyPressed, GtkAccelGroup*,
                     GObject*, guint, GdkModifierType);
  CHROMEG_CALLBACK_3(Shell, gboolean, OnHighlightURLView, GtkAccelGroup*,
                     GObject*, guint, GdkModifierType);
#endif

  scoped_ptr<ShellJavaScriptDialogCreator> dialog_creator_;

  scoped_ptr<WebContents> web_contents_;

  // layoutTestController related variables.
  bool wait_until_done_;

  gfx::NativeWindow window_;
  gfx::NativeEditView url_edit_view_;

#if defined(OS_WIN) && !defined(USE_AURA)
  WNDPROC default_edit_wnd_proc_;
  static HINSTANCE instance_handle_;
#elif defined(TOOLKIT_GTK)
  GtkWidget* vbox_;

  GtkToolItem* back_button_;
  GtkToolItem* forward_button_;
  GtkToolItem* reload_button_;
  GtkToolItem* stop_button_;

  GtkWidget* spinner_;
  GtkToolItem* spinner_item_;

  int content_width_;
  int content_height_;
#elif defined(OS_ANDROID)
  scoped_ptr<ShellView> shell_view_;
#endif

  // A container of all the open windows. We use a vector so we can keep track
  // of ordering.
  static std::vector<Shell*> windows_;

  // True if the destructur of Shell should post a quit closure on the current
  // message loop if the destructed Shell object was the last one.
  static bool quit_message_loop_;
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_H_
