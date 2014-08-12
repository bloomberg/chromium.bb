// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_component_loader.h"

#include "base/command_line.h"
#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/signin/core/browser/signin_manager.h"

// TODO(thestig): Remove after extensions are disabled on mobile.
#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/search/hotword_service_factory.h"
#endif

namespace {

bool IsUserSignedin(Profile* profile) {
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
  return signin && !signin->GetAuthenticatedUsername().empty();
}

}  // namespace

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

#if defined(ENABLE_EXTENSIONS)
  if (HotwordServiceFactory::IsHotwordAllowed(profile_)) {
    std::string hotwordId = extension_misc::kHotwordExtensionId;
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    // TODO(amistry): Load the hotword shared module when enabling built-in
    // hotword detection.
    if (!command_line->HasSwitch(switches::kEnableExperimentalHotwording)) {
      prefs_->SetString(hotwordId + ".external_update_url",
                        extension_urls::GetWebstoreUpdateUrl().spec());
    }
  }
#endif

  UpdateBookmarksExperimentState(
      profile_->GetPrefs(),
      g_browser_process->local_state(),
      IsUserSignedin(profile_),
      BOOKMARKS_EXPERIMENT_ENABLED_FROM_SYNC_UNKNOWN);
  std::string ext_id;
  if (GetBookmarksExperimentExtensionID(profile_->GetPrefs(), &ext_id) &&
      !ext_id.empty()) {
    prefs_->SetString(ext_id + ".external_update_url",
                      extension_urls::GetWebstoreUpdateUrl().spec());
  }

  LoadFinished();
}

}  // namespace extensions
