// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/data_use_measurement/data_use_web_contents_observer.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"
#include "chrome/browser/extensions/api/declarative_content/default_content_predicate_evaluators.h"
#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate.h"
#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"
#include "chrome/browser/extensions/api/metrics_private/chrome_metrics_private_delegate.h"
#include "chrome/browser/extensions/api/networking_cast_private/chrome_networking_cast_private_delegate.h"
#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"
#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"
#include "chrome/browser/extensions/api/web_request/chrome_extension_web_request_event_router_delegate.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/guest_view/app_view/chrome_app_view_guest_delegate.h"
#include "chrome/browser/guest_view/chrome_guest_view_manager_delegate.h"
#include "chrome/browser/guest_view/extension_options/chrome_extension_options_guest_delegate.h"
#include "chrome/browser/guest_view/mime_handler_view/chrome_mime_handler_view_guest_delegate.h"
#include "chrome/browser/guest_view/web_view/chrome_web_view_guest_delegate.h"
#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "printing/features/features.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/file_handlers/non_native_file_system_delegate_chromeos.h"
#include "chrome/browser/extensions/api/virtual_keyboard_private/chrome_virtual_keyboard_delegate.h"
#include "chrome/browser/extensions/clipboard_extension_helper_chromeos.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

namespace extensions {

ChromeExtensionsAPIClient::ChromeExtensionsAPIClient() {}

ChromeExtensionsAPIClient::~ChromeExtensionsAPIClient() {}

void ChromeExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<ValueStoreFactory>& factory,
    const scoped_refptr<base::ObserverListThreadSafe<SettingsObserver>>&
        observers,
    std::map<settings_namespace::Namespace, ValueStoreCache*>* caches) {
  // Add support for chrome.storage.sync.
  (*caches)[settings_namespace::SYNC] =
      new SyncValueStoreCache(factory, observers, context->GetPath());

  // Add support for chrome.storage.managed.
  (*caches)[settings_namespace::MANAGED] =
      new ManagedValueStoreCache(context, factory, observers);
}

void ChromeExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
#if BUILDFLAG(ENABLE_PRINTING)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::PrintViewManager::CreateForWebContents(web_contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(web_contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents, std::unique_ptr<pdf::PDFWebContentsHelperClient>(
                        new ChromePDFWebContentsHelperClient()));

  data_use_measurement::DataUseWebContentsObserver::CreateForWebContents(
      web_contents);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
}

AppViewGuestDelegate* ChromeExtensionsAPIClient::CreateAppViewGuestDelegate()
    const {
  return new ChromeAppViewGuestDelegate();
}

ExtensionOptionsGuestDelegate*
ChromeExtensionsAPIClient::CreateExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest) const {
  return new ChromeExtensionOptionsGuestDelegate(guest);
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
ChromeExtensionsAPIClient::CreateGuestViewManagerDelegate(
    content::BrowserContext* context) const {
  return base::MakeUnique<ChromeGuestViewManagerDelegate>(context);
}

std::unique_ptr<MimeHandlerViewGuestDelegate>
ChromeExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return base::MakeUnique<ChromeMimeHandlerViewGuestDelegate>();
}

WebViewGuestDelegate* ChromeExtensionsAPIClient::CreateWebViewGuestDelegate(
    WebViewGuest* web_view_guest) const {
  return new ChromeWebViewGuestDelegate(web_view_guest);
}

WebViewPermissionHelperDelegate* ChromeExtensionsAPIClient::
    CreateWebViewPermissionHelperDelegate(
        WebViewPermissionHelper* web_view_permission_helper) const {
  return new ChromeWebViewPermissionHelperDelegate(web_view_permission_helper);
}

std::unique_ptr<WebRequestEventRouterDelegate>
ChromeExtensionsAPIClient::CreateWebRequestEventRouterDelegate() const {
  return base::MakeUnique<ChromeExtensionWebRequestEventRouterDelegate>();
}

scoped_refptr<ContentRulesRegistry>
ChromeExtensionsAPIClient::CreateContentRulesRegistry(
    content::BrowserContext* browser_context,
    RulesCacheDelegate* cache_delegate) const {
  return scoped_refptr<ContentRulesRegistry>(
      new ChromeContentRulesRegistry(
          browser_context,
          cache_delegate,
          base::Bind(&CreateDefaultContentPredicateEvaluators,
                     base::Unretained(browser_context))));
}

std::unique_ptr<DevicePermissionsPrompt>
ChromeExtensionsAPIClient::CreateDevicePermissionsPrompt(
    content::WebContents* web_contents) const {
  return base::MakeUnique<ChromeDevicePermissionsPrompt>(web_contents);
}

std::unique_ptr<VirtualKeyboardDelegate>
ChromeExtensionsAPIClient::CreateVirtualKeyboardDelegate() const {
#if defined(OS_CHROMEOS)
  return base::MakeUnique<ChromeVirtualKeyboardDelegate>();
#else
  return nullptr;
#endif
}

ManagementAPIDelegate* ChromeExtensionsAPIClient::CreateManagementAPIDelegate()
    const {
  return new ChromeManagementAPIDelegate;
}

MetricsPrivateDelegate* ChromeExtensionsAPIClient::GetMetricsPrivateDelegate() {
  if (!metrics_private_delegate_)
    metrics_private_delegate_.reset(new ChromeMetricsPrivateDelegate());
  return metrics_private_delegate_.get();
}

NetworkingCastPrivateDelegate*
ChromeExtensionsAPIClient::GetNetworkingCastPrivateDelegate() {
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_MACOSX)
  if (!networking_cast_private_delegate_)
    networking_cast_private_delegate_ =
        ChromeNetworkingCastPrivateDelegate::Create();
#endif
  return networking_cast_private_delegate_.get();
}

FileSystemDelegate* ChromeExtensionsAPIClient::GetFileSystemDelegate() {
  if (!file_system_delegate_)
    file_system_delegate_ = base::MakeUnique<ChromeFileSystemDelegate>();
  return file_system_delegate_.get();
}

#if defined(OS_CHROMEOS)
NonNativeFileSystemDelegate*
ChromeExtensionsAPIClient::GetNonNativeFileSystemDelegate() {
  if (!non_native_file_system_delegate_) {
    non_native_file_system_delegate_ =
        base::MakeUnique<NonNativeFileSystemDelegateChromeOS>();
  }
  return non_native_file_system_delegate_.get();
}

void ChromeExtensionsAPIClient::SaveImageDataToClipboard(
    const std::vector<char>& image_data,
    api::clipboard::ImageType type,
    AdditionalDataItemList additional_items,
    const base::Closure& success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {
  if (!clipboard_extension_helper_)
    clipboard_extension_helper_ = base::MakeUnique<ClipboardExtensionHelper>();
  clipboard_extension_helper_->DecodeAndSaveImageData(
      image_data, type, std::move(additional_items), success_callback,
      error_callback);
}
#endif

}  // namespace extensions
