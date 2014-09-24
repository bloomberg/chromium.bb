// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/guest_view/guest_view_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/url_util.h"

using content::WebContents;

namespace extensions {

// static
const char MimeHandlerViewGuest::Type[] = "mimehandler";

// static
GuestViewBase* MimeHandlerViewGuest::Create(
    content::BrowserContext* browser_context,
    int guest_instance_id) {
  if (!extensions::FeatureSwitch::mime_handler_view()->IsEnabled())
    return NULL;

  return new MimeHandlerViewGuest(browser_context, guest_instance_id);
}

MimeHandlerViewGuest::MimeHandlerViewGuest(
    content::BrowserContext* browser_context,
    int guest_instance_id)
    : GuestView<MimeHandlerViewGuest>(browser_context, guest_instance_id),
      delegate_(ExtensionsAPIClient::Get()->CreateMimeHandlerViewGuestDelegate(
          this)) {
}

MimeHandlerViewGuest::~MimeHandlerViewGuest() {
}

WindowController* MimeHandlerViewGuest::GetExtensionWindowController() const {
  return NULL;
}

WebContents* MimeHandlerViewGuest::GetAssociatedWebContents() const {
  return web_contents();
}

const char* MimeHandlerViewGuest::GetAPINamespace() const {
  return "mimeHandlerViewGuestInternal";
}

int MimeHandlerViewGuest::GetTaskPrefix() const {
  return IDS_EXTENSION_TASK_MANAGER_MIMEHANDLERVIEW_TAG_PREFIX;
}

// |embedder_extension_id| is empty for mime handler view.
void MimeHandlerViewGuest::CreateWebContents(
    const std::string& embedder_extension_id,
    int embedder_render_process_id,
    const GURL& embedder_site_url,
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  std::string orig_mime_type;
  create_params.GetString(mime_handler_view::kMimeType, &orig_mime_type);
  DCHECK(!orig_mime_type.empty());

  std::string extension_src;
  create_params.GetString(mime_handler_view::kSrc, &extension_src);
  DCHECK(!extension_src.empty());

  GURL mime_handler_extension_url(extension_src);
  if (!mime_handler_extension_url.is_valid()) {
    callback.Run(NULL);
    return;
  }

  const Extension* mime_handler_extension =
      // TODO(lazyboy): Do we need handle the case where the extension is
      // terminated (ExtensionRegistry::TERMINATED)?
      ExtensionRegistry::Get(browser_context())->enabled_extensions().GetByID(
          mime_handler_extension_url.host());
  if (!mime_handler_extension) {
    LOG(ERROR) << "Extension for mime_type not found, mime_type = "
               << orig_mime_type;
    callback.Run(NULL);
    return;
  }

  ProcessManager* process_manager =
      ExtensionSystem::Get(browser_context())->process_manager();
  DCHECK(process_manager);

  // Use the mime handler extension's SiteInstance to create the guest so it
  // goes under the same process as the extension.
  content::SiteInstance* guest_site_instance =
      process_manager->GetSiteInstanceForURL(
          Extension::GetBaseURLFromExtensionId(embedder_extension_id));

  WebContents::CreateParams params(browser_context(), guest_site_instance);
  params.guest_delegate = this;
  callback.Run(WebContents::Create(params));
}

void MimeHandlerViewGuest::DidAttachToEmbedder() {
  std::string src;
  bool success = attach_params()->GetString(mime_handler_view::kSrc, &src);
  DCHECK(success && !src.empty());
  web_contents()->GetController().LoadURL(
      GURL(src),
      content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

void MimeHandlerViewGuest::DidInitialize() {
  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(browser_context(), this));
  if (delegate_)
    delegate_->AttachHelpers();
}

void MimeHandlerViewGuest::ContentsZoomChange(bool zoom_in) {
  if (delegate_)
    delegate_->ChangeZoom(zoom_in);
}

void MimeHandlerViewGuest::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (!attached())
    return;

  // Send the keyboard events back to the embedder to reprocess them.
  // TODO(fsamuel): This introduces the possibility of out-of-order keyboard
  // events because the guest may be arbitrarily delayed when responding to
  // keyboard events. In that time, the embedder may have received and processed
  // additional key events. This needs to be fixed as soon as possible.
  // See http://crbug.com/229882.
  embedder_web_contents()->GetDelegate()->HandleKeyboardEvent(web_contents(),
                                                              event);
}

bool MimeHandlerViewGuest::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MimeHandlerViewGuest, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MimeHandlerViewGuest::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  if (extension_function_dispatcher_) {
    extension_function_dispatcher_->Dispatch(
        params, web_contents()->GetRenderViewHost());
  }
}

}  // namespace extensions
