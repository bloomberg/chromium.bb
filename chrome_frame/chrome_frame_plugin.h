// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_PLUGIN_H_
#define CHROME_FRAME_CHROME_FRAME_PLUGIN_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/win/win_util.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome_frame/simple_resource_loader.h"
#include "chrome_frame/navigation_constraints.h"
#include "chrome_frame/utils.h"
#include "grit/chromium_strings.h"

#define IDC_ABOUT_CHROME_FRAME 40018

// Helper so that this file doesn't include the messages header.
void ChromeFramePluginGetParamsCoordinates(
    const MiniContextMenuParams& params,
    int* x,
    int* y);

// A class to implement common functionality for all types of
// plugins: ActiveX and ActiveDoc
template <typename T>
class ChromeFramePlugin
    : public ChromeFrameDelegateImpl,
      public NavigationConstraintsImpl  {
 public:
  ChromeFramePlugin() : ignore_setfocus_(false){
  }
  ~ChromeFramePlugin() {
    Uninitialize();
  }

BEGIN_MSG_MAP(T)
  MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
  MESSAGE_HANDLER(WM_SIZE, OnSize)
  MESSAGE_HANDLER(WM_PARENTNOTIFY, OnParentNotify)
END_MSG_MAP()

  bool Initialize() {
    DVLOG(1) << __FUNCTION__;
    DCHECK(!automation_client_.get());
    automation_client_ = CreateAutomationClient();
    if (!automation_client_.get()) {
      NOTREACHED() << "new ChromeFrameAutomationClient";
      return false;
    }

    return true;
  }

  void Uninitialize() {
    DVLOG(1) << __FUNCTION__;
    if (IsValid()) {
      automation_client_->Uninitialize();
      automation_client_ = NULL;
    }
  }

  bool InitializeAutomation(const std::wstring& profile_name,
                            bool incognito, bool is_widget_mode,
                            const GURL& url, const GURL& referrer,
                            bool route_all_top_level_navigations) {
    DCHECK(IsValid());
    DCHECK(launch_params_ == NULL);
    // We don't want to do incognito when privileged, since we're
    // running in browser chrome or some other privileged context.
    bool incognito_mode = !is_privileged() && incognito;
    base::FilePath profile_path;
    GetProfilePath(profile_name, &profile_path);
    // The profile name could change based on the browser version. For e.g. for
    // IE6/7 the profile is created in a different folder whose last component
    // is Google Chrome Frame.
    base::FilePath actual_profile_name = profile_path.BaseName();
    launch_params_ = new ChromeFrameLaunchParams(url, referrer, profile_path,
        actual_profile_name.value(), SimpleResourceLoader::GetLanguage(),
        incognito_mode, is_widget_mode, route_all_top_level_navigations,
        automation_client_->send_shutdown_delay_switch());
    return automation_client_->Initialize(this, launch_params_);
  }

  // ChromeFrameDelegate implementation
  virtual WindowType GetWindow() const {
    return (static_cast<const T*>(this))->m_hWnd;
  }

  virtual void GetBounds(RECT* bounds) {
    if (bounds) {
      if (::IsWindow(GetWindow())) {
        (static_cast<T*>(this))->GetClientRect(bounds);
      }
    }
  }
  virtual std::string GetDocumentUrl() {
    return document_url_;
  }
  virtual void OnAutomationServerReady() {
  }

  virtual bool IsValid() const {
    return automation_client_.get() != NULL;
  }

  virtual void OnHostMoved() {
    if (IsValid())
      automation_client_->OnChromeFrameHostMoved();
  }

 protected:
  virtual void OnNavigationFailed(int error_code, const GURL& gurl) {
    OnLoadFailed(error_code, gurl.spec());
  }

  virtual void OnHandleContextMenu(const ContextMenuModel& menu_model,
                                   int align_flags,
                                   const MiniContextMenuParams& params) {
    if (!automation_client_.get()) {
      NOTREACHED();
      return;
    }

    HMENU menu = BuildContextMenu(menu_model);
    if (!menu)
      return;

    T* self = static_cast<T*>(this);
    if (self->PreProcessContextMenu(menu)) {
      // In order for the context menu to handle keyboard input, give the
      // ActiveX window focus.
      ignore_setfocus_ = true;
      SetFocus(GetWindow());
      ignore_setfocus_ = false;
      UINT flags = align_flags | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_RECURSE;
      int x, y;
      ChromeFramePluginGetParamsCoordinates(params, &x, &y);
      UINT selected = TrackPopupMenuEx(menu, flags, x, y, GetWindow(), NULL);
      // Menu is over now give focus back to chrome
      GiveFocusToChrome(false);
      if (IsValid() && selected != 0 &&
          !self->HandleContextMenuCommand(selected, params)) {
        automation_client_->SendContextMenuCommandToChromeFrame(selected);
      }
    }

    DestroyMenu(menu);
  }

  LRESULT OnSetFocus(UINT message, WPARAM wparam, LPARAM lparam,
                     BOOL& handled) {  // NO_LINT
    if (!ignore_setfocus_ && IsValid()) {
      // Pass false to |restore_focus_view|, because we do not want Chrome
      // to focus the first focusable element in the current view, only the
      // view itself.
      GiveFocusToChrome(false);
    }
    return 0;
  }

  LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam,
                 BOOL& handled) {  // NO_LINT
    handled = FALSE;
    // When we get resized, we need to resize the external tab window too.
    if (IsValid())
      automation_client_->Resize(LOWORD(lparam), HIWORD(lparam),
                                 SWP_NOACTIVATE | SWP_NOZORDER);
    return 0;
  }

  LRESULT OnParentNotify(UINT message, WPARAM wparam, LPARAM lparam,
                         BOOL& handled) {  // NO_LINT
    switch (LOWORD(wparam)) {
      case WM_LBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_XBUTTONDOWN: {
        // If we got activated via mouse click on the external tab,
        // we need to update the state of this thread and tell the
        // browser that we now have the focus.
        HWND focus = ::GetFocus();
        HWND plugin_window = GetWindow();

        // The Chrome-Frame instance may have launched a popup which currently
        // has focus.  Because experimental extension popups are top-level
        // windows, we have to check that the focus has shifted to a window
        // that does not share the same GA_ROOTOWNER as the plugin.
        if (focus != plugin_window &&
            ::GetAncestor(plugin_window, GA_ROOTOWNER) !=
                ::GetAncestor(focus, GA_ROOTOWNER)) {
          ignore_setfocus_ = true;
          SetFocus(plugin_window);
          ignore_setfocus_ = false;
        }
        break;
      }
    }

    return 0;
  }

  // Return true if context menu should be displayed. The menu could be
  // modified as well (enable/disable commands, add/remove items).
  // Override in most-derived class if needed.
  bool PreProcessContextMenu(HMENU menu) {
    // Add an "About" item.
    AppendMenu(menu, MF_STRING, IDC_ABOUT_CHROME_FRAME,
               SimpleResourceLoader::Get(IDS_CHROME_FRAME_MENU_ABOUT).c_str());
    return true;
  }

  // Return true if menu command is processed, otherwise the command will be
  // passed to Chrome for execution. Override in most-derived class if needed.
  bool HandleContextMenuCommand(UINT cmd,
                                const MiniContextMenuParams& params) {
    return false;
  }

  // Allow overriding the type of automation client used, for unit tests.
  virtual ChromeFrameAutomationClient* CreateAutomationClient() {
    return new ChromeFrameAutomationClient;
  }

  void GiveFocusToChrome(bool restore_focus_to_view) {
    if (IsValid()) {
      TabProxy* tab = automation_client_->tab();
      HWND chrome_window = automation_client_->tab_window();
      if (tab && ::IsWindow(chrome_window)) {
        DVLOG(1) << "Setting initial focus";
        tab->SetInitialFocus(base::win::IsShiftPressed(), restore_focus_to_view);
      }
    }
  }

  virtual void GetProfilePath(const std::wstring& profile_name,
                              base::FilePath* profile_path) {
    return GetChromeFrameProfilePath(profile_name, profile_path);
  }

 protected:
  // Our gateway to chrome land
  scoped_refptr<ChromeFrameAutomationClient> automation_client_;

  // How we launched Chrome.
  scoped_refptr<ChromeFrameLaunchParams> launch_params_;

  // Url of the containing document.
  std::string document_url_;

  // We set this flag when we're taking the focus ourselves
  // and notifying the host browser that we're doing so.
  // When the flag is not set, we transfer the focus to chrome.
  bool ignore_setfocus_;
};

#endif  // CHROME_FRAME_CHROME_FRAME_PLUGIN_H_
