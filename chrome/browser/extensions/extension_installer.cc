// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_installer.h"

#include "base/bind.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

ExtensionInstaller::ExtensionInstaller(Profile* profile)
    : requirements_checker_(new RequirementsChecker()),
      profile_(profile),
      weak_ptr_factory_(this) {
}

ExtensionInstaller::~ExtensionInstaller() {
}

void ExtensionInstaller::CheckRequirements(
    const RequirementsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  requirements_checker_->Check(extension_, callback);
}

string16 ExtensionInstaller::CheckManagementPolicy() {
  string16 error;
  bool allowed = ExtensionSystem::Get(profile_)->management_policy()
      ->UserMayLoad(extension_.get(), &error);
  DCHECK(allowed || !error.empty());
  return error;
}

}  // namespace extensions
