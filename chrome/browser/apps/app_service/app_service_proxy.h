// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/cpp/icon_cache.h"
#include "chrome/services/app_service/public/cpp/icon_coalescer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/apps/app_service/built_in_chromeos_apps.h"
#include "chrome/browser/apps/app_service/crostini_apps.h"
#include "chrome/browser/apps/app_service/extension_apps.h"
#endif  // OS_CHROMEOS

class Profile;

namespace apps {

class AppServiceImpl;

// Singleton (per Profile) proxy and cache of an App Service's apps.
//
// Singleton-ness means that //chrome/browser code (e.g UI code) can find *the*
// proxy for a given Profile, and therefore share its caches.
//
// See chrome/services/app_service/README.md.
class AppServiceProxy : public KeyedService,
                        public apps::IconLoader,
                        public apps::mojom::Subscriber {
 public:
  explicit AppServiceProxy(Profile* profile);
  ~AppServiceProxy() override;

  void ReInitializeForTesting(Profile* profile);

  mojo::Remote<apps::mojom::AppService>& AppService();
  apps::AppRegistryCache& AppRegistryCache();

  // apps::IconLoader overrides.
  apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override;
  std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IconKeyPtr icon_key,
      apps::mojom::IconCompression icon_compression,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::mojom::Publisher::LoadIconCallback callback) override;

  // TODO: Provide comments for public API methods.
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id);
  void LaunchAppWithIntent(const std::string& app_id,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id);
  void LaunchAppWithUrl(const std::string& app_id,
                        GURL url,
                        apps::mojom::LaunchSource launch_source,
                        int64_t display_id);
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission);
  void Uninstall(const std::string& app_id);
  void OnUninstallDialogClosed(apps::mojom::AppType app_type,
                               const std::string& app_id,
                               bool uninstall,
                               bool clear_site_data,
                               bool report_abuse,
                               UninstallDialog* uninstall_dialog);
  void OpenNativeSettings(const std::string& app_id);

  void FlushMojoCallsForTesting();
  apps::IconLoader* OverrideInnerIconLoaderForTesting(
      apps::IconLoader* icon_loader);
  void ReInitializeCrostiniForTesting(Profile* profile);
  std::vector<std::string> GetAppIdsForUrl(const GURL& url);
  std::vector<std::string> GetAppIdsForIntent(apps::mojom::IntentPtr intent);
  void SetArcIsRegistered();

 private:
  // An adapter, presenting an IconLoader interface based on the underlying
  // Mojo service (or on a fake implementation for testing).
  //
  // Conceptually, the ASP (the AppServiceProxy) is itself such an adapter:
  // UI clients call the IconLoader::LoadIconFromIconKey method (which the ASP
  // implements) and the ASP translates (i.e. adapts) these to Mojo calls (or
  // C++ calls to the Fake). This diagram shows control flow going left to
  // right (with "=c=>" and "=m=>" denoting C++ and Mojo calls), and the
  // responses (callbacks) then run right to left in LIFO order:
  //
  //   UI =c=> ASP =+=m=> MojoService
  //                |       or
  //                +=c=> Fake
  //
  // It is more complicated in practice, as we want to insert IconLoader
  // decorators (as in the classic "decorator" or "wrapper" design pattern) to
  // provide optimizations like proxy-wide icon caching and IPC coalescing
  // (de-duplication). Nonetheless, from a UI client's point of view, we would
  // still like to present a simple API: that the ASP implements the IconLoader
  // interface. We therefore split the "ASP" component into multiple
  // sub-components. Once again, control flow runs from left to right, and
  // inside the ASP, outer layers (wrappers) call into inner layers (wrappees):
  //
  //           +------------------ ASP ------------------+
  //           |                                         |
  //   UI =c=> | Outer =c=> MoreDecorators... =c=> Inner | =+=m=> MojoService
  //           |                                         |  |       or
  //           +-----------------------------------------+  +=c=> Fake
  //
  // The inner_icon_loader_ field (of type InnerIconLoader) is the "Inner"
  // component: the one that ultimately talks to the Mojo service.
  //
  // The outer_icon_loader_ field (of type IconCache) is the "Outer" component:
  // the entry point for calls into the AppServiceProxy.
  //
  // Note that even if the ASP provides some icon caching, upstream UI clients
  // may want to introduce further icon caching. See the commentary where
  // IconCache::GarbageCollectionPolicy is defined.
  //
  // IPC coalescing would be one of the "MoreDecorators".
  class InnerIconLoader : public apps::IconLoader {
   public:
    explicit InnerIconLoader(AppServiceProxy* host);

    // apps::IconLoader overrides.
    apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override;
    std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
        apps::mojom::AppType app_type,
        const std::string& app_id,
        apps::mojom::IconKeyPtr icon_key,
        apps::mojom::IconCompression icon_compression,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::mojom::Publisher::LoadIconCallback callback) override;

    // |host_| owns |this|, as the InnerIconLoader is an AppServiceProxy
    // field.
    AppServiceProxy* host_;

    apps::IconLoader* overriding_icon_loader_for_testing_;
  };

  void Initialize();

  void AddAppIconSource(Profile* profile);

  // KeyedService overrides.
  void Shutdown() override;

  // apps::mojom::Subscriber overrides.
  void OnApps(std::vector<apps::mojom::AppPtr> deltas) override;
  void Clone(mojo::PendingReceiver<apps::mojom::Subscriber> receiver) override;

  // This proxy privately owns its instance of the App Service. This should not
  // be exposed except through the Mojo interface connected to |app_service_|.
  std::unique_ptr<apps::AppServiceImpl> app_service_impl_;

  mojo::Remote<apps::mojom::AppService> app_service_;
  apps::AppRegistryCache cache_;

  mojo::ReceiverSet<apps::mojom::Subscriber> receivers_;

  // The LoadIconFromIconKey implementation sends a chained series of requests
  // through each icon loader, starting from the outer and working back to the
  // inner. Fields are listed from inner to outer, the opposite of call order,
  // as each one depends on the previous one, and in the constructor,
  // initialization happens in field order.
  InnerIconLoader inner_icon_loader_;
  IconCoalescer icon_coalescer_;
  IconCache outer_icon_loader_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<BuiltInChromeOsApps> built_in_chrome_os_apps_;
  std::unique_ptr<CrostiniApps> crostini_apps_;
  std::unique_ptr<ExtensionApps> extension_apps_;
  std::unique_ptr<ExtensionApps> extension_web_apps_;
  bool arc_is_registered_ = false;
#endif  // OS_CHROMEOS

  Profile* profile_;

  using UninstallDialogs = std::set<std::unique_ptr<apps::UninstallDialog>,
                                    base::UniquePtrComparator>;
  UninstallDialogs uninstall_dialogs_;

  base::WeakPtrFactory<AppServiceProxy> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppServiceProxy);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_
