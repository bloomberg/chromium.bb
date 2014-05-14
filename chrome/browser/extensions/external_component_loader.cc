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
#include "components/sync_driver/pref_names.h"
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

  BookmarksExperimentState bookmarks_experiment_state_before =
      static_cast<BookmarksExperimentState>(profile_->GetPrefs()->GetInteger(
          sync_driver::prefs::kEnhancedBookmarksExperimentEnabled));
  if (bookmarks_experiment_state_before == kBookmarksExperimentEnabled) {
    // kEnhancedBookmarksExperiment flag could have values "", "1" and "0".
    // "0" - user opted out.
    if (CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnhancedBookmarksExperiment) != "0") {
      // Experiment enabled.
      std::string ext_id = profile_->GetPrefs()->GetString(
          sync_driver::prefs::kEnhancedBookmarksExtensionId);
      if (!ext_id.empty()) {
        prefs_->SetString(ext_id + ".external_update_url",
                          extension_urls::GetWebstoreUpdateUrl().spec());
      }
    } else {
      // Experiment enabled but user opted out.
      profile_->GetPrefs()->SetInteger(
          sync_driver::prefs::kEnhancedBookmarksExperimentEnabled,
          kBookmarksExperimentEnabledUserOptOut);
    }
  } else if (bookmarks_experiment_state_before ==
             kBookmarksExperimentEnabledUserOptOut) {
    if (CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnhancedBookmarksExperiment) != "0") {
      // User opted in again.
      profile_->GetPrefs()->SetInteger(
          sync_driver::prefs::kEnhancedBookmarksExperimentEnabled,
          kBookmarksExperimentEnabled);
      std::string ext_id = profile_->GetPrefs()->GetString(
          sync_driver::prefs::kEnhancedBookmarksExtensionId);
      if (!ext_id.empty()) {
        prefs_->SetString(ext_id + ".external_update_url",
                          extension_urls::GetWebstoreUpdateUrl().spec());
      }
    }
  } else {
    // Experiment disabled.
    profile_->GetPrefs()->ClearPref(
        sync_driver::prefs::kEnhancedBookmarksExperimentEnabled);
    profile_->GetPrefs()->ClearPref(
        sync_driver::prefs::kEnhancedBookmarksExtensionId);
  }
  BookmarksExperimentState bookmarks_experiment_state =
      static_cast<BookmarksExperimentState>(profile_->GetPrefs()->GetInteger(
          sync_driver::prefs::kEnhancedBookmarksExperimentEnabled));
  if (bookmarks_experiment_state_before != bookmarks_experiment_state) {
    UpdateBookmarksExperiment(g_browser_process->local_state(),
                              bookmarks_experiment_state);
  }

  LoadFinished();
}

}  // namespace extensions
