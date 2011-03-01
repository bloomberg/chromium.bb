// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise_extension_observer.h"

#include "base/file_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

EnterpriseExtensionObserver::EnterpriseExtensionObserver(Profile* profile)
    : profile_(profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this,
                 NotificationType::EXTENSION_INSTALLED,
                 Source<Profile>(profile_));
}

void EnterpriseExtensionObserver::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == NotificationType::EXTENSION_INSTALLED);
  if (Source<Profile>(source).ptr() != profile_) {
    return;
  }
  Extension* extension = Details<Extension>(details).ptr();
  if (extension->location() != Extension::EXTERNAL_POLICY_DOWNLOAD) {
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      NewRunnableFunction(
          &EnterpriseExtensionObserver::CheckExtensionAndNotifyEntd,
          extension->path()));
}

// static
void EnterpriseExtensionObserver::CheckExtensionAndNotifyEntd(
    const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (file_util::PathExists(
      path.Append(FILE_PATH_LITERAL("isa-cros-policy")))) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableFunction(&EnterpriseExtensionObserver::NotifyEntd));
  }
}

// static
void EnterpriseExtensionObserver::NotifyEntd() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (CrosLibrary::Get()->EnsureLoaded()) {
    CrosLibrary::Get()->GetLoginLibrary()->RestartEntd();
    return;
  }
}

}  // namespace chromeos
