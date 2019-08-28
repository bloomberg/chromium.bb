// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/abstract_web_app_database.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class WebApp;

class WebAppRegistrar : public AppRegistrar {
 public:
  explicit WebAppRegistrar(Profile* profile, AbstractWebAppDatabase* database);
  ~WebAppRegistrar() override;

  void RegisterApp(std::unique_ptr<WebApp> web_app);
  std::unique_ptr<WebApp> UnregisterApp(const AppId& app_id);

  const WebApp* GetAppById(const AppId& app_id) const;

  bool is_empty() const { return registry_.empty(); }

  // Clears registry.
  void UnregisterAll();

  // AppRegistrar:
  void Init(base::OnceClosure callback) override;
  WebAppRegistrar* AsWebAppRegistrar() override;
  bool IsInstalled(const AppId& app_id) const override;
  bool IsLocallyInstalled(const AppId& app_id) const override;
  bool WasExternalAppUninstalledByUser(const AppId& app_id) const override;
  bool WasInstalledByUser(const AppId& app_id) const override;
  base::Optional<AppId> FindAppWithUrlInScope(const GURL& url) const override;
  int CountUserInstalledApps() const override;
  std::string GetAppShortName(const AppId& app_id) const override;
  std::string GetAppDescription(const AppId& app_id) const override;
  base::Optional<SkColor> GetAppThemeColor(const AppId& app_id) const override;
  const GURL& GetAppLaunchURL(const AppId& app_id) const override;
  base::Optional<GURL> GetAppScope(const AppId& app_id) const override;
  LaunchContainer GetAppLaunchContainer(const AppId& app_id) const override;
  std::vector<AppId> GetAppIds() const override;

  // Only range-based |for| loop supported. Don't use AppSet directly.
  // Doesn't support registration and unregistration of WebApp while iterating.
  class AppSet {
   public:
    // An iterator class that can be used to access the list of apps.
    class Iter {
     public:
      using InternalIter = Registry::const_iterator;

      explicit Iter(InternalIter&& internal_iter);
      Iter(Iter&&);
      ~Iter();

      void operator++() { ++internal_iter_; }
      WebApp& operator*() const { return *internal_iter_->second.get(); }
      bool operator!=(const Iter& iter) const {
        return internal_iter_ != iter.internal_iter_;
      }

     private:
      InternalIter internal_iter_;
      DISALLOW_COPY_AND_ASSIGN(Iter);
    };

    explicit AppSet(const WebAppRegistrar* registrar);
    AppSet(AppSet&&) = default;
    ~AppSet();

    using iterator = Iter;
    using const_iterator = Iter;

    iterator begin() { return std::move(begin_); }
    iterator end() { return std::move(end_); }

   private:
    Iter begin_;
    Iter end_;
#if DCHECK_IS_ON()
    const WebAppRegistrar* registrar_;
    const size_t mutations_count_;
#endif
    DISALLOW_COPY_AND_ASSIGN(AppSet);
  };

  AppSet AllApps() const;

  const Registry& registry_for_testing() const { return registry_; }

 private:
  void OnDatabaseOpened(base::OnceClosure callback, Registry registry);

  void CountMutation();

  Registry registry_;

  // An abstract database, not owned by this registrar.
  AbstractWebAppDatabase* database_;

#if DCHECK_IS_ON()
  size_t mutations_count_ = 0;
#endif

  base::WeakPtrFactory<WebAppRegistrar> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppRegistrar);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
