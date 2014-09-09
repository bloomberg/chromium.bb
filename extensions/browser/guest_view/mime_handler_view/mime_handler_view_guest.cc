// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/guest_view/guest_view_constants.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/common/feature_switch.h"
#include "extensions/strings/grit/extensions_strings.h"
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
    : GuestView<MimeHandlerViewGuest>(browser_context, guest_instance_id) {
}

MimeHandlerViewGuest::~MimeHandlerViewGuest() {
}

const char* MimeHandlerViewGuest::GetAPINamespace() const {
  return "mimeHandlerViewGuestInternal";
}

int MimeHandlerViewGuest::GetTaskPrefix() const {
  return IDS_EXTENSION_TASK_MANAGER_MIMEHANDLERVIEW_TAG_PREFIX;
}

void MimeHandlerViewGuest::CreateWebContents(
    const std::string& embedder_extension_id,
    int embedder_render_process_id,
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  std::string orig_mime_type;
  bool success =
      create_params.GetString(mime_handler_view::kMimeType, &orig_mime_type);
  DCHECK(success && !orig_mime_type.empty());
  std::string guest_site_str;
  // Note that we put a prefix "mime-" before the mime type so that this
  // can never collide with an extension ID.
  guest_site_str = base::StringPrintf(
      "%s://mime-%s", content::kGuestScheme, orig_mime_type.c_str());
  GURL guest_site(guest_site_str);

  // If we already have a mime handler view for the same mime type, we should
  // use the same SiteInstance so they go under same process.
  GuestViewManager* guest_view_manager =
      GuestViewManager::FromBrowserContext(browser_context());
  content::SiteInstance* guest_site_instance =
      guest_view_manager->GetGuestSiteInstance(guest_site);
  if (!guest_site_instance) {
    // Create the SiteInstance in a new BrowsingInstance, which will ensure
    // that guests from different render process are not allowed to send
    // messages to each other.
    guest_site_instance =
        content::SiteInstance::CreateForURL(browser_context(), guest_site);
  }
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
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
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

}  // namespace extensions
