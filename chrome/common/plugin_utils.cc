// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/plugin_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/common/webplugininfo.h"
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#endif

bool ShouldUseJavaScriptSettingForPlugin(const content::WebPluginInfo& plugin) {
  if (plugin.type != content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS &&
      plugin.type !=
          content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS) {
    return false;
  }

#if !defined(DISABLE_NACL)
  // Treat Native Client invocations like JavaScript.
  if (plugin.name == base::ASCIIToUTF16(nacl::kNaClPluginName))
    return true;
#endif

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)
  // Treat CDM invocations like JavaScript.
  if (plugin.name == base::ASCIIToUTF16(kWidevineCdmDisplayName)) {
    DCHECK_EQ(content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS,
              plugin.type);
    return true;
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)

  return false;
}
