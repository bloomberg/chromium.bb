// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/common/extensions/manifest_handlers/nacl_modules_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

class PluginManager : public ProfileKeyedAPI,
                      public content::NotificationObserver {
 public:
  explicit PluginManager(content::BrowserContext* context);
  virtual ~PluginManager();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<PluginManager>* GetFactoryInstance();

  // content::NotificationObserver impelmentation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<PluginManager>;

  // We implement some Pepper plug-ins using NaCl to take advantage of NaCl's
  // strong sandbox. Typically, these NaCl modules are stored in extensions
  // and registered here. Not all NaCl modules need to register for a MIME
  // type, just the ones that are responsible for rendering a particular MIME
  // type, like application/pdf. Note: We only register NaCl modules in the
  // browser process.
  void RegisterNaClModule(const NaClModuleInfo& info);
  void UnregisterNaClModule(const NaClModuleInfo& info);

  // Call UpdatePluginListWithNaClModules() after registering or unregistering
  // a NaCl module to see those changes reflected in the PluginList.
  void UpdatePluginListWithNaClModules();

  extensions::NaClModuleInfo::List::iterator FindNaClModule(const GURL& url);

  // ProfileKeyedAPI implementation.
  static const char* service_name() { return "PluginManager"; }
  static const bool kServiceIsNULLWhileTesting = true;

  extensions::NaClModuleInfo::List nacl_module_list_;

  content::NotificationRegistrar registrar_;

  Profile* profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_
