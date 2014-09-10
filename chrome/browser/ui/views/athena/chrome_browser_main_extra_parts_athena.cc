// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/athena/chrome_browser_main_extra_parts_athena.h"

#include "athena/env/public/athena_env.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "athena/main/public/athena_launcher.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

namespace {

class ChromeBrowserMainExtraPartsAthena : public ChromeBrowserMainExtraParts,
                                          public content::NotificationObserver {
 public:
  ChromeBrowserMainExtraPartsAthena() {
    registrar_.Add(this,
                   chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources());
  }

  virtual ~ChromeBrowserMainExtraPartsAthena() {}

 private:
  // Overridden from ChromeBrowserMainExtraParts:
  virtual void PreProfileInit() OVERRIDE {
    athena::StartAthenaEnv(content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::FILE));
  }
  virtual void PostProfileInit() OVERRIDE {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableZeroBrowsersOpenForTests)) {
      chrome::IncrementKeepAliveCount();
    }
    Profile* profile =
        g_browser_process->profile_manager()->GetActiveUserProfile();
    // TODO(oshima|polukhin): Start OOBE/Login process.
    athena::ExtensionsDelegate::CreateExtensionsDelegateForChrome(profile);
    athena::StartAthenaSessionWithContext(profile);
  }
  virtual void PostMainMessageLoopRun() OVERRIDE { athena::ShutdownAthena(); }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_APP_TERMINATING:
        athena::AthenaEnv::Get()->OnTerminating();
        break;
    }
  }

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsAthena);
};

}  // namespace

ChromeBrowserMainExtraParts* CreateChromeBrowserMainExtraPartsAthena() {
  return new ChromeBrowserMainExtraPartsAthena();
}
