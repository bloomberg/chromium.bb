// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/ephemeral_app_launcher.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

using content::WebContents;
using extensions::Extension;
using extensions::ExtensionSystem;
using extensions::WebstoreInstaller;

namespace {

const char kExtensionTypeError[] = "Ephemeral extensions are not permitted";

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
    LaunchApp(extension);
    return;
  }

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
  Init();
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
  Init();
}

EphemeralAppLauncher::~EphemeralAppLauncher() {}

void EphemeralAppLauncher::Init() {
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
  extensions::Manifest extension_manifest(
      extensions::Manifest::INTERNAL,
      scoped_ptr<DictionaryValue>(manifest.DeepCopy()));
  if (!extension_manifest.is_app()) {
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
