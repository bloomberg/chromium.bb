// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_PLUGIN_H_
#define CHROME_FRAME_CHROME_FRAME_PLUGIN_H_

#include "base/win_util.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/utils.h"

#define IDC_ABOUT_CHROME_FRAME 40018

// A class to implement common functionality for all types of
// plugins: NPAPI. ActiveX and ActiveDoc
template <typename T>
class ChromeFramePlugin : public ChromeFrameDelegateImpl {
 public:
  ChromeFramePlugin()
      : ignore_setfocus_(false),
        is_privileged_(false) {
  }
  ~ChromeFramePlugin() {
    Uninitialize();
  }

BEGIN_MSG_MAP(ChromeFrameActivex)
  MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
  MESSAGE_HANDLER(WM_SIZE, OnSize)
  MESSAGE_HANDLER(WM_PARENTNOTIFY, OnParentNotify)
END_MSG_MAP()

  bool Initialize() {
    DCHECK(!automation_client_.get());
    automation_client_.reset(CreateAutomationClient());
    if (!automation_client_.get()) {
      NOTREACHED() << "new ChromeFrameAutomationClient";
      return false;
    }

    return true;
  }

  void Uninitialize() {
    if (automation_client_.get()) {
      automation_client_->Uninitialize();
      automation_client_.reset();
    }
  }

  bool InitializeAutomation(const std::wstring& profile_name,
                            const std::wstring& extra_chrome_arguments,
                            bool incognito) {
    // We don't want to do incognito when privileged, since we're
    // running in browser chrome or some other privileged context.
    bool incognito_mode = !is_privileged_ && incognito;
    return automation_client_->Initialize(this, kCommandExecutionTimeout, true,
                                          profile_name, extra_chrome_arguments,
                                          incognito_mode);
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
    // Issue the extension automation request if we're privileged to
    // allow this control to handle extension requests from Chrome.
    if (is_privileged_)
      automation_client_->SetEnableExtensionAutomation(functions_enabled_);
  }

  virtual bool IsValid() const {
    return automation_client_.get() != NULL;
  }

 protected:
  virtual void OnNavigationFailed(int tab_handle, int error_code,
                                  const GURL& gurl) {
    OnLoadFailed(error_code, gurl.spec());
  }

  virtual void OnHandleContextMenu(int tab_handle, HANDLE menu_handle,
                                   int x_pos, int y_pos, int align_flags) {
    if (!menu_handle || !automation_client_.get()) {
      NOTREACHED();
      return;
    }

    // TrackPopupMenuEx call will fail on IE on Vista running
    // in low integrity mode. We DO seem to be able to enumerate the menu
    // though, so just clone it and show the copy:
    HMENU copy = UtilCloneContextMenu(static_cast<HMENU>(menu_handle));
    if (!copy)
      return;

    T* pThis = static_cast<T*>(this);
    if (pThis->PreProcessContextMenu(copy)) {
      UINT flags = align_flags | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_RECURSE;
      UINT selected = TrackPopupMenuEx(copy, flags, x_pos, y_pos, GetWindow(),
                                       NULL);
      if (selected != 0 && !pThis->HandleContextMenuCommand(selected)) {
        automation_client_->SendContextMenuCommandToChromeFrame(selected);
      }
    }

    DestroyMenu(copy);
  }

  LRESULT OnSetFocus(UINT message, WPARAM wparam, LPARAM lparam,
                     BOOL& handled) {  // NO_LINT
    if (!ignore_setfocus_ && automation_client_ != NULL) {
      TabProxy* tab = automation_client_->tab();
      HWND chrome_window = automation_client_->tab_window();
      if (tab && ::IsWindow(chrome_window)) {
        DLOG(INFO) << "Setting initial focus";
        tab->SetInitialFocus(win_util::IsShiftPressed());
      }
    }

    return 0;
  }

  LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam,
                 BOOL& handled) {  // NO_LINT
    handled = FALSE;
    // When we get resized, we need to resize the external tab window too.
    if (automation_client_.get())
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
        if (focus != plugin_window && !IsChild(plugin_window, focus)) {
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
    // TODO: The string should be localized and menu should
    // be modified in ExternalTabContainer:: once we go public.
    AppendMenu(menu, MF_STRING, IDC_ABOUT_CHROME_FRAME,
        L"About Chrome Frame...");
    return true;
  }

  // Return true if menu command is processed, otherwise the command will be
  // passed to Chrome for execution. Override in most-derived class if needed.
  bool HandleContextMenuCommand(UINT cmd) {
    return false;
  }

  // Allow overriding the type of automation client used, for unit tests.
  virtual ChromeFrameAutomationClient* CreateAutomationClient() {
    return new ChromeFrameAutomationClient;
  }

 protected:
  // Our gateway to chrome land
  scoped_ptr<ChromeFrameAutomationClient> automation_client_;

  // Url of the containing document.
  std::string document_url_;

  // We set this flag when we're taking the focus ourselves
  // and notifying the host browser that we're doing so.
  // When the flag is not set, we transfer the focus to chrome.
  bool ignore_setfocus_;

  // The plugin is privileged if it is:
  // * Invoked by a window running under the system principal in FireFox.
  // * Being hosted by a custom host exposing the SID_ChromeFramePrivileged
  //   service.
  //
  // When privileged, additional interfaces are made available to the user.
  bool is_privileged_;

  // List of functions to enable for automation, or a single entry "*" to
  // enable all functions for automation.  Ignored unless is_privileged_ is
  // true.  Defaults to the empty list, meaning automation will not be
  // turned on.
  std::vector<std::string> functions_enabled_;
};

#endif  // CHROME_FRAME_CHROME_FRAME_PLUGIN_H_

