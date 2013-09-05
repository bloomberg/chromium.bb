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
#include "base/strings/string16.h"
#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"
#include "chrome/browser/devtools/devtools_file_helper.h"
#include "chrome/browser/devtools/devtools_file_system_indexer.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_frontend_host_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"

class Browser;
class BrowserWindow;
class DevToolsControllerTest;
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

namespace user_prefs {
class PrefRegistrySyncable;
}

enum DevToolsDockSide {
  DEVTOOLS_DOCK_SIDE_UNDOCKED = 0,
  DEVTOOLS_DOCK_SIDE_BOTTOM,
  DEVTOOLS_DOCK_SIDE_RIGHT,
  DEVTOOLS_DOCK_SIDE_MINIMIZED
};

class DevToolsWindow : private content::NotificationObserver,
                       private content::WebContentsDelegate,
                       private content::DevToolsFrontendHostDelegate,
                       private DevToolsEmbedderMessageDispatcher::Delegate {
 public:
  typedef base::Callback<void(bool)> InfoBarCallback;

  static const char kDevToolsApp[];

  virtual ~DevToolsWindow();

  static std::string GetDevToolsWindowPlacementPrefKey();
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
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
  static void OpenExternalFrontend(Profile* profile,
                                   const std::string& frontend_uri,
                                   content::DevToolsAgentHost* agent_host);

  // Exposed for testing, normal clients should not use this method.
  static DevToolsWindow* ToggleDevToolsWindow(
      content::RenderViewHost* inspected_rvh,
      bool force_open,
      DevToolsToggleAction action);
  static void InspectElement(
      content::RenderViewHost* inspected_rvh, int x, int y);

  static int GetMinimumWidth();
  static int GetMinimumHeight();
  static int GetMinimizedHeight();

  // content::DevToolsFrontendHostDelegate:
  virtual void InspectedContentsClosing() OVERRIDE;

  content::WebContents* web_contents() { return web_contents_; }
  Browser* browser() { return browser_; }  // For tests.
  DevToolsDockSide dock_side() const { return dock_side_; }

  content::RenderViewHost* GetRenderViewHost();
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

  void Show(DevToolsToggleAction action);

 private:
  friend class DevToolsControllerTest;

  DevToolsWindow(Profile* profile,
                 const GURL& frontend_url,
                 content::RenderViewHost* inspected_rvh,
                 DevToolsDockSide dock_side);

  static DevToolsWindow* Create(Profile* profile,
                                const GURL& frontend_url,
                                content::RenderViewHost* inspected_rvh,
                                DevToolsDockSide dock_side,
                                bool shared_worker_frontend,
                                bool external_frontend);
  static GURL GetDevToolsURL(Profile* profile,
                             const GURL& base_url,
                             DevToolsDockSide dock_side,
                             bool shared_worker_frontend,
                             bool external_frontend);
  static DevToolsWindow* FindDevToolsWindow(content::DevToolsAgentHost*);
  static DevToolsWindow* AsDevToolsWindow(content::RenderViewHost*);
  static DevToolsDockSide GetDockSideFromPrefs(Profile* profile);
  static std::string SideToString(DevToolsDockSide dock_side);
  static DevToolsDockSide SideFromString(const std::string& dock_side);

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // content::WebContentsDelegate:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual content::JavaScriptDialogManager*
      GetJavaScriptDialogManager() OVERRIDE;
  virtual content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color) OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* web_contents,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE;

  // content::DevToolsFrontendHostDelegate override:
  virtual void DispatchOnEmbedder(const std::string& message) OVERRIDE;

  // DevToolsEmbedderMessageDispatcher::Delegate overrides:
  virtual void ActivateWindow() OVERRIDE;
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
  virtual void IndexPath(int request_id,
                         const std::string& file_system_path) OVERRIDE;
  virtual void StopIndexing(int request_id) OVERRIDE;
  virtual void SearchInPath(int request_id,
                            const std::string& file_system_path,
                            const std::string& query) OVERRIDE;

  // DevToolsFileHelper callbacks.
  void FileSavedAs(const std::string& url);
  void AppendedTo(const std::string& url);
  void FileSystemsLoaded(
      const std::vector<DevToolsFileHelper::FileSystem>& file_systems);
  void FileSystemAdded(const DevToolsFileHelper::FileSystem& file_system);
  void IndexingTotalWorkCalculated(int request_id,
                                   const std::string& file_system_path,
                                   int total_work);
  void IndexingWorked(int request_id,
                      const std::string& file_system_path,
                      int worked);
  void IndexingDone(int request_id, const std::string& file_system_path);
  void SearchCompleted(int request_id,
                       const std::string& file_system_path,
                       const std::vector<std::string>& file_paths);
  void ShowDevToolsConfirmInfoBar(const string16& message,
                                  const InfoBarCallback& callback);

  void CreateDevToolsBrowser();
  bool FindInspectedBrowserAndTabIndex(Browser**, int* tab);
  BrowserWindow* GetInspectedBrowserWindow();
  bool IsInspectedBrowserPopup();
  void UpdateFrontendDockSide();
  void Hide();
  void ScheduleAction(DevToolsToggleAction action);
  void DoAction();
  void UpdateTheme();
  void AddDevToolsExtensionsToClient();
  void CallClientFunction(const std::string& function_name,
                          const base::Value* arg1,
                          const base::Value* arg2,
                          const base::Value* arg3);
  void UpdateBrowserToolbar();
  bool IsDocked();
  void Restore();
  content::WebContents* GetInspectedWebContents();

  class InspectedWebContentsObserver;
  scoped_ptr<InspectedWebContentsObserver> inspected_contents_observer_;
  class FrontendWebContentsObserver;
  scoped_ptr<FrontendWebContentsObserver> frontend_contents_observer_;

  Profile* profile_;
  content::WebContents* web_contents_;
  Browser* browser_;
  DevToolsDockSide dock_side_;
  bool is_loaded_;
  DevToolsToggleAction action_on_load_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<content::DevToolsClientHost> frontend_host_;
  base::WeakPtrFactory<DevToolsWindow> weak_factory_;
  scoped_ptr<DevToolsFileHelper> file_helper_;
  scoped_refptr<DevToolsFileSystemIndexer> file_system_indexer_;
  typedef std::map<
      int,
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob> >
      IndexingJobsMap;
  IndexingJobsMap indexing_jobs_;
  int width_;
  int height_;
  DevToolsDockSide dock_side_before_minimized_;

  scoped_ptr<DevToolsEmbedderMessageDispatcher> embedder_message_dispatcher_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindow);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_H_
