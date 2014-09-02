// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/athena/chrome_browser_main_extra_parts_athena.h"

#include "athena/extensions/public/extensions_delegate.h"
#include "athena/main/athena_launcher.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"

namespace {

class ChromeBrowserMainExtraPartsAthena : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsAthena() {}
  virtual ~ChromeBrowserMainExtraPartsAthena() {}

  // Overridden from ChromeBrowserMainExtraParts:
  virtual void PreProfileInit() OVERRIDE {
    athena::StartAthenaEnv(content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::FILE));
  }
  virtual void PostProfileInit() OVERRIDE {
    Profile* profile =
        g_browser_process->profile_manager()->GetActiveUserProfile();
    // TODO(oshima|polukhin): Start OOBE/Login process.
    athena::ExtensionsDelegate::CreateExtensionsDelegateForChrome(profile);
    athena::StartAthenaSessionWithContext(profile);
  }
  virtual void PostMainMessageLoopRun() OVERRIDE { athena::ShutdownAthena(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsAthena);
};

}  // namespace

ChromeBrowserMainExtraParts* CreateChromeBrowserMainExtraPartsAthena() {
  return new ChromeBrowserMainExtraPartsAthena();
}
