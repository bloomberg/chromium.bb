// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/ephemeral_app_launcher.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/permissions/permissions_data.h"

using content::WebContents;
using extensions::Extension;
using extensions::ExtensionSystem;
using extensions::WebstoreInstaller;

namespace {

const char kInvalidManifestError[] = "Invalid manifest";
const char kExtensionTypeError[] = "Ephemeral extensions are not permitted";
const char kLaunchAbortedError[] = "Launch aborted";

Profile* ProfileForWebContents(content::WebContents* contents) {
  if (!contents)
    return NULL;

  return Profile::FromBrowserContext(contents->GetBrowserContext());
}

gfx::NativeWindow NativeWindowForWebContents(content::WebContents* contents) {
  if (!contents)
    return NULL;

  return contents->GetView()->GetTopLevelNativeWindow();
}

}  // namespace

// static
scoped_refptr<EphemeralAppLauncher>
EphemeralAppLauncher::CreateForLauncher(
    const std::string& webstore_item_id,
    Profile* profile,
    gfx::NativeWindow parent_window,
    const Callback& callback) {
  scoped_refptr<EphemeralAppLauncher> installer =
      new EphemeralAppLauncher(webstore_item_id,
                               profile,
                               parent_window,
                               callback);
  installer->set_install_source(WebstoreInstaller::INSTALL_SOURCE_APP_LAUNCHER);
  return installer;
}

// static
scoped_refptr<EphemeralAppLauncher>
EphemeralAppLauncher::CreateForLink(
    const std::string& webstore_item_id,
    content::WebContents* web_contents) {
  scoped_refptr<EphemeralAppLauncher> installer =
      new EphemeralAppLauncher(webstore_item_id,
                               web_contents,
                               Callback());
  installer->set_install_source(WebstoreInstaller::INSTALL_SOURCE_OTHER);
  return installer;
}

void EphemeralAppLauncher::Start() {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  DCHECK(extension_service);

  const Extension* extension = extension_service->GetInstalledExtension(id());
  if (extension) {
    if (extensions::util::IsAppLaunchableWithoutEnabling(extension->id(),
                                                         profile())) {
      LaunchApp(extension);
      InvokeCallback(std::string());
      return;
    }

    // The ephemeral app may have been updated and disabled as it requests
    // more permissions. In this case we should always prompt before
    // launching.
    extension_enable_flow_.reset(
        new ExtensionEnableFlow(profile(), extension->id(), this));
    if (web_contents())
      extension_enable_flow_->StartForWebContents(web_contents());
    else
      extension_enable_flow_->StartForNativeWindow(parent_window_);

    // Keep this object alive until the enable flow is complete.
    AddRef();  // Balanced in WebstoreStandaloneInstaller::CompleteInstall.
    return;
  }

  // Fetch the app from the webstore.
  StartObserving();
  BeginInstall();
}

EphemeralAppLauncher::EphemeralAppLauncher(
    const std::string& webstore_item_id,
    Profile* profile,
    gfx::NativeWindow parent_window,
    const Callback& callback)
        : WebstoreStandaloneInstaller(
              webstore_item_id,
              profile,
              callback),
          parent_window_(parent_window),
          dummy_web_contents_(
              WebContents::Create(WebContents::CreateParams(profile))) {
}

EphemeralAppLauncher::EphemeralAppLauncher(
    const std::string& webstore_item_id,
    content::WebContents* web_contents,
    const Callback& callback)
        : WebstoreStandaloneInstaller(
              webstore_item_id,
              ProfileForWebContents(web_contents),
              callback),
          content::WebContentsObserver(web_contents),
          parent_window_(NativeWindowForWebContents(web_contents)) {
}

EphemeralAppLauncher::~EphemeralAppLauncher() {}

void EphemeralAppLauncher::StartObserving() {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile()->GetOriginalProfile()));
}

void EphemeralAppLauncher::LaunchApp(const Extension* extension) const {
  DCHECK(extension);
  if (!extension->is_app()) {
    LOG(ERROR) << "Unable to launch extension " << extension->id()
               << ". It is not an app.";
    return;
  }

  AppLaunchParams params(profile(), extension, NEW_FOREGROUND_TAB);
  params.desktop_type =
      chrome::GetHostDesktopTypeForNativeWindow(parent_window_);
  OpenApplication(params);
}

