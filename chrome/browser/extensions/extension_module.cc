// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_module.h"

#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"

namespace {

const char kOnInstalledEvent[] = "experimental.extension.onInstalled";

}

// static
void ExtensionModuleEventRouter::DispatchOnInstalledEvent(
    Profile* profile, const Extension* extension) {
  // Special case: normally, extensions add their own lazy event listeners.
  // However, since the extension has just been installed, it hasn't had a
  // chance to register for events. So we register on its behalf. If the
  // extension does not actually have a listener, the event will just be
  // ignored.
  ExtensionEventRouter* router = profile->GetExtensionEventRouter();
  router->AddLazyEventListener(kOnInstalledEvent, extension->id());
  router->DispatchEventToExtension(
      extension->id(), kOnInstalledEvent, "[]", NULL, GURL());
}

ExtensionPrefs* SetUpdateUrlDataFunction::extension_prefs() {
  return profile()->GetExtensionService()->extension_prefs();
}

bool SetUpdateUrlDataFunction::RunImpl() {
  std::string data;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &data));

  extension_prefs()->SetUpdateUrlData(extension_id(), data);
  return true;
}

bool IsAllowedIncognitoAccessFunction::RunImpl() {
  ExtensionService* ext_service = profile()->GetExtensionService();
  const Extension* extension = GetExtension();

  result_.reset(Value::CreateBooleanValue(
      ext_service->IsIncognitoEnabled(extension->id())));
  return true;
}

bool IsAllowedFileSchemeAccessFunction::RunImpl() {
  ExtensionService* ext_service = profile()->GetExtensionService();
  const Extension* extension = GetExtension();

  result_.reset(Value::CreateBooleanValue(
        ext_service->AllowFileAccess(extension)));
  return true;
}
