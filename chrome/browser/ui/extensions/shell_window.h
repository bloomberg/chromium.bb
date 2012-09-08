// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/base_window.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/console_message_level.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

class GURL;
class Profile;
class TabContents;
class NativeShellWindow;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
class PlatformAppBrowserTest;
class WindowController;

struct DraggableRegion;
}

// ShellWindow is the type of window used by platform apps. Shell windows
// have a WebContents but none of the chrome of normal browser windows.
class ShellWindow : public content::NotificationObserver,
                    public content::WebContentsDelegate,
                    public content::WebContentsObserver,
                    public ExtensionFunctionDispatcher::Delegate,
                    public ImageLoadingTracker::Observer  {
 public:
  struct CreateParams {
    enum Frame {
      FRAME_CHROME, // Chrome-style window frame.
      FRAME_NONE, // Frameless window.
    };

    CreateParams();
    ~CreateParams();

    Frame frame;
    // Specify the initial bounds of the window. If empty, the window will be a
    // default size.
    gfx::Rect bounds;
    // Specify if bounds should be restored from a previous time.
    bool restore_position;
    bool restore_size;

    gfx::Size minimum_size;
    gfx::Size maximum_size;

    std::string window_key;
  };

  static ShellWindow* Create(Profile* profile,
                             const extensions::Extension* extension,
                             const GURL& url,
                             const CreateParams& params);

  const SessionID& session_id() const { return session_id_; }
  const extensions::Extension* extension() const { return extension_; }
  TabContents* tab_contents() const { return contents_.get(); }
  content::WebContents* web_contents() const { return web_contents_; }
  Profile* profile() const { return profile_; }
  const gfx::Image& app_icon() const { return app_icon_; }

  BaseWindow* GetBaseWindow();
  gfx::NativeWindow GetNativeWindow() {
    return GetBaseWindow()->GetNativeWindow();
  }

  // NativeShellWindows should call this to determine what the window's title
  // is on startup and from within UpdateWindowTitle().
  virtual string16 GetTitle() const;

  // Call to notify ShellRegistry and delete the window. Subclasses should
  // invoke this method instead of using "delete this".
  void OnNativeClose();

  // Save the window position in the prefs.
  virtual void SaveWindowPosition();

 protected:
  ShellWindow(Profile* profile,
              const extensions::Extension* extension);
  virtual ~ShellWindow();

 private:
  // PlatformAppBrowserTest needs access to web_contents()
  friend class extensions::PlatformAppBrowserTest;

  // Instantiates a platform-specific ShellWindow subclass (one implementation
  // per platform). Public users of ShellWindow should use ShellWindow::Create.
  void Init(const GURL& url, const CreateParams& params);

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // content::WebContentsDelegate implementation.
  virtual void CloseContents(content::WebContents* contents) OVERRIDE;
  virtual bool ShouldSuppressDialogs() OVERRIDE;
  virtual void WebIntentDispatch(
      content::WebContents* web_contents,
      content::WebIntentsDispatcher* intents_dispatcher) OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* tab,
      const content::FileChooserParams& params) OVERRIDE;
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual void MoveContents(
      content::WebContents* source, const gfx::Rect& pos) OVERRIDE;
  virtual void NavigationStateChanged(const content::WebContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void ToggleFullscreenModeForTab(content::WebContents* source,
                                          bool enter_fullscreen) OVERRIDE;
  virtual bool IsFullscreenForTabOrPending(
      const content::WebContents* source) const OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest* request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate implementation.
  virtual extensions::WindowController* GetExtensionWindowController() const
      OVERRIDE;

  // Message handlers.
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  // Helper method to add a message to the renderer's DevTools console.
  void AddMessageToDevToolsConsole(content::ConsoleMessageLevel level,
                                   const std::string& message);

  virtual void UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions);

  // Load the app's image, firing a load state change when loaded.
  void UpdateExtensionAppIcon();

  // ImageLoadingTracker::Observer implementation.
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE;

  Profile* profile_;  // weak pointer - owned by ProfileManager.
  // weak pointer - owned by ExtensionService.
  const extensions::Extension* extension_;

  // Identifier that is used when saving and restoring geometry for this
  // window.
  std::string window_key_;

  const SessionID session_id_;
  scoped_ptr<TabContents> contents_;
  // web_contents_ is owned by contents_.
  content::WebContents* web_contents_;
  content::NotificationRegistrar registrar_;
  ExtensionFunctionDispatcher extension_function_dispatcher_;

  // Icon showed in the task bar.
  gfx::Image app_icon_;

  // Used for loading app_icon_.
  scoped_ptr<ImageLoadingTracker> app_icon_loader_;

  scoped_ptr<NativeShellWindow> native_window_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindow);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
