// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"

namespace {

const char kCroshExtensionEntryPoint[] = "/html/crosh.html";

const extensions::Extension* GetTerminalExtension(Profile* profile) {
  // Search order for terminal extensions.
  // We prefer hterm-dev, then hterm, then the builtin crosh extension.
  static const char* kPossibleAppIds[] = {
    extension_misc::kHTermDevAppId,
    extension_misc::kHTermAppId,
    extension_misc::kCroshBuiltinAppId,
  };

  // The hterm-dev should be first in the list.
  DCHECK_EQ(kPossibleAppIds[0], extension_misc::kHTermDevAppId);

  ExtensionService* service = profile->GetExtensionService();
  for (size_t x = 0; x < arraysize(kPossibleAppIds); ++x) {
    const extensions::Extension* extension = service->GetExtensionById(
        kPossibleAppIds[x], false);
    if (extension)
      return extension;
  }

  return NULL;
}

}  // namespace

GURL TerminalExtensionHelper::GetCroshExtensionURL(Profile* profile) {
  const extensions::Extension* extension = GetTerminalExtension(profile);
  if (!extension)
    return GURL();

  return extension->GetResourceURL(kCroshExtensionEntryPoint);
}
