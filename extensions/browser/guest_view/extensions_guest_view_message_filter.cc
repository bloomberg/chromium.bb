// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extensions_guest_view_message_filter.h"

#include "base/guid.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/stream_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserContext;
using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;
using guest_view::GuestViewManager;
using guest_view::GuestViewManagerDelegate;
using guest_view::GuestViewMessageFilter;

namespace extensions {

namespace {

// TODO(ekaramad): Remove this once MimeHandlerViewGuest has fully migrated to
// using cross-process-frames.
// Returns true if |child_routing_id| corresponds to a frame which is a direct
// child of |parent_rfh|.
bool AreRoutingIDsConsistent(content::RenderFrameHost* parent_rfh,
                             int32_t child_routing_id) {
  const bool uses_cross_process_frame =
      content::MimeHandlerViewMode::UsesCrossProcessFrame();
  const bool is_child_routing_id_none = (child_routing_id == MSG_ROUTING_NONE);

  // For cross-process-frame MimeHandlerView, |child_routing_id| cannot be none.
  bool should_shutdown_process =
      (is_child_routing_id_none == uses_cross_process_frame);

  if (!should_shutdown_process && uses_cross_process_frame) {
    // The |child_routing_id| is the routing ID of either a RenderFrame or a
    // proxy in the |parent_rfh|. Therefore, to get the associated RFH we need
    // to go through the FTN first.
    int32_t child_ftn_id =
        content::RenderFrameHost::GetFrameTreeNodeIdForRoutingId(
            parent_rfh->GetProcess()->GetID(), child_routing_id);
    // The |child_rfh| is not really used; it is retrieved to verify whether or
    // not what the renderer process says makes any sense.
    auto* child_rfh = content::WebContents::FromRenderFrameHost(parent_rfh)
                          ->UnsafeFindFrameByFrameTreeNodeId(child_ftn_id);
    should_shutdown_process =
        child_rfh && (child_rfh->GetParent() != parent_rfh);
  }
  return !should_shutdown_process;
}

}  // namespace
const uint32_t ExtensionsGuestViewMessageFilter::kFilteredMessageClasses[] = {
    GuestViewMsgStart, ExtensionsGuestViewMsgStart};

ExtensionsGuestViewMessageFilter::ExtensionsGuestViewMessageFilter(
    int render_process_id,
    BrowserContext* context)
    : GuestViewMessageFilter(kFilteredMessageClasses,
                             arraysize(kFilteredMessageClasses),
                             render_process_id,
                             context),
      content::BrowserAssociatedInterface<mojom::GuestView>(this, this) {}

ExtensionsGuestViewMessageFilter::~ExtensionsGuestViewMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void ExtensionsGuestViewMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionsGuestViewHostMsg_ResizeGuest::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      GuestViewMessageFilter::OverrideThreadForMessage(message, thread);
  }
}

bool ExtensionsGuestViewMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionsGuestViewMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionsGuestViewHostMsg_CanExecuteContentScriptSync,
                        OnCanExecuteContentScript)
    IPC_MESSAGE_HANDLER(ExtensionsGuestViewHostMsg_ResizeGuest, OnResizeGuest)
    IPC_MESSAGE_UNHANDLED(
        handled = GuestViewMessageFilter::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()
  return handled;
}

GuestViewManager* ExtensionsGuestViewMessageFilter::
    GetOrCreateGuestViewManager() {
  auto* manager = GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager) {
    manager = GuestViewManager::CreateWithDelegate(
        browser_context_,
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
            browser_context_));
  }
  return manager;
}

void ExtensionsGuestViewMessageFilter::OnCanExecuteContentScript(
    int render_view_id,
    int script_id,
    bool* allowed) {
  WebViewRendererState::WebViewInfo info;
  WebViewRendererState::GetInstance()->GetInfo(render_process_id_,
                                               render_view_id, &info);

  *allowed =
      info.content_script_ids.find(script_id) != info.content_script_ids.end();
}

void ExtensionsGuestViewMessageFilter::CreateMimeHandlerViewGuest(
    int32_t render_frame_id,
    const std::string& view_id,
    int32_t element_instance_id,
    const gfx::Size& element_size,
    mime_handler::BeforeUnloadControlPtr before_unload_control,
    int32_t plugin_frame_routing_id) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&ExtensionsGuestViewMessageFilter::
                         CreateMimeHandlerViewGuestOnUIThread,
                     this, render_frame_id, view_id, element_instance_id,
                     element_size, before_unload_control.PassInterface(),
                     plugin_frame_routing_id, false));
}

void ExtensionsGuestViewMessageFilter::CreateMimeHandlerViewGuestOnUIThread(
    int render_frame_id,
    const std::string& view_id,
    int element_instance_id,
    const gfx::Size& element_size,
    mime_handler::BeforeUnloadControlPtrInfo before_unload_control,
    int32_t plugin_frame_routing_id,
    bool is_full_page_plugin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* manager = GetOrCreateGuestViewManager();

  auto* rfh = RenderFrameHost::FromID(render_process_id_, render_frame_id);
  auto* embedder_web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!embedder_web_contents)
    return;

  if (!AreRoutingIDsConsistent(rfh, plugin_frame_routing_id)) {
    bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                    bad_message::MHVG_INVALID_PLUGIN_FRAME_ID);
    return;
  }

  GuestViewManager::WebContentsCreatedCallback callback = base::BindOnce(
      &ExtensionsGuestViewMessageFilter::MimeHandlerViewGuestCreatedCallback,
      this, element_instance_id, render_process_id_, render_frame_id,
      plugin_frame_routing_id, element_size, std::move(before_unload_control),
      is_full_page_plugin);

  base::DictionaryValue create_params;
  create_params.SetString(mime_handler_view::kViewId, view_id);
  create_params.SetInteger(guest_view::kElementWidth, element_size.width());
  create_params.SetInteger(guest_view::kElementHeight, element_size.height());
  manager->CreateGuest(MimeHandlerViewGuest::Type, embedder_web_contents,
                       create_params, std::move(callback));
}

