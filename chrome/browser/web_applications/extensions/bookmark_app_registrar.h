// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_REGISTRAR_H_

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/app_registrar.h"

class Profile;

namespace extensions {

class Extension;

class BookmarkAppRegistrar : public web_app::AppRegistrar {
 public:
  explicit BookmarkAppRegistrar(Profile* profile);
  ~BookmarkAppRegistrar() override;

  // AppRegistrar
  void Init(base::OnceClosure callback) override;
  bool IsInstalled(const GURL& start_url) const override;
  bool IsInstalled(const web_app::AppId& app_id) const override;
  bool WasExternalAppUninstalledByUser(
      const web_app::AppId& app_id) const override;
  bool HasScopeUrl(const web_app::AppId& app_id) const override;
  GURL GetScopeUrlForApp(const web_app::AppId& app_id) const override;

 private:
  const Extension* GetExtension(const web_app::AppId& app_id) const;

  Profile* profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_REGISTRAR_H_
