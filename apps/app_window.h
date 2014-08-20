// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_WINDOW_H_
#define APPS_APP_WINDOW_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sessions/session_id.h"
#include "components/web_modal/popup_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_icon_image.h"
#include "ui/base/ui_base_types.h"  // WindowShowState
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

class GURL;
class SkRegion;

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class AppDelegate;
class AppWebContentsHelper;
class Extension;
class NativeAppWindow;
class PlatformAppBrowserTest;
class WindowController;

struct DraggableRegion;
}

namespace ui {
class BaseWindow;
}

namespace apps {

// Manages the web contents for app windows. The implementation for this
// class should create and maintain the WebContents for the window, and handle
// any message passing between the web contents and the extension system or
// native window.
class AppWindowContents {
 public:
  AppWindowContents() {}
  virtual ~AppWindowContents() {}

  // Called to initialize the WebContents, before the app window is created.
  virtual void Initialize(content::BrowserContext* context,
                          const GURL& url) = 0;

  // Called to load the contents, after the app window is created.
  virtual void LoadContents(int32 creator_process_id) = 0;

  // Called when the native window changes.
  virtual void NativeWindowChanged(
      extensions::NativeAppWindow* native_app_window) = 0;

  // Called when the native window closes.
  virtual void NativeWindowClosed() = 0;

  // Called in tests when the window is shown
  virtual void DispatchWindowShownForTests() const = 0;

  virtual content::WebContents* GetWebContents() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppWindowContents);
};