void ExtensionsGuestViewMessageFilter::OnResizeGuest(
    int render_frame_id,
    int element_instance_id,
    const gfx::Size& new_size) {
  // We should have a GuestViewManager at this point. If we don't then the
  // embedder is misbehaving.
  auto* manager = GetGuestViewManagerOrKill();
  if (!manager)
    return;

  auto* guest_web_contents =
      manager->GetGuestByInstanceID(render_process_id_, element_instance_id);
  auto* mhvg = MimeHandlerViewGuest::FromWebContents(guest_web_contents);
  if (!mhvg)
    return;

  guest_view::SetSizeParams set_size_params;
  set_size_params.enable_auto_size.reset(new bool(false));
  set_size_params.normal_size.reset(new gfx::Size(new_size));
  mhvg->SetSize(set_size_params);
}

void ExtensionsGuestViewMessageFilter::CreateEmbeddedMimeHandlerViewGuest(
    int32_t render_frame_id,
    int32_t tab_id,
    const GURL& original_url,
    int32_t element_instance_id,
    const gfx::Size& element_size,
    content::mojom::TransferrableURLLoaderPtr transferrable_url_loader,
    int32_t plugin_frame_routing_id) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ExtensionsGuestViewMessageFilter::
                           CreateEmbeddedMimeHandlerViewGuest,
                       this, render_frame_id, tab_id, original_url,
                       element_instance_id, element_size,
                       base::Passed(&transferrable_url_loader),
                       plugin_frame_routing_id));
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id_,
                                           render_frame_id));
  if (!web_contents)
    return;

  auto* browser_context = web_contents->GetBrowserContext();
  std::string extension_id = transferrable_url_loader->url.host();
  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension)
    return;

  MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (!handler || !handler->HasPlugin()) {
    NOTREACHED();
    return;
  }

  GURL handler_url(Extension::GetBaseURLFromExtensionId(extension_id).spec() +
                   handler->handler_url());

  std::string view_id = base::GenerateGUID();
  std::unique_ptr<StreamContainer> stream_container(new StreamContainer(
      nullptr, tab_id, true /* embedded */, handler_url, extension_id,
      std::move(transferrable_url_loader), original_url));
  MimeHandlerStreamManager::Get(browser_context)
      ->AddStream(view_id, std::move(stream_container),
                  -1 /* frame_tree_node_id*/, render_process_id_,
                  render_frame_id);

  CreateMimeHandlerViewGuestOnUIThread(render_frame_id, view_id,
                                       element_instance_id, element_size,
                                       nullptr, plugin_frame_routing_id, false);
}

void ExtensionsGuestViewMessageFilter::MimeHandlerViewGuestCreatedCallback(
    int element_instance_id,
    int embedder_render_process_id,
    int embedder_render_frame_id,
    int32_t plugin_frame_routing_id,
    const gfx::Size& element_size,
    mime_handler::BeforeUnloadControlPtrInfo before_unload_control,
    bool is_full_page_plugin,
    WebContents* web_contents) {
  auto* guest_view = MimeHandlerViewGuest::FromWebContents(web_contents);
  if (!guest_view)
    return;

  guest_view->SetBeforeUnloadController(std::move(before_unload_control));
  int guest_instance_id = guest_view->guest_instance_id();
  auto* rfh = RenderFrameHost::FromID(embedder_render_process_id,
                                      embedder_render_frame_id);
  if (!rfh)
    return;

  guest_view->SetEmbedderFrame(embedder_render_process_id,
                               embedder_render_frame_id);

  base::DictionaryValue attach_params;
  attach_params.SetInteger(guest_view::kElementWidth, element_size.width());
  attach_params.SetInteger(guest_view::kElementHeight, element_size.height());
  auto uses_cross_process_frame =
      content::MimeHandlerViewMode::UsesCrossProcessFrame();
  if (uses_cross_process_frame) {
    auto* plugin_rfh = content::RenderFrameHost::FromID(
        embedder_render_process_id, plugin_frame_routing_id);
    if (!plugin_rfh) {
      // TODO(ekaramad): This happens when the plugin element contains a remote
      // frame. Introduce this edge case to content/ layer.
      return;
    }
    AttachToEmbedderFrame(plugin_frame_routing_id, element_instance_id,
                          guest_instance_id, attach_params,
                          is_full_page_plugin);
    return;
  }
  auto* manager = GuestViewManager::FromBrowserContext(browser_context_);
  CHECK(manager);
  manager->AttachGuest(embedder_render_process_id, element_instance_id,
                       guest_instance_id, attach_params);
  rfh->Send(new ExtensionsGuestViewMsg_CreateMimeHandlerViewGuestACK(
      element_instance_id));
}

}  // namespace extensions
