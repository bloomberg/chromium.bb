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
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "ui/gfx/geometry/size.h"

class InfoBarService;
class Profile;

namespace content {
struct FileChooserParams;
class WebContents;
}

// Base implementation of DevTools bindings around front-end.
class DevToolsUIBindings : public content::NotificationObserver,
                           public content::DevToolsFrontendHost::Delegate,
                           public DevToolsEmbedderMessageDispatcher::Delegate,
                           public DevToolsAndroidBridge::DeviceCountListener,
                           public content::DevToolsAgentHostClient,
                           public net::URLFetcherDelegate {
 public:
  static DevToolsUIBindings* ForWebContents(
      content::WebContents* web_contents);
  static GURL ApplyThemeToURL(Profile* profile, const GURL& base_url);

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void ActivateWindow() = 0;
    virtual void CloseWindow() = 0;
    virtual void SetInspectedPageBounds(const gfx::Rect& rect) = 0;
    virtual void InspectElementCompleted() = 0;
    virtual void SetIsDocked(bool is_docked) = 0;
    virtual void OpenInNewTab(const std::string& url) = 0;
    virtual void SetWhitelistedShortcuts(const std::string& message) = 0;

    virtual void InspectedContentsClosing() = 0;
    virtual void OnLoadCompleted() = 0;
    virtual InfoBarService* GetInfoBarService() = 0;
    virtual void RenderProcessGone(bool crashed) = 0;
  };

  explicit DevToolsUIBindings(content::WebContents* web_contents);
  ~DevToolsUIBindings() override;

  content::WebContents* web_contents() { return web_contents_; }
  Profile* profile() { return profile_; }
  content::DevToolsAgentHost* agent_host() { return agent_host_.get(); }

  // Takes ownership over the |delegate|.
  void SetDelegate(Delegate* delegate);
  void CallClientFunction(const std::string& function_name,
                          const base::Value* arg1,
                          const base::Value* arg2,
                          const base::Value* arg3);
  void AttachTo(const scoped_refptr<content::DevToolsAgentHost>& agent_host);
  void Reattach();
  void Detach();
  bool IsAttachedTo(content::DevToolsAgentHost* agent_host);

 private:
  // content::NotificationObserver overrides.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::DevToolsFrontendHost::Delegate implementation.
  void HandleMessageFromDevToolsFrontend(const std::string& message) override;
  void HandleMessageFromDevToolsFrontendToBackend(
      const std::string& message) override;

  // content::DevToolsAgentHostClient implementation.
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               const std::string& message) override;
  void AgentHostClosed(content::DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override;

  // DevToolsEmbedderMessageDispatcher::Delegate implementation.
  void ActivateWindow(int request_id) override;
  void CloseWindow(int request_id) override;
  void LoadCompleted(int request_id) override;
  void SetInspectedPageBounds(int request_id,
                              const gfx::Rect& rect) override;
  void InspectElementCompleted(int request_id) override;
  void InspectedURLChanged(int request_id, const std::string& url) override;
  void LoadNetworkResource(int request_id,
                           const std::string& url,
                           const std::string& headers,
                           int stream_id) override;
  void SetIsDocked(int request_id, bool is_docked) override;
  void OpenInNewTab(int request_id, const std::string& url) override;
  void SaveToFile(int request_id,
                  const std::string& url,
                  const std::string& content,
                  bool save_as) override;
  void AppendToFile(int request_id,
                    const std::string& url,
                    const std::string& content) override;
  void RequestFileSystems(int request_id) override;
  void AddFileSystem(int request_id) override;
  void RemoveFileSystem(int request_id,
                        const std::string& file_system_path) override;
  void UpgradeDraggedFileSystemPermissions(
      int request_id,
      const std::string& file_system_url) override;
  void IndexPath(int request_id,
                 int index_request_id,
                 const std::string& file_system_path) override;
  void StopIndexing(int request_id, int index_request_id) override;
  void SearchInPath(int request_id,
                    int search_request_id,
                    const std::string& file_system_path,
                    const std::string& query) override;
  void SetWhitelistedShortcuts(int request_id,
                               const std::string& message) override;
  void ZoomIn(int request_id) override;
  void ZoomOut(int request_id) override;
  void ResetZoom(int request_id) override;
  void OpenUrlOnRemoteDeviceAndInspect(int request_id,
                                       const std::string& browser_id,
                                       const std::string& url) override;
  void SetDeviceCountUpdatesEnabled(int request_id, bool enabled) override;
  void SetDevicesUpdatesEnabled(int request_id, bool enabled) override;
  void SendMessageToBrowser(int request_id,
                            const std::string& message) override;
  void RecordActionUMA(int request_id,
                       const std::string& name,
                       int action) override;

  // net::URLFetcherDelegate overrides.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void EnableRemoteDeviceCounter(bool enable);

  void SendMessageAck(int request_id,
                      const base::Value* arg1);

  // DevToolsAndroidBridge::DeviceCountListener override:
  void DeviceCountChanged(int count) override;

  // Forwards discovered devices to frontend.
  virtual void DevicesUpdated(const std::string& source,
                              const base::ListValue& targets);

  void DocumentOnLoadCompletedInMainFrame();
  void DidNavigateMainFrame();
  void FrontendLoaded();

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
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
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
  bool frontend_loaded_;
  scoped_ptr<DevToolsTargetsUIHandler> remote_targets_handler_;
  scoped_ptr<DevToolsEmbedderMessageDispatcher> embedder_message_dispatcher_;
  GURL url_;
  using PendingRequestsMap = std::map<const net::URLFetcher*, int>;
  PendingRequestsMap pending_requests_;
  base::WeakPtrFactory<DevToolsUIBindings> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsUIBindings);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_UI_BINDINGS_H_
