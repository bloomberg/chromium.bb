// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_browser_context.h"
#include "apps/shell/browser/shell_special_storage_policy.h"

namespace apps {

// Create a normal recording browser context. If we used an incognito context
// then app_shell would also have to create a normal context and manage both.
ShellBrowserContext::ShellBrowserContext()
    : content::ShellBrowserContext(false, NULL),
      storage_policy_(new ShellSpecialStoragePolicy) {
}

ShellBrowserContext::~ShellBrowserContext() {
}

quota::SpecialStoragePolicy* ShellBrowserContext::GetSpecialStoragePolicy() {
  return storage_policy_.get();
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
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext10() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext11() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext12() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext13() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext14() {
  NOTREACHED();
}
void ShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext15() {
  NOTREACHED();
}

}  // namespace apps
