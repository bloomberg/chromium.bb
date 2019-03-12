// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/skia/include/core/SkColor.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

using content::BrowserThread;
using content::RenderFrameHost;
using content::SiteInstance;

namespace extensions {

namespace {

// TODO(ekaramad): Make this a proper resource (https://crbug.com/659750).
// TODO(ekaramad): Should we use an <embed>? Verify if this causes issues with
// post messaging support for PDF viewer (https://crbug.com/659750).
const char kFullPageMimeHandlerViewHTML[] =
    "<!doctype html><html><body style='height: 100%%; width: 100%%; overflow: "
    "hidden; margin:0px; background-color: rgb(%d, %d, %d);'><iframe "
    "style='position:absolute; left: 0; top: 0;'width='100%%' height='100%%'"
    "></iframe></body></html>";
const uint32_t kFullPageMimeHandlerViewDataPipeSize = 256U;

SkColor GetBackgroundColorStringForMimeType(const GURL& url,
                                            const std::string& mime_type) {
#if BUILDFLAG(ENABLE_PLUGINS)
  std::vector<content::WebPluginInfo> web_plugin_info_array;
  std::vector<std::string> unused_actual_mime_types;
  content::PluginService::GetInstance()->GetPluginInfoArray(
      url, mime_type, true, &web_plugin_info_array, &unused_actual_mime_types);
  for (const auto& info : web_plugin_info_array) {
    for (const auto& mime_info : info.mime_types) {
      if (mime_type == mime_info.mime_type)
        return info.background_color;
    }
  }
#endif
  return content::WebPluginInfo::kDefaultBackgroundColor;
}

using ProcessIdToHelperMap =
    base::flat_map<int32_t, std::unique_ptr<MimeHandlerViewAttachHelper>>;
ProcessIdToHelperMap* GetProcessIdToHelperMap() {
  static base::NoDestructor<ProcessIdToHelperMap> instance;
  return instance.get();
}

// Helper class which tracks navigations related to |frame_tree_node_id| and
// looks for a same-SiteInstance child RenderFrameHost created which has
// |frame_tree_node_id| as its parent's FrameTreeNode Id.
class PendingFullPageNavigation : public content::WebContentsObserver {
 public:
  PendingFullPageNavigation(int32_t frame_tree_node_id,
                            const GURL& resource_url,
                            const std::string& mime_type,
                            const std::string& stream_id);
  ~PendingFullPageNavigation() override;

  // content::WebContentsObserver overrides.
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

 private:
  const int32_t frame_tree_node_id_;
  const GURL resource_url_;
  const std::string mime_type_;
  const std::string stream_id_;
};

using PendingNavigationMap =
    base::flat_map<int32_t, std::unique_ptr<PendingFullPageNavigation>>;

PendingNavigationMap* GetPendingFullPageNavigationsMap() {
  static base::NoDestructor<PendingNavigationMap> instance;
  return instance.get();
}

// Returns true if the mime type is relevant to MimeHandlerView.
bool IsRelevantMimeType(const std::string& mime_type) {
  // TODO(ekaramad): Figure out what other relevant mime-types are, e.g., for
  // quick office.
  return mime_type == "application/pdf" || mime_type == "text/pdf";
}

PendingFullPageNavigation::PendingFullPageNavigation(
    int32_t frame_tree_node_id,
    const GURL& resource_url,
    const std::string& mime_type,
    const std::string& stream_id)
    : content::WebContentsObserver(
          content::WebContents::FromFrameTreeNodeId(frame_tree_node_id)),
      frame_tree_node_id_(frame_tree_node_id),
      resource_url_(resource_url),
      mime_type_(mime_type),
      stream_id_(stream_id) {}

PendingFullPageNavigation::~PendingFullPageNavigation() {}

void PendingFullPageNavigation::DidStartNavigation(
    content::NavigationHandle* handle) {
  // This observer is created after the observed |frame_tree_node_id_| started
  // its navigation to the |resource_url|. If any new navigations start then
  // we should stop now and do not create a MHVG.
  if (handle->GetFrameTreeNodeId() == frame_tree_node_id_)
    GetPendingFullPageNavigationsMap()->erase(frame_tree_node_id_);
}

void PendingFullPageNavigation::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetParent() &&
      render_frame_host->GetParent()->GetFrameTreeNodeId() ==
          frame_tree_node_id_ &&
      render_frame_host->GetParent()->GetLastCommittedURL() == resource_url_) {
    // This suggests that a same-origin child frame is created under the
    // RFH associated with |frame_tree_node_id_|. This suggests that the HTML
    // string is loaded in the observed frame's document and now the renderer
    // can initiate the MimeHandlerViewFrameContainer creation process.
    mojom::MimeHandlerViewContainerManagerPtr container_manager;
    render_frame_host->GetParent()->GetRemoteInterfaces()->GetInterface(
        &container_manager);
    container_manager->CreateFrameContainer(resource_url_, mime_type_,
                                            stream_id_);
    GetPendingFullPageNavigationsMap()->erase(frame_tree_node_id_);
  }
}

