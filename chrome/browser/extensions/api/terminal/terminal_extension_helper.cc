// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"

namespace {

const char* kAllowedExtensionIds[] = {
    // Keep official app first, so GetTerminalExtensionID checks it first.
    "pnhechapfaindjhompbnflcldabbghjo",  // HTerm App
    "okddffdblfhhnmhodogpojmfkjmhinfp",  // test SSH/Crosh Client
};

const char kExtensionSchema[] = "chrome-extension://";
const char kCroshExtensionEntryPoint[] = "/html/crosh.html";

}  // namespace

// Allow component and whitelisted extensions.
bool TerminalExtensionHelper::AllowAccessToExtension(
    Profile* profile,
    const std::string& extension_id) {
  ExtensionService* service = profile->GetExtensionService();
  const Extension* extension = service->GetExtensionById(extension_id, false);

  if (!extension)
    return false;

  if (extension->location() == Extension::COMPONENT)
    return true;

  for (size_t i = 0; i < arraysize(kAllowedExtensionIds); i++) {
    if (extension->id() == kAllowedExtensionIds[i])
      return true;
  }
  return false;
}

GURL TerminalExtensionHelper::GetCroshExtensionURL(Profile* profile) {
  const char* extension_id = GetTerminalExtensionId(profile);
  if (!extension_id)
    return GURL();

  std::string crosh_url_str(kExtensionSchema);
  crosh_url_str.append(extension_id);
  crosh_url_str.append(kCroshExtensionEntryPoint);
  return GURL(crosh_url_str);
}

const char* TerminalExtensionHelper::GetTerminalExtensionId(Profile* profile) {
  ExtensionService* service = profile->GetExtensionService();
  for (size_t i = 0; i < arraysize(kAllowedExtensionIds); i++) {
    if (service->GetExtensionById(kAllowedExtensionIds[i], false) != 0)
      return kAllowedExtensionIds[i];
  }
  return NULL;
}
