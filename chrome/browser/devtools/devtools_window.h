// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/devtools_file_helper.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_frontend_host_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"

class Browser;
class BrowserWindow;
class DevToolsControllerTest;
class PrefRegistrySyncable;
class Profile;

namespace base {
class Value;
}

namespace content {
class DevToolsAgentHost;
class DevToolsClientHost;
struct FileChooserParams;
class RenderViewHost;
class WebContents;
}

namespace IPC {
class Message;
}

enum DevToolsDockSide {
  DEVTOOLS_DOCK_SIDE_UNDOCKED = 0,
  DEVTOOLS_DOCK_SIDE_BOTTOM,
  DEVTOOLS_DOCK_SIDE_RIGHT
};

class DevToolsWindow : private content::NotificationObserver,
                       private content::WebContentsDelegate,
                       private content::DevToolsFrontendHostDelegate {
 public:
  static const char kDevToolsApp[];
  static std::string GetDevToolsWindowPlacementPrefKey();
  static void RegisterUserPrefs(PrefRegistrySyncable* registry);
  static DevToolsWindow* GetDockedInstanceForInspectedTab(
      content::WebContents* inspected_tab);
  static bool IsDevToolsWindow(content::RenderViewHost* window_rvh);

  static DevToolsWindow* OpenDevToolsWindowForWorker(
      Profile* profile,
      content::DevToolsAgentHost* worker_agent);
  static DevToolsWindow* CreateDevToolsWindowForWorker(Profile* profile);
  static DevToolsWindow* OpenDevToolsWindow(
      content::RenderViewHost* inspected_rvh);
  static DevToolsWindow* ToggleDevToolsWindow(
      Browser* browser,
      DevToolsToggleAction action);

  // Exposed for testing, normal clients should not use this method.
  static DevToolsWindow* ToggleDevToolsWindow(
      content::RenderViewHost* inspected_rvh,
      bool force_open,
      DevToolsToggleAction action);
  static void InspectElement(
      content::RenderViewHost* inspected_rvh, int x, int y);

  virtual ~DevToolsWindow();

  // Overridden from DevToolsClientHost.
  virtual void InspectedContentsClosing() OVERRIDE;
  content::RenderViewHost* GetRenderViewHost();

  void Show(DevToolsToggleAction action);

  content::WebContents* web_contents() { return web_contents_; }
  Browser* browser() { return browser_; }  // For tests.
  DevToolsDockSide dock_side() { return dock_side_; }

  content::DevToolsClientHost* GetDevToolsClientHostForTest();

  // Returns preferred devtools window width for given |container_width|. It
  // tries to use the saved window width, or, if none exists, 1/3 of the
  // container width, then clamps to try and ensure both devtools and content
  // are at least somewhat visible.
  // Called only for the case when devtools window is docked to the side.
  int GetWidth(int container_width);

  // Returns preferred devtools window height for given |container_height|.
  // Uses the same logic as GetWidth.
  // Called only for the case when devtools window is docked to bottom.
  int GetHeight(int container_height);

  // Stores preferred devtools window width for this instance.
  void SetWidth(int width);

  // Stores preferred devtools window height for this instance.
  void SetHeight(int height);

 private:
  friend class DevToolsControllerTest;
  static DevToolsWindow* Create(Profile* profile,
                                content::RenderViewHost* inspected_rvh,
                                DevToolsDockSide dock_side,
                                bool shared_worker_frontend);
  DevToolsWindow(content::WebContents* web_contents,
                 Profile* profile,
                 content::RenderViewHost* inspected_rvh,
                 DevToolsDockSide dock_side);

  void CreateDevToolsBrowser();
  bool FindInspectedBrowserAndTabIndex(Browser**, int* tab);
  BrowserWindow* GetInspectedBrowserWindow();
  bool IsInspectedBrowserPopupOrPanel();
  void UpdateFrontendDockSide();

  // Overridden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void ScheduleAction(DevToolsToggleAction action);
  void DoAction();
  static GURL GetDevToolsUrl(Profile* profile,
                             DevToolsDockSide dock_side,
                             bool shared_worker_frontend);
  void UpdateTheme();
  void AddDevToolsExtensionsToClient();
  void CallClientFunction(const std::string& function_name,
                          const base::Value* arg1 = NULL,
                          const base::Value* arg2 = NULL);
  // Overridden from content::WebContentsDelegate.
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE {}
  virtual bool PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual content::JavaScriptDialogManager*
      GetJavaScriptDialogManager() OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* web_contents,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE;

  static DevToolsWindow* AsDevToolsWindow(content::DevToolsClientHost*);
  static DevToolsWindow* AsDevToolsWindow(content::RenderViewHost*);

  // content::DevToolsFrontendHostDelegate overrides.
  virtual void ActivateWindow() OVERRIDE;
  virtual void ChangeAttachedWindowHeight(unsigned height) OVERRIDE;
  virtual void CloseWindow() OVERRIDE;
  virtual void MoveWindow(int x, int y) OVERRIDE;
  virtual void SetDockSide(const std::string& side) OVERRIDE;
  virtual void OpenInNewTab(const std::string& url) OVERRIDE;
  virtual void SaveToFile(const std::string& url,
                          const std::string& content,
                          bool save_as) OVERRIDE;
  virtual void AppendToFile(const std::string& url,
                            const std::string& content) OVERRIDE;
  virtual void RequestFileSystems() OVERRIDE;
  virtual void AddFileSystem() OVERRIDE;
  virtual void RemoveFileSystem(const std::string& file_system_path) OVERRIDE;

  // DevToolsFileHelper callbacks.
  void FileSavedAs(const std::string& url);
  void AppendedTo(const std::string& url);
  void FileSystemsLoaded(
      const std::vector<DevToolsFileHelper::FileSystem>& file_systems);
  void FileSystemAdded(std::string error_string,
                       const DevToolsFileHelper::FileSystem& file_system);

  void UpdateBrowserToolbar();
  bool IsDocked();
  static DevToolsDockSide GetDockSideFromPrefs(Profile* profile);
  static std::string SideToString(DevToolsDockSide dock_side);
  static DevToolsDockSide SideFromString(const std::string& dock_side);

  Profile* profile_;
  content::WebContents* inspected_web_contents_;
  content::WebContents* web_contents_;
  Browser* browser_;
  DevToolsDockSide dock_side_;
  bool is_loaded_;
  DevToolsToggleAction action_on_load_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<content::DevToolsClientHost> frontend_host_;
  base::WeakPtrFactory<DevToolsWindow> weak_factory_;
  scoped_ptr<DevToolsFileHelper> file_helper_;
  int width_;
  int height_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindow);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_H_
