// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/base_window.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

class GURL;
class NativePanel;
class PanelCollection;
class PanelHost;
class PanelManager;
class Profile;
class StackedPanelCollection;

namespace content {
class WebContents;
struct NativeWebKeyboardEvent;
}

namespace extensions {
class Extension;
class WindowController;
}

// A platform independent implementation of ui::BaseWindow for Panels.
// This class gets the first crack at all the ui::BaseWindow calls for Panels
// and does one or more of the following:
// - Do nothing.  The function is not relevant to Panels.
// - Do Panel specific platform independent processing and then invoke the
//   function on the platform specific member. For example, restrict panel
//   size to certain limits.
// - Invoke an appropriate PanelManager function to do stuff that might affect
//   other Panels. For example deleting a panel would rearrange other panels.
class Panel : public ui::BaseWindow,
              public CommandUpdaterDelegate,
              public content::NotificationObserver {
 public:
  enum ExpansionState {
    // The panel is fully expanded with both title-bar and the client-area.
    EXPANDED,
    // The panel is shown with the title-bar only.
    TITLE_ONLY,
    // The panel is shown with 3-pixel line.
    MINIMIZED
  };

  // Controls how the attention should be drawn.
  enum AttentionMode {
    // Uses the panel attention. The panel's titlebar would be painted
    // differently to attract the user's attention. This is the default mode.
    USE_PANEL_ATTENTION = 0x01,
    // Uses the system attention. On Windows or Linux (depending on Window
    // Manager), the app icon on taskbar will be flashed. On MacOS, the dock
    // icon will jump once.
    USE_SYSTEM_ATTENTION = 0x02
  };

  virtual ~Panel();

  // Returns the PanelManager associated with this panel.
  PanelManager* manager() const;

  const std::string& app_name() const { return app_name_; }
  const gfx::Image& app_icon() const { return app_icon_; }
  const SessionID& session_id() const { return session_id_; }
  extensions::WindowController* extension_window_controller() const {
    return extension_window_controller_.get();
  }
  const std::string extension_id() const;

  CommandUpdater* command_updater();
  Profile* profile() const;

  const extensions::Extension* GetExtension() const;

  // Returns web contents of the panel, if any. There may be none if web
  // contents have not been added to the panel yet.
  content::WebContents* GetWebContents() const;

  void SetExpansionState(ExpansionState new_expansion_state);

  bool IsDrawingAttention() const;

  // This function will only get called by PanelManager when full screen mode
  // changes i.e it gets called when an app goes into full screen mode or when
  // an app exits full screen mode. Panel should respond by making sure
  // a) it does not go on top when some app enters full screen mode.
  // b) it remains on top when an app exits full screen mode.
  void FullScreenModeChanged(bool is_full_screen);

  int TitleOnlyHeight() const;

  // Returns true if the panel can show minimize or restore button in its
  // titlebar, depending on its state.
  bool CanShowMinimizeButton() const;
  bool CanShowRestoreButton() const;

  // ui::BaseWindow overrides.
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual ui::WindowShowState GetRestoredState() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool on_top) OVERRIDE;

  // Overridden from CommandUpdaterDelegate:
  virtual void ExecuteCommandWithDisposition(
      int id,
      WindowOpenDisposition disposition) OVERRIDE;

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Construct a native panel implementation.
  static NativePanel* CreateNativePanel(Panel* panel,
                                        const gfx::Rect& bounds,
                                        bool always_on_top);

  NativePanel* native_panel() const { return native_panel_; }

  // Invoked when the native panel has detected a mouse click on the
  // panel's titlebar, minimize or restore buttons. Behavior of the
  // click may be modified as indicated by |modifier|.
  void OnTitlebarClicked(panel::ClickModifier modifier);
  void OnMinimizeButtonClicked(panel::ClickModifier modifier);
  void OnRestoreButtonClicked(panel::ClickModifier modifier);

  // Used on platforms where the panel cannot determine its window size
  // until the window has been created. (e.g. GTK)
  void OnWindowSizeAvailable();

  // Asynchronous completion of panel close request.
  void OnNativePanelClosed();

  // May be NULL if:
  // * panel is newly created and has not been positioned yet.
  // * panel is being closed asynchronously.
  // Please use it with caution.
  PanelCollection* collection() const { return collection_; }

  // Sets the current panel collection that contains this panel.
  void set_collection(PanelCollection* new_collection) {
    collection_ = new_collection;
  }

  StackedPanelCollection* stack() const;

  ExpansionState expansion_state() const { return expansion_state_; }
  const gfx::Size& min_size() const { return min_size_; }
  const gfx::Size& max_size() const { return max_size_; }
  bool auto_resizable() const { return auto_resizable_; }

  bool in_preview_mode() const { return in_preview_mode_; }

  panel::Resizability CanResizeByMouse() const;

  AttentionMode attention_mode() const { return attention_mode_; }
  void set_attention_mode(AttentionMode attention_mode) {
    attention_mode_ = attention_mode;
  }

  // The full size is the size of the panel when it is detached or expanded
  // in the docked collection and squeezing mode is not on.
  gfx::Size full_size() const { return full_size_; }
  void set_full_size(const gfx::Size& size) { full_size_ = size; }

  // Panel must be initialized to be "fully created" and ready for use.
  // Only called by PanelManager.
  bool initialized() const { return initialized_; }
  void Initialize(const GURL& url, const gfx::Rect& bounds, bool always_on_top);

  // This is different from BaseWindow::SetBounds():
  // * SetPanelBounds() is only called by PanelManager to manage its position.
  // * SetBounds() is called by the API to try to change the bounds, which may
  //   only change the size for Panel.
  void SetPanelBounds(const gfx::Rect& bounds);

  // Updates the panel bounds instantly without any animation.
  void SetPanelBoundsInstantly(const gfx::Rect& bounds);

  // Ensures that the panel's size does not exceed the work area by updating
  // maximum and full size of the panel. This is called each time when display
  // settings are changed. Note that bounds are not updated here and the call
  // of setting bounds or refreshing layout should be called after this.
  void LimitSizeToWorkArea(const gfx::Rect& work_area);

  // Sets whether the panel will auto resize according to its content.
  void SetAutoResizable(bool resizable);

  // Configures the web contents for auto resize, including configurations
  // on the renderer and detecting renderer changes.
  void EnableWebContentsAutoResize(content::WebContents* web_contents);

  // Invoked when the preferred window size of the given panel might need to
  // get changed due to the contents being auto-resized.
  void OnContentsAutoResized(const gfx::Size& new_content_size);

  // Resizes the panel and sets the origin. Invoked when the panel is resized
  // via the mouse.
  void OnWindowResizedByMouse(const gfx::Rect& new_bounds);

  // Sets minimum and maximum size for the panel.
  void SetSizeRange(const gfx::Size& min_size, const gfx::Size& max_size);

  // Updates the maximum size of the panel so that it's never smaller than the
  // panel's desired size. Note that even if the user resizes the panel smaller
  // later, the increased maximum size will still be in effect. Since it's not
  // possible currently to switch the panel back to autosizing from
  // user-resizable, it should not be a problem.
  void IncreaseMaxSize(const gfx::Size& desired_panel_size);

  // Handles keyboard events coming back from the renderer.
  void HandleKeyboardEvent(const content::NativeWebKeyboardEvent& event);

  // Sets whether the panel is shown in preview mode. When the panel is
  // being dragged, it is in preview mode.
  void SetPreviewMode(bool in_preview_mode);

  // Sets whether the minimize or restore button, if any, are visible.
  void UpdateMinimizeRestoreButtonVisibility();

  // Changes the preferred size to acceptable based on min_size() and max_size()
  gfx::Size ClampSize(const gfx::Size& size) const;

  // Called when the panel's active state changes.
  // |active| is true if panel became active.
  void OnActiveStateChanged(bool active);

  // Called when the panel starts/ends the user resizing.
  void OnPanelStartUserResizing();
  void OnPanelEndUserResizing();

  // Gives beforeunload handlers the chance to cancel the close.
  bool ShouldCloseWindow();

  // Invoked when the window containing us is closing. Performs the necessary
  // cleanup.
  void OnWindowClosing();

  // Executes a command if it's enabled.
  // Returns true if the command is executed.
  bool ExecuteCommandIfEnabled(int id);

  // Gets the title of the window from the web contents.
  base::string16 GetWindowTitle() const;

  // Gets the Favicon of the web contents.
  gfx::Image GetCurrentPageIcon() const;

  // Updates the title bar to display the current title and icon.
  void UpdateTitleBar();

  // Updates UI to reflect change in loading state.
  void LoadingStateChanged(bool is_loading);

  // Updates UI to reflect that the web cotents receives the focus.
  void WebContentsFocused(content::WebContents* contents);

  // Moves the panel by delta instantly.
  void MoveByInstantly(const gfx::Vector2d& delta_origin);

  // Applies |corner_style| to the panel window.
  void SetWindowCornerStyle(panel::CornerStyle corner_style);

  // Performs the system minimize for the panel, i.e. becoming iconic.
  void MinimizeBySystem();

  bool IsMinimizedBySystem() const;

  // Returns true if the panel is shown in the active desktop. The user could
  // create or use multiple desktops or workspaces.
  bool IsShownOnActiveDesktop() const;

  // Turns on/off the shadow effect around the window shape.
  void ShowShadow(bool show);

 protected:
  // Panel can only be created using PanelManager::CreatePanel() or subclass.
  // |app_name| is the default title for Panels when the page content does not
  // provide a title. For extensions, this is usually the application name
  // generated from the extension id.
  Panel(Profile* profile, const std::string& app_name,
        const gfx::Size& min_size, const gfx::Size& max_size);

 private:
  friend class PanelManager;
  friend class PanelBrowserTest;

  enum MaxSizePolicy {
    // Default maximum size is proportional to the work area.
    DEFAULT_MAX_SIZE,
    // Custom maximum size is used when the panel is resized by the user.
    CUSTOM_MAX_SIZE
  };

  void OnImageLoaded(const gfx::Image& image);

  // Initialize state for all supported commands.
  void InitCommandState();

  // Configures the renderer for auto resize (if auto resize is enabled).
  void ConfigureAutoResize(content::WebContents* web_contents);

  // Load the app's image, firing a load state change when loaded.
  void UpdateAppIcon();

  // Prepares a title string for display (removes embedded newlines, etc).
  static void FormatTitleForDisplay(base::string16* title);

  // The application name that is also the name of the window when the
  // page content does not provide a title.
  // This name should be set when the panel is created.
  const std::string app_name_;

  Profile* profile_;

  // Current collection of panels to which this panel belongs. This determines
  // the panel's screen layout.
  PanelCollection* collection_;  // Owned by PanelManager.

  bool initialized_;

  // Stores the full size of the panel so we can restore it after it's
  // been minimized or squeezed due to lack of space in the collection.
  gfx::Size full_size_;

  // This is the minimum size that the panel can shrink to.
  gfx::Size min_size_;

  // This is the size beyond which the panel is not going to grow to accomodate
  // the growing content and WebKit would add the scrollbars in such case.
  gfx::Size max_size_;

  MaxSizePolicy max_size_policy_;

  // True if this panel auto resizes based on content.
  bool auto_resizable_;

  // True if this panel is in preview mode. When in preview mode, panel bounds
  // should not be affected by layout refresh. This is currently used by drag
  // controller to add a panel to the collection without causing its bounds to
  // change.
  bool in_preview_mode_;

  // Platform specifc implementation for panels.  It'd be one of
  // PanelGtk/PanelView/PanelCocoa.
  NativePanel* native_panel_;  // Weak, owns us.

  AttentionMode attention_mode_;

  ExpansionState expansion_state_;

  // The CommandUpdater manages the window commands.
  CommandUpdater command_updater_;

  content::NotificationRegistrar registrar_;
  const SessionID session_id_;
  scoped_ptr<extensions::WindowController> extension_window_controller_;
  scoped_ptr<PanelHost> panel_host_;

  // Icon showed in the task bar.
  gfx::Image app_icon_;

  base::WeakPtrFactory<Panel> image_loader_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Panel);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_H_
