// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/shell_browser_context.h"

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
ShellBrowserContext::ShellBrowserContext()
    : content::ShellBrowserContext(false, NULL) {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}

ShellBrowserContext::~ShellBrowserContext() {
}

void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext1() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext2() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext3() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext4() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext5() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext6() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext7() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext8() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext9() {
  NOTREACHED();
}

}  // namespace apps
