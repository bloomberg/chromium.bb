// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_component_loader.h"

#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/signin/core/browser/signin_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#endif

namespace extensions {

ExternalComponentLoader::ExternalComponentLoader(Profile* profile)
    : profile_(profile) {
}

ExternalComponentLoader::~ExternalComponentLoader() {}

// static
bool ExternalComponentLoader::IsModifiable(const Extension* extension) {
  if (extension->location() == Manifest::EXTERNAL_COMPONENT) {
    static const char* const enhanced_extension_hashes[] = {
        "D5736E4B5CF695CB93A2FB57E4FDC6E5AFAB6FE2",  // http://crbug.com/312900
        "D57DE394F36DC1C3220E7604C575D29C51A6C495",  // http://crbug.com/319444
        "3F65507A3B39259B38C8173C6FFA3D12DF64CCE9"   // http://crbug.com/371562
    };
    std::string hash = base::SHA1HashString(extension->id());
    hash = base::HexEncode(hash.c_str(), hash.length());
    for (size_t i = 0; i < arraysize(enhanced_extension_hashes); i++)
      if (hash == enhanced_extension_hashes[i])
        return true;
  }
  return false;
}

void ExternalComponentLoader::StartLoading() {
  prefs_.reset(new base::DictionaryValue());
  std::string app_id = extension_misc::kInAppPaymentsSupportAppId;
  prefs_->SetString(app_id + ".external_update_url",
                    extension_urls::GetWebstoreUpdateUrl().spec());

  if (HotwordServiceFactory::IsHotwordAllowed(profile_)) {
    std::string hotword_id = extension_misc::kHotwordExtensionId;
    if (HotwordService::IsExperimentalHotwordingEnabled()) {
      hotword_id = extension_misc::kHotwordSharedModuleId;
    }
    prefs_->SetString(hotword_id + ".external_update_url",
                      extension_urls::GetWebstoreUpdateUrl().spec());
  }

  InitBookmarksExperimentState(profile_);

  {
    std::string extension_id;
    if (GetBookmarksExperimentExtensionID(profile_->GetPrefs(),
                                          &extension_id) &&
        !extension_id.empty()) {
      prefs_->SetString(extension_id + ".external_update_url",
                        extension_urls::GetWebstoreUpdateUrl().spec());
    }
  }

#if defined(OS_CHROMEOS)
  {
    base::CommandLine* const command_line =
        base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(chromeos::switches::kDisableNewZIPUnpacker)) {
      const std::string extension_id = extension_misc::kZIPUnpackerExtensionId;
      prefs_->SetString(extension_id + ".external_update_url",
                        extension_urls::GetWebstoreUpdateUrl().spec());
    }
  }
#endif

  LoadFinished();
}

}  // namespace extensions