// AppWindow is the type of window used by platform apps. App windows
// have a WebContents but none of the chrome of normal browser windows.
class AppWindow : public content::NotificationObserver,
                  public content::WebContentsDelegate,
                  public content::WebContentsObserver,
                  public web_modal::WebContentsModalDialogManagerDelegate,
                  public extensions::IconImage::Observer {
 public:
  enum WindowType {
    WINDOW_TYPE_DEFAULT = 1 << 0,   // Default app window.
    WINDOW_TYPE_PANEL = 1 << 1,     // OS controlled panel window (Ash only).
    WINDOW_TYPE_V1_PANEL = 1 << 2,  // For apps v1 support in Ash; deprecate
                                    // with v1 apps.
  };

  enum Frame {
    FRAME_CHROME,  // Chrome-style window frame.
    FRAME_NONE,    // Frameless window.
  };

  enum FullscreenType {
    // Not fullscreen.
    FULLSCREEN_TYPE_NONE = 0,

    // Fullscreen entered by the app.window api.
    FULLSCREEN_TYPE_WINDOW_API = 1 << 0,

    // Fullscreen entered by HTML requestFullscreen().
    FULLSCREEN_TYPE_HTML_API = 1 << 1,

    // Fullscreen entered by the OS. ChromeOS uses this type of fullscreen to
    // enter immersive fullscreen when the user hits the <F4> key.
    FULLSCREEN_TYPE_OS = 1 << 2,

    // Fullscreen mode that could not be exited by the user. ChromeOS uses
    // this type of fullscreen to run an app in kiosk mode.
    FULLSCREEN_TYPE_FORCED = 1 << 3,
  };

  struct BoundsSpecification {
    // INT_MIN represents an unspecified position component.
    static const int kUnspecifiedPosition;

    BoundsSpecification();
    ~BoundsSpecification();

    // INT_MIN designates 'unspecified' for the position components, and 0
    // designates 'unspecified' for the size components. When unspecified,
    // they should be replaced with a default value.
    gfx::Rect bounds;

    gfx::Size minimum_size;
    gfx::Size maximum_size;

    // Reset the bounds fields to their 'unspecified' values. The minimum and
    // maximum size constraints remain unchanged.
    void ResetBounds();
  };

  struct CreateParams {
    CreateParams();
    ~CreateParams();

    WindowType window_type;
    Frame frame;

    bool has_frame_color;
    SkColor active_frame_color;
    SkColor inactive_frame_color;
    bool alpha_enabled;

    // The initial content/inner bounds specification (excluding any window
    // decorations).
    BoundsSpecification content_spec;

    // The initial window/outer bounds specification (including window
    // decorations).
    BoundsSpecification window_spec;

    std::string window_key;

    // The process ID of the process that requested the create.
    int32 creator_process_id;

    // Initial state of the window.
    ui::WindowShowState state;

    // If true, don't show the window after creation.
    bool hidden;

    // If true, the window will be resizable by the user. Defaults to true.
    bool resizable;

    // If true, the window will be focused on creation. Defaults to true.
    bool focused;

    // If true, the window will stay on top of other windows that are not
    // configured to be always on top. Defaults to false.
    bool always_on_top;

    // The API enables developers to specify content or window bounds. This
    // function combines them into a single, constrained window size.
    gfx::Rect GetInitialWindowBounds(const gfx::Insets& frame_insets) const;

    // The API enables developers to specify content or window size constraints.
    // These functions combine them so that we can work with one set of
    // constraints.
    gfx::Size GetContentMinimumSize(const gfx::Insets& frame_insets) const;
    gfx::Size GetContentMaximumSize(const gfx::Insets& frame_insets) const;
    gfx::Size GetWindowMinimumSize(const gfx::Insets& frame_insets) const;
    gfx::Size GetWindowMaximumSize(const gfx::Insets& frame_insets) const;
  };

  // Convert draggable regions in raw format to SkRegion format. Caller is
  // responsible for deleting the returned SkRegion instance.
  static SkRegion* RawDraggableRegionsToSkRegion(
      const std::vector<extensions::DraggableRegion>& regions);

  // The constructor and Init methods are public for constructing a AppWindow
  // with a non-standard render interface (e.g. v1 apps using Ash Panels).
  // Normally AppWindow::Create should be used.
  // Takes ownership of |app_delegate| and |delegate|.
  AppWindow(content::BrowserContext* context,
            extensions::AppDelegate* app_delegate,
            const extensions::Extension* extension);

  // Initializes the render interface, web contents, and native window.
  // |app_window_contents| will become owned by AppWindow.
  void Init(const GURL& url,
            AppWindowContents* app_window_contents,
            const CreateParams& params);

  const std::string& window_key() const { return window_key_; }
  const SessionID& session_id() const { return session_id_; }
  const std::string& extension_id() const { return extension_id_; }
  content::WebContents* web_contents() const;
  WindowType window_type() const { return window_type_; }
  bool window_type_is_panel() const {
    return (window_type_ == WINDOW_TYPE_PANEL ||
            window_type_ == WINDOW_TYPE_V1_PANEL);
  }
  content::BrowserContext* browser_context() const { return browser_context_; }
  const gfx::Image& app_icon() const { return app_icon_; }
  const GURL& app_icon_url() const { return app_icon_url_; }
  const gfx::Image& badge_icon() const { return badge_icon_; }
  const GURL& badge_icon_url() const { return badge_icon_url_; }
  bool is_hidden() const { return is_hidden_; }

  const extensions::Extension* GetExtension() const;
  extensions::NativeAppWindow* GetBaseWindow();
  gfx::NativeWindow GetNativeWindow();

  // Returns the bounds that should be reported to the renderer.
  gfx::Rect GetClientBounds() const;

  // NativeAppWindows should call this to determine what the window's title
  // is on startup and from within UpdateWindowTitle().
  base::string16 GetTitle() const;

  // Call to notify ShellRegistry and delete the window. Subclasses should
  // invoke this method instead of using "delete this".
  void OnNativeClose();

  // Should be called by native implementations when the window size, position,
  // or minimized/maximized state has changed.
  void OnNativeWindowChanged();

  // Should be called by native implementations when the window is activated.
  void OnNativeWindowActivated();

  // Specifies a url for the launcher icon.
  void SetAppIconUrl(const GURL& icon_url);

  // Specifies a url for the window badge.
  void SetBadgeIconUrl(const GURL& icon_url);

  // Clear the current badge.
  void ClearBadge();

  // Set the window shape. Passing a NULL |region| sets the default shape.
  void UpdateShape(scoped_ptr<SkRegion> region);

  // Called from the render interface to modify the draggable regions.
  void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions);

  // Updates the app image to |image|. Called internally from the image loader
  // callback. Also called externally for v1 apps using Ash Panels.
  void UpdateAppIcon(const gfx::Image& image);

  // Enable or disable fullscreen mode. |type| specifies which type of
  // fullscreen mode to change (note that disabling one type of fullscreen may
  // not exit fullscreen mode because a window may have a different type of
  // fullscreen enabled). If |type| is not FORCED, checks that the extension has
  // the required permission.
  void SetFullscreen(FullscreenType type, bool enable);

  // Returns true if the app window is in a fullscreen state.
  bool IsFullscreen() const;

  // Returns true if the app window is in a forced fullscreen state (one that
  // cannot be exited by the user).
  bool IsForcedFullscreen() const;

  // Returns true if the app window is in a fullscreen state entered from an
  // HTML API request.
  bool IsHtmlApiFullscreen() const;

  // Transitions window into fullscreen, maximized, minimized or restores based
  // on chrome.app.window API.
  void Fullscreen();
  void Maximize();
  void Minimize();
  void Restore();

  // Transitions to OS fullscreen. See FULLSCREEN_TYPE_OS for more details.
  void OSFullscreen();

  // Transitions to forced fullscreen. See FULLSCREEN_TYPE_FORCED for more
  // details.
  void ForcedFullscreen();

  // Set the minimum and maximum size of the content bounds.
  void SetContentSizeConstraints(const gfx::Size& min_size,
                                 const gfx::Size& max_size);

  enum ShowType { SHOW_ACTIVE, SHOW_INACTIVE };

  // Shows the window if its contents have been painted; otherwise flags the
  // window to be shown as soon as its contents are painted for the first time.
  void Show(ShowType show_type);

  // Hides the window. If the window was previously flagged to be shown on
  // first paint, it will be unflagged.
  void Hide();

  AppWindowContents* app_window_contents_for_test() {
    return app_window_contents_.get();
  }

  int fullscreen_types_for_test() {
    return fullscreen_types_;
  }

  // Set whether the window should stay above other windows which are not
  // configured to be always-on-top.
  void SetAlwaysOnTop(bool always_on_top);

  // Whether the always-on-top property has been set by the chrome.app.window
  // API. Note that the actual value of this property in the native app window
  // may be false if the bit is silently switched off for security reasons.
  bool IsAlwaysOnTop() const;

  // Retrieve the current state of the app window as a dictionary, to pass to
  // the renderer.
  void GetSerializedState(base::DictionaryValue* properties) const;

  // Called by the window API when events can be sent to the window for this
  // app.
  void WindowEventsReady();

  // Whether the app window wants to be alpha enabled.
  bool requested_alpha_enabled() const { return requested_alpha_enabled_; }

 protected:
  virtual ~AppWindow();

 private:
  // PlatformAppBrowserTest needs access to web_contents()
  friend class extensions::PlatformAppBrowserTest;

  // content::WebContentsDelegate implementation.
  virtual void CloseContents(content::WebContents* contents) OVERRIDE;
  virtual bool ShouldSuppressDialogs() OVERRIDE;
  virtual content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<content::ColorSuggestion>& suggestions) OVERRIDE;
  virtual void RunFileChooser(content::WebContents* tab,
                              const content::FileChooserParams& params)
      OVERRIDE;
  virtual bool IsPopupOrPanel(const content::WebContents* source)
      const OVERRIDE;
  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& pos) OVERRIDE;
  virtual void NavigationStateChanged(
      const content::WebContents* source,
      content::InvalidateTypes changed_flags) OVERRIDE;
  virtual void ToggleFullscreenModeForTab(content::WebContents* source,
                                          bool enter_fullscreen) OVERRIDE;
  virtual bool IsFullscreenForTabOrPending(const content::WebContents* source)
      const OVERRIDE;
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
  virtual bool PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(content::WebContents* source,
                                   const content::NativeWebKeyboardEvent& event)
      OVERRIDE;
  virtual void RequestToLockMouse(content::WebContents* web_contents,
                                  bool user_gesture,
                                  bool last_unlocked_by_target) OVERRIDE;
  virtual bool PreHandleGestureEvent(content::WebContents* source,
                                     const blink::WebGestureEvent& event)
      OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void DidFirstVisuallyNonEmptyPaint() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // web_modal::WebContentsModalDialogManagerDelegate implementation.
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) OVERRIDE;
  virtual bool IsWebContentsVisible(content::WebContents* web_contents)
      OVERRIDE;

  // Saves the window geometry/position/screen bounds.
  void SaveWindowPosition();

  // Helper method to adjust the cached bounds so that we can make sure it can
  // be visible on the screen. See http://crbug.com/145752.
  void AdjustBoundsToBeVisibleOnScreen(const gfx::Rect& cached_bounds,
                                       const gfx::Rect& cached_screen_bounds,
                                       const gfx::Rect& current_screen_bounds,
                                       const gfx::Size& minimum_size,
                                       gfx::Rect* bounds) const;

  // Loads the appropriate default or cached window bounds. Returns a new
  // CreateParams that should be used to create the window.
  CreateParams LoadDefaults(CreateParams params) const;

  // Load the app's image, firing a load state change when loaded.
  void UpdateExtensionAppIcon();

  // Set the fullscreen state in the native app window.
  void SetNativeWindowFullscreen();

  // Returns true if there is any overlap between the window and the taskbar
  // (Windows only).
  bool IntersectsWithTaskbar() const;

  // Update the always-on-top bit in the native app window.
  void UpdateNativeAlwaysOnTop();

  // Sends the onWindowShown event to the app if the window has been shown. Only
  // has an effect in tests.
  void SendOnWindowShownIfShown();

  // web_modal::WebContentsModalDialogManagerDelegate implementation.
  virtual web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      OVERRIDE;

  // Updates the badge to |image|. Called internally from the image loader
  // callback.
  void UpdateBadgeIcon(const gfx::Image& image);

  // Callback from web_contents()->DownloadFavicon.
  void DidDownloadFavicon(int id,
                          int http_status_code,
                          const GURL& image_url,
                          const std::vector<SkBitmap>& bitmaps,
                          const std::vector<gfx::Size>& original_bitmap_sizes);

  // extensions::IconImage::Observer implementation.
  virtual void OnExtensionIconImageChanged(extensions::IconImage* image)
      OVERRIDE;

  // The browser context with which this window is associated. AppWindow does
  // not own this object.
  content::BrowserContext* browser_context_;

  const std::string extension_id_;

  // Identifier that is used when saving and restoring geometry for this
  // window.
  std::string window_key_;

  const SessionID session_id_;
  WindowType window_type_;
  content::NotificationRegistrar registrar_;

  // Icon shown in the task bar.
  gfx::Image app_icon_;

  // Icon URL to be used for setting the app icon. If not empty, app_icon_ will
  // be fetched and set using this URL.
  GURL app_icon_url_;

  // An object to load the app's icon as an extension resource.
  scoped_ptr<extensions::IconImage> app_icon_image_;

  // Badge for icon shown in the task bar.
  gfx::Image badge_icon_;

  // URL to be used for setting the badge on the app icon.
  GURL badge_icon_url_;

  // An object to load the badge as an extension resource.
  scoped_ptr<extensions::IconImage> badge_icon_image_;

  scoped_ptr<extensions::NativeAppWindow> native_app_window_;
  scoped_ptr<AppWindowContents> app_window_contents_;
  scoped_ptr<extensions::AppDelegate> app_delegate_;
  scoped_ptr<extensions::AppWebContentsHelper> helper_;

  // Manages popup windows (bubbles, tab-modals) visible overlapping the
  // app window.
  scoped_ptr<web_modal::PopupManager> popup_manager_;

  base::WeakPtrFactory<AppWindow> image_loader_ptr_factory_;

  // Bit field of FullscreenType.
  int fullscreen_types_;

  // Show has been called, so the window should be shown once the first visually
  // non-empty paint occurs.
  bool show_on_first_paint_;

  // The first visually non-empty paint has completed.
  bool first_paint_complete_;

  // Whether the window has been shown or not.
  bool has_been_shown_;

  // Whether events can be sent to the window.
  bool can_send_events_;

  // Whether the window is hidden or not. Hidden in this context means actively
  // by the chrome.app.window API, not in an operating system context. For
  // example windows which are minimized are not hidden, and windows which are
  // part of a hidden app on OS X are not hidden. Windows which were created
  // with the |hidden| flag set to true, or which have been programmatically
  // hidden, are considered hidden.
  bool is_hidden_;

  // Whether the delayed Show() call was for an active or inactive window.
  ShowType delayed_show_type_;

  // Cache the desired value of the always-on-top property. When windows enter
  // fullscreen or overlap the Windows taskbar, this property will be
  // automatically and silently switched off for security reasons. It is
  // reinstated when the window exits fullscreen and moves away from the
  // taskbar.
  bool cached_always_on_top_;

  // Whether |alpha_enabled| was set in the CreateParams.
  bool requested_alpha_enabled_;

  DISALLOW_COPY_AND_ASSIGN(AppWindow);
};

}  // namespace apps

#endif  // APPS_APP_WINDOW_H_
