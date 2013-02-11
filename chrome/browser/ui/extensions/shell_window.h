// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
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
class NativeAppWindow;

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
                    public extensions::ExtensionKeybindingRegistry::Delegate {
 public:
  enum WindowType {
    WINDOW_TYPE_DEFAULT,  // Default shell window
    WINDOW_TYPE_PANEL,  // OS controlled panel window (Ash only)
  };

  enum Frame {
    FRAME_CHROME,  // Chrome-style window frame.
    FRAME_NONE,  // Frameless window.
  };

  struct CreateParams {
    CreateParams();
    ~CreateParams();

    WindowType window_type;
    Frame frame;
    bool transparent_background;  // Only supported on ash.

    // Specify the initial content bounds of the window (excluding any window
    // decorations). INT_MIN designates 'unspecified' for the position
    // components, and 0 for the size components. When unspecified, they should
    // be replaced with a default value.
    gfx::Rect bounds;

    gfx::Size minimum_size;
    gfx::Size maximum_size;

    std::string window_key;

    // The process ID of the process that requested the create.
    int32 creator_process_id;

    // If true, don't show the window after creation.
    bool hidden;
  };

  static ShellWindow* Create(Profile* profile,
                             const extensions::Extension* extension,
                             const GURL& url,
                             const CreateParams& params);

  // Convert draggable regions in raw format to SkRegion format. Caller is
  // responsible for deleting the returned SkRegion instance.
  static SkRegion* RawDraggableRegionsToSkRegion(
      const std::vector<extensions::DraggableRegion>& regions);

  const std::string& window_key() const { return window_key_; }
  const SessionID& session_id() const { return session_id_; }
  const extensions::Extension* extension() const { return extension_; }
  content::WebContents* web_contents() const { return web_contents_.get(); }
  WindowType window_type() const { return window_type_; }
  Profile* profile() const { return profile_; }
  const gfx::Image& app_icon() const { return app_icon_; }
  const GURL& app_icon_url() { return app_icon_url_; }

  NativeAppWindow* GetBaseWindow();
  gfx::NativeWindow GetNativeWindow();

  // This will return a slightly smaller icon then the app_icon to be used in
  // application lists. It is the responsibility of the caller to delete the
  // returned image after use.
  gfx::Image* GetAppListIcon();

  // NativeAppWindows should call this to determine what the window's title
  // is on startup and from within UpdateWindowTitle().
  virtual string16 GetTitle() const;

  // Call to notify ShellRegistry and delete the window. Subclasses should
  // invoke this method instead of using "delete this".
  void OnNativeClose();

  // Should be called by native implementations when the window size, position,
  // or minimized/maximized state has changed.
  void OnNativeWindowChanged();

  // Specifies a url for the launcher icon.
  void SetAppIconUrl(const GURL& icon_url);

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
      const content::MediaStreamRequest& request,
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
  virtual void RequestToLockMouse(content::WebContents* web_contents,
                                  bool user_gesture,
                                  bool last_unlocked_by_target) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate implementation.
  virtual extensions::WindowController* GetExtensionWindowController() const
      OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  // Message handlers.
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  // Helper method to add a message to the renderer's DevTools console.
  void AddMessageToDevToolsConsole(content::ConsoleMessageLevel level,
                                   const std::string& message);

  // Saves the window geometry/position.
  void SaveWindowPosition();

  virtual void UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions);

  // Load the app's image, firing a load state change when loaded.
  void UpdateExtensionAppIcon();

  void OnImageLoaded(const gfx::Image& image);

  // extensions::ExtensionKeybindingRegistry::Delegate implementation.
  virtual extensions::ActiveTabPermissionGranter*
      GetActiveTabPermissionGranter() OVERRIDE;

  // Callback from web_contents()->DownloadFavicon.
  void DidDownloadFavicon(int id,
                          const GURL& image_url,
                          int requested_size,
                          const std::vector<SkBitmap>& bitmaps);

  // Updates the app image to |image|, called from image loader callback.
  void UpdateAppIcon(const gfx::Image& image);

  Profile* profile_;  // weak pointer - owned by ProfileManager.
  // weak pointer - owned by ExtensionService.
  const extensions::Extension* extension_;

  // Identifier that is used when saving and restoring geometry for this
  // window.
  std::string window_key_;

  const SessionID session_id_;
  scoped_ptr<content::WebContents> web_contents_;
  WindowType window_type_;
  content::NotificationRegistrar registrar_;
  ExtensionFunctionDispatcher extension_function_dispatcher_;

  // Icon shown in the task bar.
  gfx::Image app_icon_;

  // Icon URL to be used for setting the app icon. If not empty, app_icon_ will
  // be fetched and set using this URL.
  GURL app_icon_url_;

  scoped_ptr<NativeAppWindow> native_app_window_;

  base::WeakPtrFactory<ShellWindow> weak_ptr_factory_;

  base::WeakPtrFactory<ShellWindow> image_loader_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindow);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SHELL_WINDOW_H_
