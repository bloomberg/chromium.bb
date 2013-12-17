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
        incognito_mode, is_widget_mode, route_all_top_level_navigations);
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

 protected:
  LRESULT OnSetFocus(UINT message, WPARAM wparam, LPARAM lparam,
                     BOOL& handled) {  // NO_LINT
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

  // Allow overriding the type of automation client used, for unit tests.
  virtual ChromeFrameAutomationClient* CreateAutomationClient() {
    return new ChromeFrameAutomationClient;
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
