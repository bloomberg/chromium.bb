// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pepper_permission_util.h"

#include <vector>

#include "base/command_line.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/manifest_handlers/shared_module_info.h"
#include "extensions/common/constants.h"

using extensions::Extension;
using extensions::Manifest;

namespace chrome {

namespace {

std::string HashHost(const std::string& host) {
  const std::string id_hash = base::SHA1HashString(host);
  DCHECK_EQ(id_hash.length(), base::kSHA1Length);
  return base::HexEncode(id_hash.c_str(), id_hash.length());
}

bool HostIsInSet(const std::string& host, const std::set<std::string>& set) {
  return set.count(host) > 0 || set.count(HashHost(host)) > 0;
}

}  // namespace

bool IsExtensionOrSharedModuleWhitelisted(
    Profile* profile,
    const GURL& url,
    const std::set<std::string>& whitelist,
    const char* command_line_switch) {
  if (!url.is_valid())
    return false;

  const std::string host = url.host();
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      HostIsInSet(host, whitelist)) {
    return true;
  }

  const Extension* extension = NULL;
  ExtensionService* extension_service = !profile ? NULL :
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (extension_service) {
    extension = extension_service->extensions()->
        GetExtensionOrAppByURL(ExtensionURLInfo(url));
  }

  // Check the modules that are imported by this extension to see if any of them
  // is whitelisted.
  if (extension) {
    typedef std::vector<extensions::SharedModuleInfo::ImportInfo>
        ImportInfoVector;
    const ImportInfoVector& imports =
        extensions::SharedModuleInfo::GetImports(extension);
    for (ImportInfoVector::const_iterator it = imports.begin();
         it != imports.end(); ++it) {
      const Extension* imported_extension = extension_service->
          GetExtensionById(it->extension_id, false);
      if (imported_extension &&
          extensions::SharedModuleInfo::IsSharedModule(imported_extension) &&
          HostIsInSet(it->extension_id, whitelist)) {
        return true;
      }
    }
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  const std::string allowed_list =
      command_line.GetSwitchValueASCII(command_line_switch);
  if (allowed_list == "*") {
    // The wildcard allows socket API only for packaged and platform apps.
    return extension &&
        (extension->GetType() == Manifest::TYPE_LEGACY_PACKAGED_APP ||
         extension->GetType() == Manifest::TYPE_PLATFORM_APP);
  }

  if (!allowed_list.empty()) {
    base::StringTokenizer t(allowed_list, ",");
    while (t.GetNext()) {
      if (t.token() == host)
        return true;
    }
  }

  return false;
}

}  // namespace chrome
