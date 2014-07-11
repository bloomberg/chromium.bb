// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"

#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"
#include "chrome/browser/guest_view/app_view/app_view_guest.h"
#include "content/public/browser/browser_context.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"
#endif

namespace extensions {

ChromeExtensionsAPIClient::ChromeExtensionsAPIClient() {}

ChromeExtensionsAPIClient::~ChromeExtensionsAPIClient() {}

void ChromeExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<SettingsStorageFactory>& factory,
    const scoped_refptr<ObserverListThreadSafe<SettingsObserver> >& observers,
    std::map<settings_namespace::Namespace, ValueStoreCache*>* caches) {
  // Add support for chrome.storage.sync.
  (*caches)[settings_namespace::SYNC] =
      new SyncValueStoreCache(factory, observers, context->GetPath());

#if defined(ENABLE_CONFIGURATION_POLICY)
  // Add support for chrome.storage.managed.
  (*caches)[settings_namespace::MANAGED] =
      new ManagedValueStoreCache(context, factory, observers);
#endif
}

bool ChromeExtensionsAPIClient::AppViewInternalAttachFrame(
    content::BrowserContext* browser_context,
    const GURL& url,
    int guest_instance_id,
    const std::string& guest_extension_id) {
  return AppViewGuest::CompletePendingRequest(browser_context,
                                              url,
                                              guest_instance_id,
                                              guest_extension_id);
}

bool ChromeExtensionsAPIClient::AppViewInternalDenyRequest(
    content::BrowserContext* browser_context,
    int guest_instance_id,
    const std::string& guest_extension_id) {
  return AppViewGuest::CompletePendingRequest(browser_context,
                                              GURL(),
                                              guest_instance_id,
                                              guest_extension_id);
}

}  // namespace extensions
