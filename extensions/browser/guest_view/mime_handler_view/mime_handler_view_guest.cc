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
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension_messages.h"
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
    content::WebContents* owner_web_contents,
    int guest_instance_id) {
  return new MimeHandlerViewGuest(browser_context,
                                  owner_web_contents,
                                  guest_instance_id);
}

MimeHandlerViewGuest::MimeHandlerViewGuest(
    content::BrowserContext* browser_context,
    content::WebContents* owner_web_contents,
    int guest_instance_id)
    : GuestView<MimeHandlerViewGuest>(browser_context,
                                      owner_web_contents,
                                      guest_instance_id),
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

void MimeHandlerViewGuest::CreateWebContents(
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  std::string orig_mime_type;
  create_params.GetString(mime_handler_view::kMimeType, &orig_mime_type);
  DCHECK(!orig_mime_type.empty());

  std::string extension_src;
  create_params.GetString(mime_handler_view::kSrc, &extension_src);
  DCHECK(!extension_src.empty());
  std::string content_url_str;
  create_params.GetString(mime_handler_view::kContentUrl, &content_url_str);
  content_url_ = GURL(content_url_str);
  if (!content_url_.is_valid()) {
    content_url_ = GURL();
    callback.Run(nullptr);
    return;
  }

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

  // Use the mime handler extension's SiteInstance to create the guest so it
  // goes under the same process as the extension.
  ProcessManager* process_manager = ProcessManager::Get(browser_context());
  content::SiteInstance* guest_site_instance =
      process_manager->GetSiteInstanceForURL(
          Extension::GetBaseURLFromExtensionId(GetOwnerSiteURL().host()));

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

bool MimeHandlerViewGuest::ZoomPropagatesFromEmbedderToGuest() const {
  return false;
}

bool MimeHandlerViewGuest::Find(int request_id,
                                const base::string16& search_text,
                                const blink::WebFindOptions& options) {
  if (is_full_page_plugin()) {
    web_contents()->Find(request_id, search_text, options);
    return true;
  }
  return false;
}

void MimeHandlerViewGuest::ContentsZoomChange(bool zoom_in) {
  if (delegate_)
    delegate_->ChangeZoom(zoom_in);
}

bool MimeHandlerViewGuest::HandleContextMenu(
    const content::ContextMenuParams& params) {
  if (delegate_)
    return delegate_->HandleContextMenu(web_contents(), params);

  return false;
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

void MimeHandlerViewGuest::FindReply(content::WebContents* web_contents,
                                     int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->FindReply(embedder_web_contents(),
                                                    request_id,
                                                    number_of_matches,
                                                    selection_rect,
                                                    active_match_ordinal,
                                                    final_update);
}

bool MimeHandlerViewGuest::SaveFrame(const GURL& url,
                                     const content::Referrer& referrer) {
  if (!attached())
    return false;

  embedder_web_contents()->SaveFrame(content_url_, referrer);
  return true;
}

void MimeHandlerViewGuest::DocumentOnLoadCompletedInMainFrame() {
  embedder_web_contents()->Send(
      new ExtensionMsg_MimeHandlerViewGuestOnLoadCompleted(
          element_instance_id()));
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
