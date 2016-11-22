// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_VPN_LIST_FORWARDER_H_
#define CHROME_BROWSER_UI_ASH_VPN_LIST_FORWARDER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {
class ExtensionRegistry;
}

// Forwards the list of extension-backed VPN providers in the primary user's
// profile to ash over mojo.
class VpnListForwarder : public extensions::ExtensionRegistryObserver,
                         public content::NotificationObserver {
 public:
  VpnListForwarder();
  ~VpnListForwarder() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  // Retrieves the current list of VPN providers enabled in the primary user's
  // profile and notifies observers that it has changed. Must only be called
  // when a user is logged in and the primary user's extension registry is being
  // observed.
  void UpdateVPNProviders();

  // Starts observing the primary user's extension registry to detect changes to
  // the list of VPN providers enabled in the user's profile and caches the
  // initial list. Must only be called when a user is logged in.
  void AttachToPrimaryUserExtensionRegistry();

  // Whether this object has ever sent a third-party provider list to ash.
  bool sent_providers_ = false;

  // The primary user's extension registry, if a user is logged in.
  extensions::ExtensionRegistry* extension_registry_ = nullptr;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<VpnListForwarder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VpnListForwarder);
};

#endif  // CHROME_BROWSER_UI_ASH_VPN_LIST_FORWARDER_H_
