// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise_extension_observer.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

EnterpriseExtensionObserver::EnterpriseExtensionObserver(Profile* profile)
    : profile_(profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
}

void EnterpriseExtensionObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == chrome::NOTIFICATION_EXTENSION_INSTALLED);
  if (content::Source<Profile>(source).ptr() != profile_) {
    return;
  }
  extensions::Extension* extension =
      content::Details<extensions::Extension>(details).ptr();
  if (extension->location() !=
      extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD) {
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &EnterpriseExtensionObserver::CheckExtensionAndNotifyEntd,
          extension->path()));
}

// static
void EnterpriseExtensionObserver::CheckExtensionAndNotifyEntd(
    const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (file_util::PathExists(
      path.Append(FILE_PATH_LITERAL("isa-cros-policy")))) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&EnterpriseExtensionObserver::NotifyEntd));
  }
}

// static
void EnterpriseExtensionObserver::NotifyEntd() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DBusThreadManager::Get()->GetSessionManagerClient()->RestartEntd();
}

}  // namespace chromeos
