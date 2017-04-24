// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/extension_tab_util_delegate_chromeos.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/extensions/api/tabs.h"
#include "url/gurl.h"

namespace extensions {

ExtensionTabUtilDelegateChromeOS::ExtensionTabUtilDelegateChromeOS() {}

ExtensionTabUtilDelegateChromeOS::~ExtensionTabUtilDelegateChromeOS() {}

void ExtensionTabUtilDelegateChromeOS::ScrubTabForExtension(
    const Extension* extension,
    content::WebContents* contents,
    api::tabs::Tab* tab) {
  if (!profiles::IsPublicSession() || !tab->url) {
    return;
  }
  // Scrub URL down to the origin (security reasons inside Public Sessions).
  tab->url = base::MakeUnique<std::string>(GURL(*tab->url).GetOrigin().spec());
}

}  // namespace extensions
