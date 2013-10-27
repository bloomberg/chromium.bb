// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_ephemeral_installer.h"

#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace extensions {

namespace {

// TODO(tmdiep): Launching the app will be moved downstream when an ephemeral
// app service/manager is implemented.
void LaunchApp(Profile* profile, const std::string& id) {
  ExtensionService* service = extensions::ExtensionSystem::Get(profile)->
      extension_service();
  DCHECK(service);
  const extensions::Extension* extension = service->GetExtensionById(id, false);
  if (extension) {
    AppLaunchParams params(profile, extension, NEW_FOREGROUND_TAB);
    params.desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
    OpenApplication(params);
  }
}

void OnInstallDone(Profile* profile,
                   const std::string& id,
                   const WebstoreEphemeralInstaller::Callback& callback,
                   bool success,
                   const std::string& error) {
  if (success)
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&LaunchApp, profile, id),
        base::TimeDelta());

  callback.Run(success, error);
}

}  // namespace

// static
scoped_refptr<WebstoreEphemeralInstaller>
WebstoreEphemeralInstaller::CreateForLauncher(
    const std::string& webstore_item_id,
    Profile* profile,
    gfx::NativeWindow parent_window,
    const Callback& callback) {
  scoped_refptr<WebstoreEphemeralInstaller> installer =
      new WebstoreEphemeralInstaller(webstore_item_id,
                                     profile,
                                     parent_window,
                                     callback);
  installer->set_install_source(
      extensions::WebstoreInstaller::INSTALL_SOURCE_APP_LAUNCHER);
  return installer;
}

WebstoreEphemeralInstaller::WebstoreEphemeralInstaller(
    const std::string& webstore_item_id,
    Profile* profile,
    gfx::NativeWindow parent_window,
    const Callback& callback)
        : WebstoreStandaloneInstaller(
              webstore_item_id,
              profile,
              base::Bind(OnInstallDone, profile, webstore_item_id, callback)),
          parent_window_(parent_window),
          dummy_web_contents_(
              WebContents::Create(WebContents::CreateParams(profile))) {}

WebstoreEphemeralInstaller::~WebstoreEphemeralInstaller() {}

bool WebstoreEphemeralInstaller::CheckRequestorAlive() const {
  return true;
}

const GURL& WebstoreEphemeralInstaller::GetRequestorURL() const {
  return GURL::EmptyGURL();
}

bool WebstoreEphemeralInstaller::ShouldShowPostInstallUI() const {
  return false;
}

bool WebstoreEphemeralInstaller::ShouldShowAppInstalledBubble() const {
  return false;
}

WebContents* WebstoreEphemeralInstaller::GetWebContents() const {
  return dummy_web_contents_.get();
}

scoped_ptr<ExtensionInstallPrompt::Prompt>
WebstoreEphemeralInstaller::CreateInstallPrompt() const {
  DCHECK(manifest() != NULL);

  // Skip the prompt by returning null if the app does not need to display
  // permission warnings.
  // Extension::Create() can return null when an error occurs, e.g. due to a bad
  // manifest. In this case we should return a valid prompt and the bad manifest
  // will be handled in WebstoreStandaloneInstaller::ShowInstallUI(). If null is
  // returned here, the install will proceed.
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      base::FilePath(),
      Manifest::INTERNAL,
      *manifest(),
      Extension::REQUIRE_KEY | Extension::FROM_WEBSTORE,
      id(),
      &error);
  if (extension.get()) {
    PermissionMessages permissions =
        PermissionsData::GetPermissionMessages(extension.get());
    if (permissions.empty())
      return scoped_ptr<ExtensionInstallPrompt::Prompt>();
  }

  return make_scoped_ptr(new ExtensionInstallPrompt::Prompt(
      ExtensionInstallPrompt::LAUNCH_PROMPT));
}

bool WebstoreEphemeralInstaller::CheckInlineInstallPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  *error = "";
  return true;
}

bool WebstoreEphemeralInstaller::CheckRequestorPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  *error = "";
  return true;
}

scoped_ptr<ExtensionInstallPrompt>
WebstoreEphemeralInstaller::CreateInstallUI() {
  return make_scoped_ptr(
      new ExtensionInstallPrompt(profile(), parent_window_, NULL));
}

}  // namespace extensions
