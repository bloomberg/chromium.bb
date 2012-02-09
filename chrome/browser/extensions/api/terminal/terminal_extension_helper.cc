// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"

namespace {

const char kCroshExtensionEntryPoint[] = "/html/crosh.html";

const Extension* GetTerminalExtension(Profile* profile) {
  static const char* kPossibleAppIds[] = {
    extension_misc::kHTermAppId,
    extension_misc::kHTermDevAppId
  };

  // The production app should be first in the list.
  DCHECK_EQ(kPossibleAppIds[0], extension_misc::kHTermAppId);

  ExtensionService* service = profile->GetExtensionService();
  for (size_t x = 0; x < arraysize(kPossibleAppIds); ++x) {
    const Extension* extension = service->GetExtensionById(
        kPossibleAppIds[x], false);
    if (extension)
      return extension;
  }

  return NULL;
}

}  // namespace

GURL TerminalExtensionHelper::GetCroshExtensionURL(Profile* profile) {
  const Extension* extension = GetTerminalExtension(profile);
  if (!extension)
    return GURL();

  return extension->GetResourceURL(kCroshExtensionEntryPoint);
}