bool EphemeralAppLauncher::CheckRequestorAlive() const {
  return dummy_web_contents_.get() != NULL || web_contents() != NULL;
}

const GURL& EphemeralAppLauncher::GetRequestorURL() const {
  return GURL::EmptyGURL();
}

bool EphemeralAppLauncher::ShouldShowPostInstallUI() const {
  return false;
}

bool EphemeralAppLauncher::ShouldShowAppInstalledBubble() const {
  return false;
}

WebContents* EphemeralAppLauncher::GetWebContents() const {
  return web_contents() ? web_contents() : dummy_web_contents_.get();
}

scoped_ptr<ExtensionInstallPrompt::Prompt>
EphemeralAppLauncher::CreateInstallPrompt() const {
  DCHECK(extension_.get() != NULL);

  // Skip the prompt by returning null if the app does not need to display
  // permission warnings.
  extensions::PermissionMessages permissions =
      extensions::PermissionsData::GetPermissionMessages(extension_.get());
  if (permissions.empty())
    return scoped_ptr<ExtensionInstallPrompt::Prompt>();

  return make_scoped_ptr(new ExtensionInstallPrompt::Prompt(
      ExtensionInstallPrompt::LAUNCH_PROMPT));
}

bool EphemeralAppLauncher::CheckInlineInstallPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  *error = "";
  return true;
}

bool EphemeralAppLauncher::CheckRequestorPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  *error = "";
  return true;
}

bool EphemeralAppLauncher::CheckInstallValid(
    const base::DictionaryValue& manifest,
    std::string* error) {
  extension_ = Extension::Create(
      base::FilePath(),
      extensions::Manifest::INTERNAL,
      manifest,
      Extension::REQUIRE_KEY |
          Extension::FROM_WEBSTORE |
          Extension::IS_EPHEMERAL,
      id(),
      error);
  if (!extension_.get()) {
    *error = kInvalidManifestError;
    return false;
  }

  if (!extension_->is_app()) {
    *error = kExtensionTypeError;
    return false;
  }

  return true;
}

scoped_ptr<ExtensionInstallPrompt>
EphemeralAppLauncher::CreateInstallUI() {
  if (web_contents())
    return make_scoped_ptr(new ExtensionInstallPrompt(web_contents()));

  return make_scoped_ptr(
      new ExtensionInstallPrompt(profile(), parent_window_, NULL));
}

scoped_ptr<WebstoreInstaller::Approval>
EphemeralAppLauncher::CreateApproval() const {
  scoped_ptr<WebstoreInstaller::Approval> approval =
      WebstoreStandaloneInstaller::CreateApproval();
  approval->is_ephemeral = true;
  return approval.Pass();
}

void EphemeralAppLauncher::CompleteInstall(const std::string& error) {
  if (!error.empty())
    WebstoreStandaloneInstaller::CompleteInstall(error);

  // If the installation succeeds, we reach this point as a result of
  // chrome::NOTIFICATION_EXTENSION_INSTALLED, but this is broadcasted before
  // ExtensionService has added the extension to its list of installed
  // extensions and is too early to launch the app. Instead, we will launch at
  // chrome::NOTIFICATION_EXTENSION_LOADED.
  // TODO(tmdiep): Refactor extensions/WebstoreInstaller or
  // WebstoreStandaloneInstaller to support this cleanly.
}

void EphemeralAppLauncher::WebContentsDestroyed(
    content::WebContents* web_contents) {
  AbortInstall();
}

void EphemeralAppLauncher::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const extensions::Extension* extension =
          content::Details<const extensions::Extension>(details).ptr();
      DCHECK(extension);
      if (extension->id() == id()) {
        LaunchApp(extension);
        WebstoreStandaloneInstaller::CompleteInstall(std::string());
      }
      break;
    }

    default:
      NOTREACHED();
  }
}

void EphemeralAppLauncher::ExtensionEnableFlowFinished() {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  DCHECK(extension_service);

  const Extension* extension = extension_service->GetExtensionById(id(), false);
  if (extension) {
    LaunchApp(extension);
    WebstoreStandaloneInstaller::CompleteInstall(std::string());
  } else {
    WebstoreStandaloneInstaller::CompleteInstall(kLaunchAbortedError);
  }
}

void EphemeralAppLauncher::ExtensionEnableFlowAborted(bool user_initiated) {
  WebstoreStandaloneInstaller::CompleteInstall(kLaunchAbortedError);
}
