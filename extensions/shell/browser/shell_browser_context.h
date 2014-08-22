// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/shell/browser/shell_browser_context.h"
#include "webkit/browser/quota/special_storage_policy.h"

namespace extensions {

class ShellSpecialStoragePolicy;

// The BrowserContext used by the content, apps and extensions systems in
// app_shell.
class ShellBrowserContext : public content::ShellBrowserContext {
 public:
  ShellBrowserContext();
  virtual ~ShellBrowserContext();

  // content::BrowserContext implementation.
  virtual storage::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;

  // HACK: Pad the virtual function table so we trip an assertion if someone
  // tries to use |this| as a Profile.
  virtual void ProfileFunctionCallOnNonProfileBrowserContext1();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext2();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext3();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext4();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext5();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext6();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext7();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext8();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext9();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext10();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext11();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext12();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext13();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext14();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext15();

 private:
  scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserContext);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
