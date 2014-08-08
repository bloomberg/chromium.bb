// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXTENSIONS_API_CLIENT_H_
#define EXTENSIONS_BROWSER_API_EXTENSIONS_API_CLIENT_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "extensions/browser/api/storage/settings_namespace.h"

class GURL;

template <class T>
class ObserverListThreadSafe;

namespace content {
class BrowserContext;
}

namespace device {
class HidService;
}

namespace extensions {

class SettingsObserver;
class SettingsStorageFactory;
class ValueStoreCache;

// Allows the embedder of the extensions module to customize its support for
// API features. The embedder must create a single instance in the browser
// process. Provides a default implementation that does nothing.
class ExtensionsAPIClient {
 public:
  // Construction sets the single instance.
  ExtensionsAPIClient();

  // Destruction clears the single instance.
  virtual ~ExtensionsAPIClient();

  // Returns the single instance of |this|.
  static ExtensionsAPIClient* Get();

  // Storage API support.

  // Add any additional value store caches (e.g. for chrome.storage.managed)
  // to |caches|. By default adds nothing.
  virtual void AddAdditionalValueStoreCaches(
      content::BrowserContext* context,
      const scoped_refptr<SettingsStorageFactory>& factory,
      const scoped_refptr<ObserverListThreadSafe<SettingsObserver> >& observers,
      std::map<settings_namespace::Namespace, ValueStoreCache*>* caches);

  // Attaches a frame |url| inside the <appview> specified by
  // |guest_instance_id|. Returns true if the operation completes succcessfully.
  virtual bool AppViewInternalAttachFrame(
      content::BrowserContext* browser_context,
      const GURL& url,
      int guest_instance_id,
      const std::string& guest_extension_id);

  // Denies the embedding requested by the <appview> specified by
  // |guest_instance_id|. Returns true if the operation completes successfully.
  virtual bool AppViewInternalDenyRequest(
      content::BrowserContext* browser_context,
      int guest_instance_id,
      const std::string& guest_extension_id);

  // Returns the HidService instance for this embedder.
  virtual device::HidService* GetHidService();

  virtual void RegisterGuestViewTypes() {}

  // NOTE: If this interface gains too many methods (perhaps more than 20) it
  // should be split into one interface per API.
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_EXTENSIONS_API_CLIENT_H_
