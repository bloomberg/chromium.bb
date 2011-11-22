// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "content/browser/debugger/devtools_client_host.h"
#include "content/browser/renderer_host/render_view_host_observer.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace IPC {
class Message;
}

class Browser;
class BrowserWindow;
class DevToolsAgentHost;
class PrefService;
class Profile;
class RenderViewHost;
class TabContentsWrapper;

namespace base {
class Value;
}

class DevToolsWindow
    : public DevToolsClientHost,
      private content::NotificationObserver,
      private TabContentsDelegate,
      private RenderViewHostObserver {
 public:
  static const char kDevToolsApp[];
  static void RegisterUserPrefs(PrefService* prefs);
  static TabContentsWrapper* GetDevToolsContents(TabContents* inspected_tab);

  static DevToolsWindow* OpenDevToolsWindowForWorker(
      Profile* profile,
      DevToolsAgentHost* worker_agent);
  static DevToolsWindow* CreateDevToolsWindowForWorker(Profile* profile);
  static DevToolsWindow* OpenDevToolsWindow(RenderViewHost* inspected_rvh);
  static DevToolsWindow* ToggleDevToolsWindow(RenderViewHost* inspected_rvh,
                                              DevToolsToggleAction action);
  static void InspectElement(RenderViewHost* inspected_rvh, int x, int y);

  virtual ~DevToolsWindow();

  // Overridden from DevToolsClientHost.
  virtual void SendMessageToClient(const IPC::Message& message) OVERRIDE;
  virtual void InspectedTabClosing() OVERRIDE;
  virtual void TabReplaced(TabContents* new_tab) OVERRIDE;
  RenderViewHost* GetRenderViewHost();

  void Show(DevToolsToggleAction action);

  TabContentsWrapper* tab_contents() { return tab_contents_; }
  Browser* browser() { return browser_; }  // For tests.
  bool is_docked() { return docked_; }

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
  // Deprecated. Use two-arguments variant instead.
  virtual TabContents* OpenURLFromTab(
      TabContents* source,
      const GURL& url,
      const GURL& referrer,
      WindowOpenDisposition disposition,
      content::PageTransition transition) OVERRIDE;
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
  static DevToolsWindow* AsDevToolsWindow(DevToolsClientHost*);

  // RenderViewHostObserver overrides.
  virtual void RenderViewHostDestroyed(RenderViewHost* rvh) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnActivateWindow();
  void OnCloseWindow();
  void OnMoveWindow(int x, int y);
  void OnRequestDockWindow();
  void OnRequestUndockWindow();
  void OnSaveAs(const std::string& file_name,
                const std::string& content);
  void RequestSetDocked(bool docked);

  Profile* profile_;
  TabContentsWrapper* inspected_tab_;
  TabContentsWrapper* tab_contents_;
  Browser* browser_;
  bool docked_;
  bool is_loaded_;
  DevToolsToggleAction action_on_load_;
  content::NotificationRegistrar registrar_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindow);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_H_
