// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_installer.h"

#include "base/bind.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#endif

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

#if defined(ENABLE_MANAGED_USERS)
void ExtensionInstaller::ShowPassphraseDialog(
    content::WebContents* web_contents,
    const base::Closure& authorization_callback) {

  ManagedUserService* service =
      ManagedUserServiceFactory::GetForProfile(profile());
  // Check whether the profile is managed.
  if (service->ProfileIsManaged()) {
    service->RequestAuthorization(
        web_contents,
        base::Bind(&ExtensionInstaller::OnAuthorizationResult,
                   weak_ptr_factory_.GetWeakPtr(),
                   authorization_callback));
    return;
  }
  authorization_callback.Run();
}

void ExtensionInstaller::OnAuthorizationResult(
    const base::Closure& authorization_callback,
    bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (success) {
    ManagedUserService* service =
        ManagedUserServiceFactory::GetForProfile(profile_);
    if (service->ProfileIsManaged())
      service->AddElevationForExtension(extension_->id());
  }
  authorization_callback.Run();
}
#endif

string16 ExtensionInstaller::CheckManagementPolicy() {
  string16 error;
  bool allowed =
      ExtensionSystem::Get(profile_)->management_policy()->UserMayLoad(
          extension_, &error);
  DCHECK(allowed || !error.empty());
  return error;
}

}  // namespace extensions
