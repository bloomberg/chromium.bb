// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/extensions_api_client.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/web_request/web_request_event_router_delegate.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"

namespace extensions {
class AppViewGuestDelegate;

namespace {
ExtensionsAPIClient* g_instance = NULL;
}  // namespace

ExtensionsAPIClient::ExtensionsAPIClient() { g_instance = this; }

ExtensionsAPIClient::~ExtensionsAPIClient() { g_instance = NULL; }

// static
ExtensionsAPIClient* ExtensionsAPIClient::Get() { return g_instance; }

void ExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<ValueStoreFactory>& factory,
    const scoped_refptr<base::ObserverListThreadSafe<SettingsObserver>>&
        observers,
    std::map<settings_namespace::Namespace, ValueStoreCache*>* caches) {}

void ExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
}

AppViewGuestDelegate* ExtensionsAPIClient::CreateAppViewGuestDelegate() const {
  return NULL;
}

ExtensionOptionsGuestDelegate*
ExtensionsAPIClient::CreateExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest) const {
  return NULL;
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
ExtensionsAPIClient::CreateGuestViewManagerDelegate(
    content::BrowserContext* context) const {
  return base::MakeUnique<ExtensionsGuestViewManagerDelegate>(context);
}

std::unique_ptr<MimeHandlerViewGuestDelegate>
ExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return std::unique_ptr<MimeHandlerViewGuestDelegate>();
}

WebViewGuestDelegate* ExtensionsAPIClient::CreateWebViewGuestDelegate(
    WebViewGuest* web_view_guest) const {
  return NULL;
}

WebViewPermissionHelperDelegate* ExtensionsAPIClient::
    CreateWebViewPermissionHelperDelegate(
        WebViewPermissionHelper* web_view_permission_helper) const {
  return new WebViewPermissionHelperDelegate(web_view_permission_helper);
}

std::unique_ptr<WebRequestEventRouterDelegate>
ExtensionsAPIClient::CreateWebRequestEventRouterDelegate() const {
  return nullptr;
}

scoped_refptr<ContentRulesRegistry>
ExtensionsAPIClient::CreateContentRulesRegistry(
    content::BrowserContext* browser_context,
    RulesCacheDelegate* cache_delegate) const {
  return scoped_refptr<ContentRulesRegistry>();
}

std::unique_ptr<DevicePermissionsPrompt>
ExtensionsAPIClient::CreateDevicePermissionsPrompt(
    content::WebContents* web_contents) const {
  return nullptr;
}

std::unique_ptr<VirtualKeyboardDelegate>
ExtensionsAPIClient::CreateVirtualKeyboardDelegate() const {
  return nullptr;
}

ManagementAPIDelegate* ExtensionsAPIClient::CreateManagementAPIDelegate()
    const {
  return nullptr;
}

MetricsPrivateDelegate* ExtensionsAPIClient::GetMetricsPrivateDelegate() {
  return nullptr;
}

NetworkingCastPrivateDelegate*
ExtensionsAPIClient::GetNetworkingCastPrivateDelegate() {
  return nullptr;
}

#if defined(OS_CHROMEOS)
NonNativeFileSystemDelegate*
ExtensionsAPIClient::GetNonNativeFileSystemDelegate() {
  return nullptr;
}

void ExtensionsAPIClient::SaveImageDataToClipboard(
    const std::vector<char>& image_data,
    api::clipboard::ImageType type,
    AdditionalDataItemList additional_items,
    const base::Closure& success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {}
#endif

}  // namespace extensions
