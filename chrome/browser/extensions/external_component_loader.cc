// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_component_loader.h"

#include "base/command_line.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

ExternalComponentLoader::ExternalComponentLoader(Profile* profile)
    : profile_(profile) {
}

ExternalComponentLoader::~ExternalComponentLoader() {}

void ExternalComponentLoader::StartLoading() {
  prefs_.reset(new base::DictionaryValue());
  std::string appId = extension_misc::kInAppPaymentsSupportAppId;
  prefs_->SetString(appId + ".external_update_url",
                    extension_urls::GetWebstoreUpdateUrl().spec());

  if (HotwordServiceFactory::IsHotwordAllowed(profile_)) {
    std::string hotwordId = extension_misc::kHotwordExtensionId;
    prefs_->SetString(hotwordId + ".external_update_url",
                      extension_urls::GetWebstoreUpdateUrl().spec());
  }

  if (CommandLine::ForCurrentProcess()->
          GetSwitchValueASCII(switches::kEnableEnhancedBookmarks) != "0") {
    std::string ext_id;
    if (profile_->GetPrefs()->GetBoolean(
            prefs::kEnhancedBookmarksExperimentEnabled)) {
      ext_id =
          profile_->GetPrefs()->GetString(prefs::kEnhancedBookmarksExtensionId);
    } else {
      ext_id = GetEnhancedBookmarksExtensionIdFromFinch();
    }
    if (!ext_id.empty()) {
      prefs_->SetString(ext_id + ".external_update_url",
                        extension_urls::GetWebstoreUpdateUrl().spec());
    }
  } else {
    profile_->GetPrefs()->ClearPref(prefs::kEnhancedBookmarksExperimentEnabled);
    profile_->GetPrefs()->ClearPref(prefs::kEnhancedBookmarksExtensionId);
  }
  LoadFinished();
}

}  // namespace extensions
