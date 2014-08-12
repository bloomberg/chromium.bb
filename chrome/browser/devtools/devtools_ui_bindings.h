// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_UI_BINDINGS_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_UI_BINDINGS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"
#include "chrome/browser/devtools/devtools_file_helper.h"
#include "chrome/browser/devtools/devtools_file_system_indexer.h"
#include "chrome/browser/devtools/devtools_targets_ui.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/size.h"

class InfoBarService;
class Profile;

namespace content {
class DevToolsClientHost;
struct FileChooserParams;
class WebContents;
}

// Base implementation of DevTools bindings around front-end.
class DevToolsUIBindings : public content::NotificationObserver,
                           public content::DevToolsFrontendHost::Delegate,
                           public DevToolsEmbedderMessageDispatcher::Delegate,
                           public DevToolsAndroidBridge::DeviceCountListener,
                           public content::DevToolsClientHost {
 public:
  static GURL ApplyThemeToURL(Profile* profile, const GURL& base_url);

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void ActivateWindow() = 0;
    virtual void CloseWindow() = 0;
    virtual void SetInspectedPageBounds(const gfx::Rect& rect) = 0;
    virtual void InspectElementCompleted() = 0;
    virtual void MoveWindow(int x, int y) = 0;
    virtual void SetIsDocked(bool is_docked) = 0;
    virtual void OpenInNewTab(const std::string& url) = 0;
    virtual void SetWhitelistedShortcuts(const std::string& message) = 0;

    virtual void InspectedContentsClosing() = 0;
    virtual void OnLoadCompleted() = 0;
    virtual InfoBarService* GetInfoBarService() = 0;
    virtual void RenderProcessGone() = 0;
  };

  DevToolsUIBindings(content::WebContents* web_contents, const GURL& url);
  virtual ~DevToolsUIBindings();

  content::WebContents* web_contents() { return web_contents_; }
  Profile* profile() { return profile_; }

  // Takes ownership over the |delegate|.
  void SetDelegate(Delegate* delegate);
  void CallClientFunction(const std::string& function_name,
                          const base::Value* arg1,
                          const base::Value* arg2,
                          const base::Value* arg3);
 private:
  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // content::DevToolsFrontendHost::Delegate implementation.
  virtual void HandleMessageFromDevToolsFrontend(
      const std::string& message) OVERRIDE;
  virtual void HandleMessageFromDevToolsFrontendToBackend(
      const std::string& message) OVERRIDE;

  // content::DevToolsClientHost implementation.
  virtual void DispatchOnInspectorFrontend(const std::string& message) OVERRIDE;
  virtual void InspectedContentsClosing() OVERRIDE;
  virtual void ReplacedWithAnotherClient() OVERRIDE;

  // DevToolsEmbedderMessageDispatcher::Delegate implementation.
  virtual void ActivateWindow() OVERRIDE;
  virtual void CloseWindow() OVERRIDE;
  virtual void SetInspectedPageBounds(const gfx::Rect& rect) OVERRIDE;
  virtual void InspectElementCompleted() OVERRIDE;
  virtual void InspectedURLChanged(const std::string& url) OVERRIDE;
  virtual void MoveWindow(int x, int y) OVERRIDE;
  virtual void SetIsDocked(bool is_docked) OVERRIDE;
  virtual void OpenInNewTab(const std::string& url) OVERRIDE;
  virtual void SaveToFile(const std::string& url,
                          const std::string& content,
                          bool save_as) OVERRIDE;
  virtual void AppendToFile(const std::string& url,
                            const std::string& content) OVERRIDE;
  virtual void RequestFileSystems() OVERRIDE;
  virtual void AddFileSystem() OVERRIDE;
  virtual void RemoveFileSystem(const std::string& file_system_path) OVERRIDE;
  virtual void UpgradeDraggedFileSystemPermissions(
      const std::string& file_system_url) OVERRIDE;
  virtual void IndexPath(int request_id,
                         const std::string& file_system_path) OVERRIDE;
  virtual void StopIndexing(int request_id) OVERRIDE;
  virtual void SearchInPath(int request_id,
                            const std::string& file_system_path,
                            const std::string& query) OVERRIDE;
  virtual void SetWhitelistedShortcuts(const std::string& message) OVERRIDE;
  virtual void ZoomIn() OVERRIDE;
  virtual void ZoomOut() OVERRIDE;
  virtual void ResetZoom() OVERRIDE;
  virtual void OpenUrlOnRemoteDeviceAndInspect(const std::string& browser_id,
                                               const std::string& url) OVERRIDE;
  virtual void SetDeviceCountUpdatesEnabled(bool enabled) OVERRIDE;
  virtual void SetDevicesUpdatesEnabled(bool enabled) OVERRIDE;
  virtual void SendMessageToBrowser(const std::string& message) OVERRIDE;

  void EnableRemoteDeviceCounter(bool enable);

  // DevToolsAndroidBridge::DeviceCountListener override:
  virtual void DeviceCountChanged(int count) OVERRIDE;

  // Forwards discovered devices to frontend.
  virtual void DevicesUpdated(const std::string& source,
                              const base::ListValue& targets);

  void DocumentOnLoadCompletedInMainFrame();

  // DevToolsFileHelper callbacks.
  void FileSavedAs(const std::string& url);
  void CanceledFileSaveAs(const std::string& url);
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
  typedef base::Callback<void(bool)> InfoBarCallback;
  void ShowDevToolsConfirmInfoBar(const base::string16& message,
                                  const InfoBarCallback& callback);

  // Theme and extensions support.
  void UpdateTheme();
  void AddDevToolsExtensionsToClient();

  class FrontendWebContentsObserver;
  friend class FrontendWebContentsObserver;
  scoped_ptr<FrontendWebContentsObserver> frontend_contents_observer_;

  Profile* profile_;
  content::WebContents* web_contents_;
  scoped_ptr<Delegate> delegate_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<content::DevToolsFrontendHost> frontend_host_;
  scoped_ptr<DevToolsFileHelper> file_helper_;
  scoped_refptr<DevToolsFileSystemIndexer> file_system_indexer_;
  typedef std::map<
      int,
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob> >
      IndexingJobsMap;
  IndexingJobsMap indexing_jobs_;

  bool device_count_updates_enabled_;
  bool devices_updates_enabled_;
  scoped_ptr<DevToolsTargetsUIHandler> remote_targets_handler_;
  scoped_ptr<DevToolsEmbedderMessageDispatcher> embedder_message_dispatcher_;
  GURL url_;
  base::WeakPtrFactory<DevToolsUIBindings> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsUIBindings);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_UI_BINDINGS_H_
