// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

ExtensionWebUIOverrideRegistrar::ExtensionWebUIOverrideRegistrar(
    Profile* profile) : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

ExtensionWebUIOverrideRegistrar::~ExtensionWebUIOverrideRegistrar() {
}

void ExtensionWebUIOverrideRegistrar::Observe(
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

static base::LazyInstance<
  ProfileKeyedAPIFactory<ExtensionWebUIOverrideRegistrar> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<ExtensionWebUIOverrideRegistrar>*
ExtensionWebUIOverrideRegistrar::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
