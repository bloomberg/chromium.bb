// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "headless/lib/browser/headless_url_request_context_getter.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"

namespace headless {
class HeadlessBrowserImpl;
class HeadlessResourceContext;
class HeadlessWebContentsImpl;

class HeadlessBrowserContextImpl : public HeadlessBrowserContext,
                                   public content::BrowserContext {
 public:
  ~HeadlessBrowserContextImpl() override;

  static HeadlessBrowserContextImpl* From(
      HeadlessBrowserContext* browser_context);
  static HeadlessBrowserContextImpl* From(
      content::BrowserContext* browser_context);

  static std::unique_ptr<HeadlessBrowserContextImpl> Create(
      HeadlessBrowserContext::Builder* builder);

  // HeadlessBrowserContext implementation:
  HeadlessWebContents::Builder CreateWebContentsBuilder() override;
  std::vector<HeadlessWebContents*> GetAllWebContents() override;
  HeadlessWebContents* GetWebContentsForDevToolsAgentHostId(
      const std::string& devtools_agent_host_id) override;
  void Close() override;
  const std::string& Id() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void SetFrameTreeNodeId(int render_process_id,
                          int render_frame_routing_id,
                          int frame_tree_node_id);

  void RemoveFrameTreeNode(int render_process_id, int render_frame_routing_id);

  // BrowserContext implementation:
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  base::FilePath GetPath() const override;
  bool IsOffTheRecord() const override;
  content::ResourceContext* GetResourceContext() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionManager* GetPermissionManager() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;

  HeadlessWebContents* CreateWebContents(HeadlessWebContents::Builder* builder);
  // Register web contents which were created not through Headless API
  // (calling window.open() is a best example for this).
  void RegisterWebContents(
      std::unique_ptr<HeadlessWebContentsImpl> web_contents);
  void DestroyWebContents(HeadlessWebContentsImpl* web_contents);

  HeadlessBrowserImpl* browser() const;
  const HeadlessBrowserContextOptions* options() const;

  // Returns the FrameTreeNode id for the corresponding RenderFrameHost or -1
  // if it can't be found. Can be called on any thread.
  int GetFrameTreeNodeId(int render_process_id, int render_frame_id) const;

  void NotifyChildContentsCreated(HeadlessWebContentsImpl* parent,
                                  HeadlessWebContentsImpl* child);

 private:
  HeadlessBrowserContextImpl(
      HeadlessBrowserImpl* browser,
      std::unique_ptr<HeadlessBrowserContextOptions> context_options);

  // Performs initialization of the HeadlessBrowserContextImpl while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  HeadlessBrowserImpl* browser_;  // Not owned.
  std::unique_ptr<HeadlessBrowserContextOptions> context_options_;
  std::unique_ptr<HeadlessResourceContext> resource_context_;
  base::FilePath path_;
  base::ObserverList<Observer> observers_;

  std::unordered_map<std::string, std::unique_ptr<HeadlessWebContents>>
      web_contents_map_;

  // Guards |frame_tree_node_map_| from being concurrently written on the UI
  // thread and read on the IO thread.
  // TODO(alexclarke): Remove if we can add FrameTreeNode ID to
  // URLRequestUserData. See https://crbug.com/715541
  mutable base::Lock frame_tree_node_map_lock_;
  std::map<std::pair<int, int>, int> frame_tree_node_map_;

  std::unique_ptr<content::PermissionManager> permission_manager_;

  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserContextImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_
