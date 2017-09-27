// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test_utils_internal.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/file_chooser_file_info.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "net/url_request/url_request.h"

namespace content {

void NavigateFrameToURL(FrameTreeNode* node, const GURL& url) {
  TestFrameNavigationObserver observer(node);
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = node->frame_tree_node_id();
  node->navigator()->GetController()->LoadURLWithParams(params);
  observer.Wait();
}

void SetShouldProceedOnBeforeUnload(Shell* shell, bool proceed) {
  ShellJavaScriptDialogManager* manager =
      static_cast<ShellJavaScriptDialogManager*>(
          shell->GetJavaScriptDialogManager(shell->web_contents()));
  manager->set_should_proceed_on_beforeunload(proceed);
}

RenderFrameHost* ConvertToRenderFrameHost(FrameTreeNode* frame_tree_node) {
  return frame_tree_node->current_frame_host();
}

FrameTreeVisualizer::FrameTreeVisualizer() {
}

FrameTreeVisualizer::~FrameTreeVisualizer() {
}

std::string FrameTreeVisualizer::DepictFrameTree(FrameTreeNode* root) {
  // Tracks the sites actually used in this depiction.
  std::map<std::string, SiteInstance*> legend;

  // Traversal 1: Assign names to current frames. This ensures that the first
  // call to the pretty-printer will result in a naming of the site instances
  // that feels natural and stable.
  std::stack<FrameTreeNode*> to_explore;
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    RenderFrameHost* current = node->render_manager()->current_frame_host();
    legend[GetName(current->GetSiteInstance())] = current->GetSiteInstance();
  }

