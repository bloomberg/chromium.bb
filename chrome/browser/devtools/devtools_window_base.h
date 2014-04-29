// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_BASE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_BASE_H_

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
#include "content/public/browser/devtools_frontend_host_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/size.h"

class Profile;

namespace content {
class DevToolsClientHost;
struct FileChooserParams;
class WebContents;
}

// Base implementation of DevTools bindings around front-end.
class DevToolsWindowBase : public content::NotificationObserver,
                           public content::DevToolsFrontendHostDelegate,
                           public DevToolsEmbedderMessageDispatcher::Delegate,
                           public DevToolsAndroidBridge::DeviceCountListener {
 public:
  virtual ~DevToolsWindowBase();

  content::WebContents* web_contents() { return web_contents_; }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // content::DevToolsFrontendHostDelegate override:
  virtual void InspectedContentsClosing() OVERRIDE;
  virtual void DispatchOnEmbedder(const std::string& message) OVERRIDE;

  // DevToolsEmbedderMessageDispatcher::Delegate overrides:
  virtual void ActivateWindow() OVERRIDE;
  virtual void CloseWindow() OVERRIDE;
  virtual void SetContentsInsets(
      int left, int top, int right, int bottom) OVERRIDE;
  virtual void SetContentsResizingStrategy(
      const gfx::Insets& insets, const gfx::Size& min_size) OVERRIDE;
  virtual void InspectElementCompleted() OVERRIDE;
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
  virtual void StartRemoteDevicesListener() OVERRIDE;
  virtual void StopRemoteDevicesListener() OVERRIDE;
  virtual void EnableRemoteDeviceCounter(bool enable) OVERRIDE;

  // DevToolsAndroidBridge::DeviceCountListener override:
  virtual void DeviceCountChanged(int count) OVERRIDE;

  // Forwards discovered devices to frontend.
  virtual void PopulateRemoteDevices(const std::string& source,
                                     scoped_ptr<base::ListValue> targets);

 protected:
  DevToolsWindowBase(content::WebContents* web_contents,
                     const GURL& frontend_url);

  virtual void AddDevToolsExtensionsToClient();
  virtual void DocumentOnLoadCompletedInMainFrame();
  void CallClientFunction(const std::string& function_name,
                          const base::Value* arg1,
                          const base::Value* arg2,
                          const base::Value* arg3);
  Profile* profile() { return profile_; }
  content::DevToolsClientHost* frontend_host() { return frontend_host_.get(); }

 private:
  typedef base::Callback<void(bool)> InfoBarCallback;

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
  void ShowDevToolsConfirmInfoBar(const base::string16& message,
                                  const InfoBarCallback& callback);

  // Theme and extensions support.
  GURL ApplyThemeToURL(const GURL& base_url);
  void UpdateTheme();

  class FrontendWebContentsObserver;
  friend class FrontendWebContentsObserver;
  scoped_ptr<FrontendWebContentsObserver> frontend_contents_observer_;

  Profile* profile_;
  content::WebContents* web_contents_;
  bool device_listener_enabled_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<content::DevToolsClientHost> frontend_host_;
  scoped_ptr<DevToolsFileHelper> file_helper_;
  scoped_refptr<DevToolsFileSystemIndexer> file_system_indexer_;
  typedef std::map<
      int,
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob> >
      IndexingJobsMap;
  IndexingJobsMap indexing_jobs_;

  scoped_ptr<DevToolsRemoteTargetsUIHandler> remote_targets_handler_;
  scoped_ptr<DevToolsEmbedderMessageDispatcher> embedder_message_dispatcher_;
  base::WeakPtrFactory<DevToolsWindowBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsWindowBase);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_BASE_H_
