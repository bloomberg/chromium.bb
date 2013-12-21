// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_SHELL_EXTENSIONS_BROWSER_CLIENT_H_
#define APPS_SHELL_SHELL_EXTENSIONS_BROWSER_CLIENT_H_

#include "base/compiler_specific.h"
#include "extensions/browser/extensions_browser_client.h"

class PrefService;

namespace apps {

// An ExtensionsBrowserClient that supports a single content::BrowserContent
// with no related incognito context.
class ShellExtensionsBrowserClient
    : public extensions::ExtensionsBrowserClient {
 public:
  // |context| is the single BrowserContext used for IsValidContext() below.
  explicit ShellExtensionsBrowserClient(content::BrowserContext* context);
  virtual ~ShellExtensionsBrowserClient();

  // extensions::ExtensionsBrowserClient overrides:
  virtual bool IsShuttingDown() OVERRIDE;
  virtual bool AreExtensionsDisabled(const CommandLine& command_line,
                                     content::BrowserContext* context) OVERRIDE;
  virtual bool IsValidContext(content::BrowserContext* context) OVERRIDE;
  virtual bool IsSameContext(content::BrowserContext* first,
                             content::BrowserContext* second) OVERRIDE;
  virtual bool HasOffTheRecordContext(content::BrowserContext* context)
      OVERRIDE;
  virtual content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) OVERRIDE;
  virtual content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) OVERRIDE;
  virtual PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) OVERRIDE;
  virtual bool DeferLoadingBackgroundHosts(content::BrowserContext* context)
      const OVERRIDE;
  virtual bool IsBackgroundPageAllowed(content::BrowserContext* context)
      const OVERRIDE;
  virtual bool DidVersionUpdate(content::BrowserContext* context) OVERRIDE;
  virtual scoped_ptr<extensions::AppSorting> CreateAppSorting() OVERRIDE;
  virtual bool IsRunningInForcedAppMode() OVERRIDE;
  virtual content::JavaScriptDialogManager* GetJavaScriptDialogManager()
      OVERRIDE;

 private:
  // The single BrowserContext for app_shell. Not owned.
  content::BrowserContext* browser_context_;

  // The PrefService for |browser_context_|.
  scoped_ptr<PrefService> prefs_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsBrowserClient);
};

}  // namespace apps

#endif  // APPS_SHELL_SHELL_EXTENSIONS_BROWSER_CLIENT_H_
