// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INSTALL_LIMITER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INSTALL_LIMITER_H_

#include <queue>
#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {

// InstallLimiter defers big app installs after all small app installs and then
// runs big app installs one by one. This improves first-time login experience.
// See http://crbug.com/166296
class InstallLimiter : public ProfileKeyedService,
                       public content::NotificationObserver,
                       public base::SupportsWeakPtr<InstallLimiter> {
 public:
  static InstallLimiter* Get(Profile* profile);

  InstallLimiter();
  virtual ~InstallLimiter();

  void DisableForTest();

  void Add(const scoped_refptr<CrxInstaller>& installer,
           const base::FilePath& path);

 private:
  // DeferredInstall holds info to run a CrxInstaller later.
  struct DeferredInstall {
    DeferredInstall(const scoped_refptr<CrxInstaller>& installer,
                   const base::FilePath& path);
    ~DeferredInstall();

    const scoped_refptr<CrxInstaller> installer;
    const base::FilePath path;
  };

  typedef std::queue<DeferredInstall> DeferredInstallList;
  typedef std::set<scoped_refptr<CrxInstaller> > CrxInstallerSet;

  // Adds install info with size. If |size| is greater than a certain threshold,
  // it stores the install info into |deferred_installs_| to run it later.
  // Otherwise, it just runs the installer.
  void AddWithSize(const scoped_refptr<CrxInstaller>& installer,
                   const base::FilePath& path,
                   int64* size);

  // Checks and runs deferred big app installs when appropriate.
  void CheckAndRunDeferrredInstalls();

  // Starts install using passed-in info and observes |installer|'s done
  // notification.
  void RunInstall(const scoped_refptr<CrxInstaller>& installer,
                  const base::FilePath& path);

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  DeferredInstallList deferred_installs_;
  CrxInstallerSet running_installers_;

  // A timer to wait before running deferred big app install.
  base::OneShotTimer<InstallLimiter> wait_timer_;

  bool disabled_for_test_;

  DISALLOW_COPY_AND_ASSIGN(InstallLimiter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INSTALL_LIMITER_H_
