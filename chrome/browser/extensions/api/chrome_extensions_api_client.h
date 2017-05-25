// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CHROME_EXTENSIONS_API_CLIENT_H_
#define CHROME_BROWSER_EXTENSIONS_API_CHROME_EXTENSIONS_API_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "extensions/browser/api/extensions_api_client.h"

namespace extensions {

class ChromeMetricsPrivateDelegate;
class ClipboardExtensionHelper;

// Extra support for extensions APIs in Chrome.
class ChromeExtensionsAPIClient : public ExtensionsAPIClient {
 public:
  ChromeExtensionsAPIClient();
  ~ChromeExtensionsAPIClient() override;

  // ExtensionsApiClient implementation.
  void AddAdditionalValueStoreCaches(
      content::BrowserContext* context,
      const scoped_refptr<ValueStoreFactory>& factory,
      const scoped_refptr<base::ObserverListThreadSafe<SettingsObserver>>&
          observers,
      std::map<settings_namespace::Namespace, ValueStoreCache*>* caches)
      override;
  void AttachWebContentsHelpers(content::WebContents* web_contents) const
      override;
  AppViewGuestDelegate* CreateAppViewGuestDelegate() const override;
  ExtensionOptionsGuestDelegate* CreateExtensionOptionsGuestDelegate(
      ExtensionOptionsGuest* guest) const override;
  std::unique_ptr<guest_view::GuestViewManagerDelegate>
  CreateGuestViewManagerDelegate(
      content::BrowserContext* context) const override;
  std::unique_ptr<MimeHandlerViewGuestDelegate>
  CreateMimeHandlerViewGuestDelegate(
      MimeHandlerViewGuest* guest) const override;
  WebViewGuestDelegate* CreateWebViewGuestDelegate(
      WebViewGuest* web_view_guest) const override;
  WebViewPermissionHelperDelegate* CreateWebViewPermissionHelperDelegate(
      WebViewPermissionHelper* web_view_permission_helper) const override;
  std::unique_ptr<WebRequestEventRouterDelegate>
  CreateWebRequestEventRouterDelegate() const override;
  scoped_refptr<ContentRulesRegistry> CreateContentRulesRegistry(
      content::BrowserContext* browser_context,
      RulesCacheDelegate* cache_delegate) const override;
  std::unique_ptr<DevicePermissionsPrompt> CreateDevicePermissionsPrompt(
      content::WebContents* web_contents) const override;
  std::unique_ptr<VirtualKeyboardDelegate> CreateVirtualKeyboardDelegate()
      const override;
  ManagementAPIDelegate* CreateManagementAPIDelegate() const override;
  MetricsPrivateDelegate* GetMetricsPrivateDelegate() override;
  NetworkingCastPrivateDelegate* GetNetworkingCastPrivateDelegate() override;

#if defined(OS_CHROMEOS)
  NonNativeFileSystemDelegate* GetNonNativeFileSystemDelegate() override;

  void SaveImageDataToClipboard(
      const std::vector<char>& image_data,
      api::clipboard::ImageType type,
      AdditionalDataItemList additional_items,
      const base::Closure& success_callback,
      const base::Callback<void(const std::string&)>& error_callback) override;
#endif

 private:
  std::unique_ptr<ChromeMetricsPrivateDelegate> metrics_private_delegate_;
  std::unique_ptr<NetworkingCastPrivateDelegate>
      networking_cast_private_delegate_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<NonNativeFileSystemDelegate> non_native_file_system_delegate_;
  std::unique_ptr<ClipboardExtensionHelper> clipboard_extension_helper_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsAPIClient);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CHROME_EXTENSIONS_API_CLIENT_H_
