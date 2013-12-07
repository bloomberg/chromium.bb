// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app_shell_browser_context.h"

#include "apps/app_load_service_factory.h"

namespace {

// See ChromeBrowserMainExtraPartsProfiles for details.
void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  apps::AppLoadServiceFactory::GetInstance();
}

}  // namespace

namespace apps {

// TODO(jamescook): Should this be an off-the-record context?
// TODO(jamescook): Could initialize NetLog here to get logs from the networking
// stack.
AppShellBrowserContext::AppShellBrowserContext()
    : content::ShellBrowserContext(false, NULL) {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}

AppShellBrowserContext::~AppShellBrowserContext() {
}

void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext1() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext2() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext3() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext4() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext5() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext6() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext7() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext8() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext9() {
  NOTREACHED();
}

}  // namespace apps
