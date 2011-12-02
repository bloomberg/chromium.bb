// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_frontend_host_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace IPC {
class Message;
}

class Browser;
class BrowserWindow;
class PrefService;
class Profile;
class RenderViewHost;
class TabContentsWrapper;

namespace base {
class Value;
}

namespace content {
class DevToolsAgentHost;
class DevToolsClientHost;
}

class DevToolsWindow : private content::NotificationObserver,
                       private TabContentsDelegate,
                       private content::DevToolsFrontendHostDelegate {
 public:
  static const char kDevToolsApp[];
  static void RegisterUserPrefs(PrefService* prefs);
  static TabContentsWrapper* GetDevToolsContents(TabContents* inspected_tab);
  static bool IsDevToolsWindow(RenderViewHost* window_rvh);

  static DevToolsWindow* OpenDevToolsWindowForWorker(
      Profile* profile,
      content::DevToolsAgentHost* worker_agent);
  static DevToolsWindow* CreateDevToolsWindowForWorker(Profile* profile);
  static DevToolsWindow* OpenDevToolsWindow(RenderViewHost* inspected_rvh);
  static DevToolsWindow* ToggleDevToolsWindow(RenderViewHost* inspected_rvh,
                                              DevToolsToggleAction action);
  static void InspectElement(RenderViewHost* inspected_rvh, int x, int y);

  virtual ~DevToolsWindow();

  // Overridden from DevToolsClientHost.
  virtual void InspectedTabClosing() OVERRIDE;
  virtual void TabReplaced(TabContents* new_tab) OVERRIDE;
  RenderViewHost* GetRenderViewHost();

  void Show(DevToolsToggleAction action);

  TabContentsWrapper* tab_contents() { return tab_contents_; }
  Browser* browser() { return browser_; }  // For tests.
  bool is_docked() { return docked_; }
  content::DevToolsClientHost* devtools_client_host() {
    return frontend_host_;
  }

 private:
  static DevToolsWindow* Create(Profile* profile,
                                RenderViewHost* inspected_rvh,
                                bool docked, bool shared_worker_frontend);
  DevToolsWindow(TabContentsWrapper* tab_contents, Profile* profile,
                 RenderViewHost* inspected_rvh, bool docked);

  void CreateDevToolsBrowser();
  bool FindInspectedBrowserAndTabIndex(Browser**, int* tab);
  BrowserWindow* GetInspectedBrowserWindow();
  bool IsInspectedBrowserPopupOrPanel();
  void UpdateFrontendAttachedState();

  // Overridden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void ScheduleAction(DevToolsToggleAction action);
  void DoAction();
  static GURL GetDevToolsUrl(Profile* profile, bool docked,
                             bool shared_worker_frontend);
  void UpdateTheme();
  void AddDevToolsExtensionsToClient();
  void CallClientFunction(const string16& function_name,
                          const base::Value& arg);
  // Overridden from TabContentsDelegate.
  virtual TabContents* OpenURLFromTab(TabContents* source,
                                      const OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;
  virtual void CloseContents(TabContents* source) OVERRIDE {}
  virtual bool CanReloadContents(TabContents* source) const OVERRIDE;
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual content::JavaScriptDialogCreator*
      GetJavaScriptDialogCreator() OVERRIDE;

  virtual void FrameNavigating(const std::string& url) OVERRIDE {}

  static DevToolsWindow* ToggleDevToolsWindow(RenderViewHost* inspected_rvh,
                                              bool force_open,
                                              DevToolsToggleAction action);
  static DevToolsWindow* AsDevToolsWindow(content::DevToolsClientHost*);

  // content::DevToolsClientHandlerDelegate overrides.
  virtual void ActivateWindow() OVERRIDE;
  virtual void CloseWindow() OVERRIDE;
  virtual void MoveWindow(int x, int y) OVERRIDE;
  virtual void DockWindow() OVERRIDE;
  virtual void UndockWindow() OVERRIDE;
  virtual void SaveToFile(const std::string& suggested_file_name,
                          const std::string& content) OVERRIDE;

  void RequestSetDocked(bool docked);

  Profile* profile_;
  TabContentsWrapper* inspected_tab_;
  TabContentsWrapper* tab_contents_;
  Browser* browser_;
  bool docked_;
  bool is_loaded_;
  DevToolsToggleAction action_on_load_;
  content::NotificationRegistrar registrar_;
  content::DevToolsClientHost* frontend_host_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindow);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_H_
