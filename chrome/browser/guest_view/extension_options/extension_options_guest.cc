// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/extension_options/extension_options_guest.h"

#include "base/values.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/guest_view/extension_options/extension_options_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/extension_options_internal.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message_macros.h"

using content::WebContents;
using namespace extensions::api;

// static
const char ExtensionOptionsGuest::Type[] = "extensionoptions";

ExtensionOptionsGuest::ExtensionOptionsGuest(
    content::BrowserContext* browser_context,
    int guest_instance_id)
    : GuestView<ExtensionOptionsGuest>(browser_context, guest_instance_id) {
}

ExtensionOptionsGuest::~ExtensionOptionsGuest() {
}

bool ExtensionOptionsGuest::CanEmbedderUseGuestView(
    const std::string& embedder_extension_id) {
  const extensions::Extension* embedder_extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->enabled_extensions()
          .GetByID(embedder_extension_id);
  if (!embedder_extension)
    return false;
  return embedder_extension->permissions_data()->HasAPIPermission(
      extensions::APIPermission::kEmbeddedExtensionOptions);
}

// static
GuestViewBase* ExtensionOptionsGuest::Create(
    content::BrowserContext* browser_context,
    int guest_instance_id) {
  if (!extensions::FeatureSwitch::embedded_extension_options()->IsEnabled()) {
    return NULL;
  }
  return new ExtensionOptionsGuest(browser_context, guest_instance_id);
}

void ExtensionOptionsGuest::CreateWebContents(
    const std::string& embedder_extension_id,
    int embedder_render_process_id,
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  // Get the extension's base URL.
  std::string extension_id;
  create_params.GetString(extensionoptions::kExtensionId, &extension_id);
  if (extension_id.empty()) {
    callback.Run(NULL);
    return;
  }
  DCHECK(extensions::Extension::IdIsValid(extension_id));

  GURL extension_url =
      extensions::Extension::GetBaseURLFromExtensionId(extension_id);
  if (!extension_url.is_valid()) {
    callback.Run(NULL);
    return;
  }

  // Get the options page URL for later use.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context());
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  options_page_ = extensions::ManifestURL::GetOptionsPage(extension);
  if (!options_page_.is_valid()) {
    callback.Run(NULL);
    return;
  }

  // Create a WebContents using the extension URL. The options page's
  // WebContents should live in the same process as its parent extension's
  // WebContents, so we can use |extension_url| for creating the SiteInstance.
  content::SiteInstance* options_site_instance =
      content::SiteInstance::CreateForURL(browser_context(), extension_url);
  WebContents::CreateParams params(browser_context(), options_site_instance);
  params.guest_delegate = this;
  callback.Run(WebContents::Create(params));
}

void ExtensionOptionsGuest::DidAttachToEmbedder() {
  SetUpAutoSize();
  guest_web_contents()->GetController().LoadURL(options_page_,
                                                content::Referrer(),
                                                content::PAGE_TRANSITION_LINK,
                                                std::string());
}

void ExtensionOptionsGuest::DidInitialize() {
  extension_function_dispatcher_.reset(
      new extensions::ExtensionFunctionDispatcher(browser_context(), this));
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      guest_web_contents());
}

void ExtensionOptionsGuest::DidStopLoading() {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToEmbedder(new GuestViewBase::Event(
      extensions::api::extension_options_internal::OnLoad::kEventName,
      args.Pass()));
}

content::WebContents* ExtensionOptionsGuest::GetAssociatedWebContents() const {
  return guest_web_contents();
}

bool ExtensionOptionsGuest::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionOptionsGuest, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionOptionsGuest::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(
      params, guest_web_contents()->GetRenderViewHost());
}

void ExtensionOptionsGuest::GuestSizeChangedDueToAutoSize(
    const gfx::Size& old_size,
    const gfx::Size& new_size) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(extensionoptions::kWidth, new_size.width());
  args->SetInteger(extensionoptions::kHeight, new_size.height());
  DispatchEventToEmbedder(new GuestViewBase::Event(
      extension_options_internal::OnSizeChanged::kEventName, args.Pass()));
}

bool ExtensionOptionsGuest::IsAutoSizeSupported() const {
  return true;
}

void ExtensionOptionsGuest::SetUpAutoSize() {
  // Read the autosize parameters passed in from the embedder.
  bool auto_size_enabled;
  extra_params()->GetBoolean(extensionoptions::kAttributeAutoSize,
                             &auto_size_enabled);

  int max_height = 0;
  int max_width = 0;
  extra_params()->GetInteger(extensionoptions::kAttributeMaxHeight,
                             &max_height);
  extra_params()->GetInteger(extensionoptions::kAttributeMaxWidth, &max_width);

  int min_height = 0;
  int min_width = 0;
  extra_params()->GetInteger(extensionoptions::kAttributeMinHeight,
                             &min_height);
  extra_params()->GetInteger(extensionoptions::kAttributeMinWidth, &min_width);

  // Call SetAutoSize to apply all the appropriate validation and clipping of
  // values.
  SetAutoSize(
      true, gfx::Size(min_width, min_height), gfx::Size(max_width, max_height));
}
