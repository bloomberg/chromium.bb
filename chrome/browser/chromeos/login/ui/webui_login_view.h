// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEBUI_LOGIN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEBUI_LOGIN_VIEW_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/signin/scoped_gaia_auth_extension.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "components/web_modal/popup_manager.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

class GURL;

namespace content {
class WebUI;
}

namespace views {
class View;
class WebView;
class Widget;
}

namespace chromeos {

// View used to render a WebUI supporting Widget. This widget is used for the
// WebUI based start up and lock screens. It contains a WebView.
class WebUILoginView : public views::View,
                       public content::WebContentsDelegate,
                       public content::WebContentsObserver,
                       public content::NotificationObserver,
                       public ChromeWebModalDialogManagerDelegate,
                       public web_modal::WebContentsModalDialogHost {
 public:
  class FrameObserver {
   public:
    // Called when a frame failed to load.
    virtual void OnFrameError(const std::string& frame_unique_name) = 0;
  };

  // Internal class name.
  static const char kViewClassName[];

  WebUILoginView();
  virtual ~WebUILoginView();

  // Initializes the webui login view.
  virtual void Init();

  // Overridden from views::View:
  virtual bool AcceleratorPressed(
      const ui::Accelerator& accelerator) OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual void RequestFocus() OVERRIDE;

  // Overridden from ChromeWebModalDialogManagerDelegate:
  virtual web_modal::WebContentsModalDialogHost*
      GetWebContentsModalDialogHost() OVERRIDE;

  // Overridden from web_modal::WebContentsModalDialogHost:
  virtual gfx::NativeView GetHostView() const OVERRIDE;
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) OVERRIDE;
  virtual gfx::Size GetMaximumDialogSize() OVERRIDE;
  virtual void AddObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;

  // Gets the native window from the view widget.
  gfx::NativeWindow GetNativeWindow() const;

  // Loads given page. Should be called after Init() has been called.
  void LoadURL(const GURL& url);

  // Returns current WebUI.
  content::WebUI* GetWebUI();

  // Returns current WebContents.
  content::WebContents* GetWebContents();

  // Opens proxy settings dialog.
  void OpenProxySettings();

  // Called when WebUI is being shown after being initilized hidden.
  void OnPostponedShow();

  // Toggles status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Sets whether UI should be enabled.
  void SetUIEnabled(bool enabled);

  void set_is_hidden(bool hidden) { is_hidden_ = hidden; }

  bool webui_visible() const { return webui_visible_; }

  // Let suppress emission of this signal.
  void set_should_emit_login_prompt_visible(bool emit) {
    should_emit_login_prompt_visible_ = emit;
  }

  void AddFrameObserver(FrameObserver* frame_observer);
  void RemoveFrameObserver(FrameObserver* frame_observer);

 protected:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual void OnLocaleChanged() OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse) OVERRIDE;

  // Overridden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // WebView for rendering a webpage as a webui login.
  views::WebView* webui_login_;

 private:
  // Map type for the accelerator-to-identifier map.
  typedef std::map<ui::Accelerator, std::string> AccelMap;

  // Overridden from content::WebContentsDelegate.
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual bool TakeFocus(content::WebContents* source, bool reverse) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual bool PreHandleGestureEvent(
      content::WebContents* source,
      const blink::WebGestureEvent& event) OVERRIDE;

  // Overridden from content::WebContentsObserver.
  virtual void DidFailProvisionalLoad(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description) OVERRIDE;

  // Performs series of actions when login prompt is considered
  // to be ready and visible.
  // 1. Emits LoginPromptVisible signal if needed
  // 2. Notifies OOBE/sign classes.
  void OnLoginPromptVisible();

  // Called when focus is returned from status area.
  // |reverse| is true when focus is traversed backwards (using Shift-Tab).
  void ReturnFocus(bool reverse);

  content::NotificationRegistrar registrar_;

  // Converts keyboard events on the WebContents to accelerators.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Maps installed accelerators to OOBE webui accelerator identifiers.
  AccelMap accel_map_;

  // True when WebUI is being initialized hidden.
  bool is_hidden_;

  // True when the WebUI has finished initializing and is visible.
  bool webui_visible_;

  // Should we emit the login-prompt-visible signal when the login page is
  // displayed?
  bool should_emit_login_prompt_visible_;

  // True to forward keyboard event.
  bool forward_keyboard_event_;

  scoped_ptr<ScopedGaiaAuthExtension> auth_extension_;

  ObserverList<web_modal::ModalDialogHostObserver> observer_list_;
  ObserverList<FrameObserver> frame_observer_list_;

  // Manage popups appearing over the login window.
  // TODO(gbillock): See if we can get rid of this. Perhaps in favor of
  // in-content styled popups or something? There oughtta be a way...
  scoped_ptr<web_modal::PopupManager> popup_manager_;

  DISALLOW_COPY_AND_ASSIGN(WebUILoginView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEBUI_LOGIN_VIEW_H_
