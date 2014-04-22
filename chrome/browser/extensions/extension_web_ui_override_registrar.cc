// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

ExtensionWebUIOverrideRegistrar::ExtensionWebUIOverrideRegistrar(
    content::BrowserContext* context)
    : extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(context));
}

ExtensionWebUIOverrideRegistrar::~ExtensionWebUIOverrideRegistrar() {
}

void ExtensionWebUIOverrideRegistrar::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  ExtensionWebUI::RegisterChromeURLOverrides(
      Profile::FromBrowserContext(browser_context),
      URLOverrides::GetChromeURLOverrides(extension));
}

void ExtensionWebUIOverrideRegistrar::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  ExtensionWebUI::UnregisterChromeURLOverrides(
      Profile::FromBrowserContext(browser_context),
      URLOverrides::GetChromeURLOverrides(extension));
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ExtensionWebUIOverrideRegistrar> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ExtensionWebUIOverrideRegistrar>*
ExtensionWebUIOverrideRegistrar::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
