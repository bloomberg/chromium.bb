// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_app_registrar.h"

#include "base/callback.h"
#include "base/stl_util.h"
#include "url/gurl.h"

namespace web_app {

TestAppRegistrar::TestAppRegistrar() = default;

TestAppRegistrar::~TestAppRegistrar() = default;

void TestAppRegistrar::AddAsInstalled(const AppId& app_id) {
  installed_apps_.insert(app_id);
}

void TestAppRegistrar::RemoveAsInstalled(const AppId& app_id) {
  DCHECK(base::ContainsKey(installed_apps_, app_id));
  installed_apps_.erase(app_id);
}

void TestAppRegistrar::AddAsExternalAppUninstalledByUser(const AppId& app_id) {
  DCHECK(!base::ContainsKey(uninstalled_external_apps_, app_id));
  uninstalled_external_apps_.insert(app_id);
}

void TestAppRegistrar::Init(base::OnceClosure callback) {}

bool TestAppRegistrar::IsInstalled(const AppId& app_id) const {
  return base::ContainsKey(installed_apps_, app_id);
}

bool TestAppRegistrar::WasExternalAppUninstalledByUser(
    const AppId& app_id) const {
  return base::ContainsKey(uninstalled_external_apps_, app_id);
}

bool TestAppRegistrar::HasScopeUrl(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return false;
}

GURL TestAppRegistrar::GetScopeUrlForApp(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return GURL();
}

}  // namespace web_app
