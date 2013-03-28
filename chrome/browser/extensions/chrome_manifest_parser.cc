// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_manifest_parser.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/manifest_handlers/offline_enabled_info.h"
#include "chrome/common/extensions/manifest_handlers/requirements_handler.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

ChromeManifestParser::ChromeManifestParser(Profile* profile)
    : profile_(profile) {
  (new DevToolsPageHandler)->Register();
  (new HomepageURLHandler)->Register();
  (new UpdateURLHandler)->Register();
  (new OptionsPageHandler)->Register();
  (new URLOverridesHandler)->Register();
  (new RequirementsHandler)->Register();
  (new OfflineEnabledHandler)->Register();

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

ChromeManifestParser::~ChromeManifestParser() {
}

void ChromeManifestParser::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    const Extension* extension =
        content::Details<const Extension>(details).ptr();
    ExtensionWebUI::RegisterChromeURLOverrides(
        profile_, URLOverrides::GetChromeURLOverrides(extension));

  } else if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    const Extension* extension =
        content::Details<const UnloadedExtensionInfo>(details)->extension;
    ExtensionWebUI::UnregisterChromeURLOverrides(
        profile_, URLOverrides::GetChromeURLOverrides(extension));
  }
}

static base::LazyInstance<ProfileKeyedAPIFactory<ChromeManifestParser> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<ChromeManifestParser>*
ChromeManifestParser::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