  // Traversal 2: Assign names to the pending/speculative frames. For stability
  // of assigned names it's important to do this before trying to name the
  // proxies, which have a less well defined order.
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    RenderFrameHost* pending = node->render_manager()->pending_frame_host();
    RenderFrameHost* spec = node->render_manager()->speculative_frame_host();
    if (pending)
      legend[GetName(pending->GetSiteInstance())] = pending->GetSiteInstance();
    if (spec)
      legend[GetName(spec->GetSiteInstance())] = spec->GetSiteInstance();
  }

  // Traversal 3: Assign names to the proxies and add them to |legend| too.
  // Typically, only openers should have their names assigned this way.
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    // Sort the proxies by SiteInstance ID to avoid unordered_map ordering.
    std::vector<SiteInstance*> site_instances;
    for (const auto& proxy_pair :
         node->render_manager()->GetAllProxyHostsForTesting()) {
      site_instances.push_back(proxy_pair.second->GetSiteInstance());
    }
    std::sort(site_instances.begin(), site_instances.end(),
              [](SiteInstance* lhs, SiteInstance* rhs) {
                return lhs->GetId() < rhs->GetId();
              });

    for (SiteInstance* site_instance : site_instances)
      legend[GetName(site_instance)] = site_instance;
  }

  // Traversal 4: Now that all names are assigned, make a big loop to pretty-
  // print the tree. Each iteration produces exactly one line of format.
  std::string result;
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    // Draw the feeler line tree graphics by walking up to the root. A feeler
    // line is needed for each ancestor that is the last child of its parent.
    // This creates the ASCII art that looks like:
    //    Foo
    //      |--Foo
    //      |--Foo
    //      |    |--Foo
    //      |    +--Foo
    //      |         +--Foo
    //      +--Foo
    //           +--Foo
    //
    // TODO(nick): Make this more elegant.
    std::string line;
    if (node != root) {
      if (node->parent()->child_at(node->parent()->child_count() - 1) != node)
        line = "  |--";
      else
        line = "  +--";
      for (FrameTreeNode* up = node->parent(); up != root; up = up->parent()) {
        if (up->parent()->child_at(up->parent()->child_count() - 1) != up)
          line = "  |  " + line;
        else
          line = "     " + line;
      }
    }

    // Prefix one extra space of padding for two reasons. First, this helps the
    // diagram aligns nicely with the legend. Second, this makes it easier to
    // read the diffs that gtest spits out on EXPECT_EQ failure.
    line = " " + line;

    // Summarize the FrameTreeNode's state. Always show the site of the current
    // RenderFrameHost, and show any exceptional state of the node, like a
    // pending or speculative RenderFrameHost.
    RenderFrameHost* current = node->render_manager()->current_frame_host();
    RenderFrameHost* pending = node->render_manager()->pending_frame_host();
    RenderFrameHost* spec = node->render_manager()->speculative_frame_host();
    base::StringAppendF(&line, "Site %s",
                        GetName(current->GetSiteInstance()).c_str());
    if (pending) {
      base::StringAppendF(&line, " (%s pending)",
                          GetName(pending->GetSiteInstance()).c_str());
    }
    if (spec) {
      base::StringAppendF(&line, " (%s speculative)",
                          GetName(spec->GetSiteInstance()).c_str());
    }

    // Show the SiteInstances of the RenderFrameProxyHosts of this node.
    const auto& proxy_host_map =
        node->render_manager()->GetAllProxyHostsForTesting();
    if (!proxy_host_map.empty()) {
      // Show a dashed line of variable length before the proxy list. Always at
      // least two dashes.
      line.append(" --");

      // To make proxy lists align vertically for the first three tree levels,
      // pad with dashes up to a first tab stop at column 19 (which works out to
      // text editor column 28 in the typical diagram fed to EXPECT_EQ as a
      // string literal). Lining the lists up vertically makes differences in
      // the proxy sets easier to spot visually. We choose not to use the
      // *actual* tree height here, because that would make the diagram's
      // appearance less stable as the tree's shape evolves.
      while (line.length() < 20) {
        line.append("-");
      }
      line.append(" proxies for");

      // Sort these alphabetically, to avoid hash_map ordering dependency.
      std::vector<std::string> sorted_proxy_hosts;
      for (const auto& proxy_pair : proxy_host_map) {
        sorted_proxy_hosts.push_back(
            GetName(proxy_pair.second->GetSiteInstance()));
      }
      std::sort(sorted_proxy_hosts.begin(), sorted_proxy_hosts.end());
      for (std::string& proxy_name : sorted_proxy_hosts) {
        base::StringAppendF(&line, " %s", proxy_name.c_str());
      }
    }
    if (node != root)
      result.append("\n");
    result.append(line);
  }

  // Finally, show a legend with details of the site instances.
  const char* prefix = "Where ";
  for (auto& legend_entry : legend) {
    SiteInstanceImpl* site_instance =
        static_cast<SiteInstanceImpl*>(legend_entry.second);
    std::string description = site_instance->GetSiteURL().spec();
    if (site_instance->IsDefaultSubframeSiteInstance())
      description = "default subframe process";
    base::StringAppendF(&result, "\n%s%s = %s", prefix,
                        legend_entry.first.c_str(), description.c_str());
    // Highlight some exceptionable conditions.
    if (site_instance->active_frame_count() == 0)
      result.append(" (active_frame_count == 0)");
    if (!site_instance->GetProcess()->HasConnection())
      result.append(" (no process)");
    prefix = "      ";
  }
  return result;
}

std::string FrameTreeVisualizer::GetName(SiteInstance* site_instance) {
  // Indices into the vector correspond to letters of the alphabet.
  size_t index =
      std::find(seen_site_instance_ids_.begin(), seen_site_instance_ids_.end(),
                site_instance->GetId()) -
      seen_site_instance_ids_.begin();
  if (index == seen_site_instance_ids_.size())
    seen_site_instance_ids_.push_back(site_instance->GetId());

  // Whosoever writes a test using >=26 site instances shall be a lucky ducky.
  if (index < 25)
    return base::StringPrintf("%c", 'A' + static_cast<char>(index));
  else
    return base::StringPrintf("Z%d", static_cast<int>(index - 25));
}

Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name) {
  ShellAddedObserver new_shell_observer;
  bool did_create_popup = false;
  bool did_execute_script = ExecuteScriptAndExtractBool(
      opener,
      "window.domAutomationController.send("
      "    !!window.open('" + url.spec() + "', '" + name + "'));",
      &did_create_popup);
  if (!did_execute_script || !did_create_popup)
    return nullptr;

  Shell* new_shell = new_shell_observer.GetShell();
  WaitForLoadStop(new_shell->web_contents());
  return new_shell;
}