void PendingFullPageNavigation::WebContentsDestroyed() {
  GetPendingFullPageNavigationsMap()->erase(frame_tree_node_id_);
}

}  // namespace


// static
MimeHandlerViewAttachHelper* MimeHandlerViewAttachHelper::Get(
    int32_t render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& map = *GetProcessIdToHelperMap();
  if (!base::ContainsKey(map, render_process_id)) {
    auto* process_host = content::RenderProcessHost::FromID(render_process_id);
    if (!process_host)
      return nullptr;
    map[render_process_id] = base::WrapUnique<MimeHandlerViewAttachHelper>(
        new MimeHandlerViewAttachHelper(process_host));
  }
  return map[render_process_id].get();
}

// static
void MimeHandlerViewAttachHelper::OverrideBodyForInterceptedResponse(
    int32_t navigating_frame_tree_node_id,
    const GURL& resource_url,
    const std::string& mime_type,
    const std::string& stream_id,
    std::string* payload,
    uint32_t* data_pipe_size) {
  if (!content::MimeHandlerViewMode::UsesCrossProcessFrame())
    return;
  if (!IsRelevantMimeType(mime_type))
    return;
  auto color = GetBackgroundColorStringForMimeType(resource_url, mime_type);
  auto html_str =
      base::StringPrintf(kFullPageMimeHandlerViewHTML, SkColorGetR(color),
                         SkColorGetG(color), SkColorGetB(color));
  payload->assign(html_str);
  *data_pipe_size = kFullPageMimeHandlerViewDataPipeSize;
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(CreateFullPageMimeHandlerView,
                                          navigating_frame_tree_node_id,
                                          resource_url, mime_type, stream_id));
}

void MimeHandlerViewAttachHelper::RenderProcessHostDestroyed(
    content::RenderProcessHost* render_process_host) {
  if (render_process_host != render_process_host_)
    return;
  render_process_host->RemoveObserver(this);
  GetProcessIdToHelperMap()->erase(render_process_host_->GetID());
}

void MimeHandlerViewAttachHelper::AttachToOuterWebContents(
    MimeHandlerViewGuest* guest_view,
    int32_t embedder_render_process_id,
    int32_t plugin_frame_routing_id,
    int32_t element_instance_id,
    bool is_full_page_plugin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* plugin_rfh = RenderFrameHost::FromID(embedder_render_process_id,
                                             plugin_frame_routing_id);
  if (!plugin_rfh) {
    // The plugin element has a proxy instead.
    plugin_rfh = RenderFrameHost::FromPlaceholderId(embedder_render_process_id,
                                                    plugin_frame_routing_id);
  }
  if (!plugin_rfh) {
    // This should only happen if the original plugin frame was cross-process
    // and a concurrent navigation in its process won the race and ended up
    // destroying the proxy whose routing ID was sent here by the
    // MimeHandlerViewFrameContainer. We should ask the embedder to retry
    // creating the guest.
    mojom::MimeHandlerViewContainerManagerPtr container_manager;
    guest_view->GetEmbedderFrame()->GetRemoteInterfaces()->GetInterface(
        &container_manager);
    container_manager->RetryCreatingMimeHandlerViewGuest(element_instance_id);
    guest_view->Destroy(true);
    return;
  }

  pending_guests_[element_instance_id] = guest_view;
  plugin_rfh->PrepareForInnerWebContentsAttach(base::BindOnce(
      &MimeHandlerViewAttachHelper::ResumeAttachOrDestroy,
      weak_factory_.GetWeakPtr(), element_instance_id, is_full_page_plugin));
}

// static
void MimeHandlerViewAttachHelper::CreateFullPageMimeHandlerView(
    int32_t frame_tree_node_id,
    const GURL& resource_url,
    const std::string& mime_type,
    const std::string& stream_id) {
  auto& map = *GetPendingFullPageNavigationsMap();
  map[frame_tree_node_id] = std::make_unique<PendingFullPageNavigation>(
      frame_tree_node_id, resource_url, mime_type, stream_id);
}

MimeHandlerViewAttachHelper::MimeHandlerViewAttachHelper(
    content::RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host), weak_factory_(this) {
  render_process_host->AddObserver(this);
}

MimeHandlerViewAttachHelper::~MimeHandlerViewAttachHelper() {}

void MimeHandlerViewAttachHelper::ResumeAttachOrDestroy(
    int32_t element_instance_id,
    bool is_full_page_plugin,
    content::RenderFrameHost* plugin_rfh) {
  DCHECK(!plugin_rfh || (plugin_rfh->GetProcess() == render_process_host_));
  auto* guest_view = pending_guests_[element_instance_id];
  pending_guests_.erase(element_instance_id);
  if (!plugin_rfh) {
    mojom::MimeHandlerViewContainerManagerPtr container_manager;
    guest_view->GetEmbedderFrame()->GetRemoteInterfaces()->GetInterface(
        &container_manager);
    container_manager->DestroyFrameContainer(element_instance_id);
    guest_view->Destroy(true);
    return;
  }
  guest_view->AttachToOuterWebContentsFrame(plugin_rfh, element_instance_id,
                                            is_full_page_plugin);
}
}  // namespace extensions
