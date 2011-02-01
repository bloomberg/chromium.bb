// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

namespace IPC {
class Message;
}

class Browser;
class BrowserWindow;
class Profile;
class RenderViewHost;
class Value;

class DevToolsWindow
    : public DevToolsClientHost,
      public NotificationObserver,
      public TabContentsDelegate {
 public:
  static const char kDevToolsApp[];
  static TabContents* GetDevToolsContents(TabContents* inspected_tab);

  DevToolsWindow(Profile* profile, RenderViewHost* inspected_rvh, bool docked);
  virtual ~DevToolsWindow();

  // Overridden from DevToolsClientHost.
  virtual DevToolsWindow* AsDevToolsWindow();
  virtual void SendMessageToClient(const IPC::Message& message);
  virtual void InspectedTabClosing();
  virtual void TabReplaced(TabContentsWrapper* new_tab);

  void Show(DevToolsToggleAction action);
  void Activate();
  void SetDocked(bool docked);
  RenderViewHost* GetRenderViewHost();

  TabContentsWrapper* tab_contents() { return tab_contents_; }
  Browser* browser() { return browser_; }  // For tests.
  bool is_docked() { return docked_; }

 private:
  void CreateDevToolsBrowser();
  bool FindInspectedBrowserAndTabIndex(Browser**, int* tab);
  BrowserWindow* GetInspectedBrowserWindow();
  void SetAttachedWindow();

  // Overridden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void ScheduleAction(DevToolsToggleAction action);
  void DoAction();
  GURL GetDevToolsUrl();
  void UpdateTheme();
  void AddDevToolsExtensionsToClient();
  void CallClientFunction(const std::wstring& function_name,
                          const Value& arg);
  // Overridden from TabContentsDelegate.
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) {}
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents) {}
  virtual void DeactivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void CloseContents(TabContents* source) {}
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual bool CanReloadContents(TabContents* source) const;
  virtual void URLStarredChanged(TabContents* source, bool starred) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

  virtual void FrameNavigating(const std::string& url) {}

  Profile* profile_;
  TabContents* inspected_tab_;
  TabContentsWrapper* tab_contents_;
  Browser* browser_;
  bool docked_;
  bool is_loaded_;
  DevToolsToggleAction action_on_load_;
  NotificationRegistrar registrar_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindow);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_WINDOW_H_