namespace {

class HttpRequestStallThrottle : public ResourceThrottle {
 public:
  // ResourceThrottle
  void WillStartRequest(bool* defer) override { *defer = true; }

  const char* GetNameForLogging() const override {
    return "HttpRequestStallThrottle";
  }
};

}  // namespace

NavigationStallDelegate::NavigationStallDelegate(const GURL& url) : url_(url) {}

void NavigationStallDelegate::RequestBeginning(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::AppCacheService* appcache_service,
    ResourceType resource_type,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (request->url() == url_)
    throttles->push_back(base::MakeUnique<HttpRequestStallThrottle>());
}

FileChooserDelegate::FileChooserDelegate(const base::FilePath& file)
      : file_(file), file_chosen_(false) {}

void FileChooserDelegate::RunFileChooser(RenderFrameHost* render_frame_host,
                                         const FileChooserParams& params) {
  // Send the selected file to the renderer process.
  FileChooserFileInfo file_info;
  file_info.file_path = file_;
  std::vector<FileChooserFileInfo> files;
  files.push_back(file_info);
  render_frame_host->FilesSelectedInChooser(files, FileChooserParams::Open);

  file_chosen_ = true;
  params_ = params;
}

FrameTestNavigationManager::FrameTestNavigationManager(
    int filtering_frame_tree_node_id,
    WebContents* web_contents,
    const GURL& url)
    : TestNavigationManager(web_contents, url),
      filtering_frame_tree_node_id_(filtering_frame_tree_node_id) {}

bool FrameTestNavigationManager::ShouldMonitorNavigation(
    NavigationHandle* handle) {
  return TestNavigationManager::ShouldMonitorNavigation(handle) &&
         handle->GetFrameTreeNodeId() == filtering_frame_tree_node_id_;
}

UrlCommitObserver::UrlCommitObserver(FrameTreeNode* frame_tree_node,
                                     const GURL& url)
    : content::WebContentsObserver(frame_tree_node->current_frame_host()
                                       ->delegate()
                                       ->GetAsWebContents()),
      frame_tree_node_id_(frame_tree_node->frame_tree_node_id()),
      url_(url) {
}

UrlCommitObserver::~UrlCommitObserver() {}

void UrlCommitObserver::Wait() {
  run_loop_.Run();
}

void UrlCommitObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      !navigation_handle->IsErrorPage() &&
      navigation_handle->GetURL() == url_ &&
      navigation_handle->GetFrameTreeNodeId() == frame_tree_node_id_) {
    run_loop_.Quit();
  }
}

FrameRectChangedMessageFilter::FrameRectChangedMessageFilter()
    : content::BrowserMessageFilter(FrameMsgStart),
      message_loop_runner_(new content::MessageLoopRunner),
      frame_rect_received_(false) {}

bool FrameRectChangedMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(FrameRectChangedMessageFilter, message)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FrameRectChanged, OnFrameRectChanged)
  IPC_END_MESSAGE_MAP()
  return false;
}

gfx::Rect FrameRectChangedMessageFilter::last_rect() const {
  return last_rect_;
}

void FrameRectChangedMessageFilter::Wait() {
  message_loop_runner_->Run();
}

void FrameRectChangedMessageFilter::Reset() {
  last_rect_ = gfx::Rect();
  message_loop_runner_ = new content::MessageLoopRunner;
  frame_rect_received_ = false;
}

FrameRectChangedMessageFilter::~FrameRectChangedMessageFilter() {}

void FrameRectChangedMessageFilter::OnFrameRectChanged(
    const gfx::Rect& rect,
    const viz::LocalSurfaceId& local_surface_id) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&FrameRectChangedMessageFilter::OnFrameRectChangedOnUI,
                     this, rect, local_surface_id));
}

void FrameRectChangedMessageFilter::OnFrameRectChangedOnUI(
    const gfx::Rect& rect,
    const viz::LocalSurfaceId& local_surface_id) {
  last_rect_ = rect;
  if (!frame_rect_received_) {
    frame_rect_received_ = true;
    message_loop_runner_->Quit();
  }
}

}  // namespace content
