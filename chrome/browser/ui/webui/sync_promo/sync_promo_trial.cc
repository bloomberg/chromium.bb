// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo/sync_promo_trial.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/string_util.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace {

const char kLayoutExperimentTrialName[] = "SyncPromoLayoutExperiment";

enum LayoutExperimentType {
  LAYOUT_EXPERIMENT_DEFAULT = 0,
  LAYOUT_EXPERIMENT_DEVICES,
  LAYOUT_EXPERIMENT_VERBOSE,
  LAYOUT_EXPERIMENT_SIMPLE,
  LAYOUT_EXPERIMENT_NONE,
  LAYOUT_EXPERIMENT_DIALOG,
  LAYOUT_EXPERIMENT_BOUNDARY,
};

// Flag to make sure sync_promo_trial::Activate() has been called.
bool sync_promo_trial_initialized;

// Checks if a sync promo layout experiment is active. If it is active then the
// layout type is return in |type|.
bool GetActiveLayoutExperiment(LayoutExperimentType* type) {
  DCHECK(type);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSyncPromoVersion))
    return false;

  if (chrome::VersionInfo::GetChannel() ==
      chrome::VersionInfo::CHANNEL_STABLE) {
    std::string brand;
    if (!google_util::GetBrand(&brand))
      return false;

    if (brand == "GGRG" || brand == "CHCG")
      *type = LAYOUT_EXPERIMENT_DEFAULT;
    else if (brand == "GGRH" || brand == "CHCH")
      *type = LAYOUT_EXPERIMENT_DEVICES;
    else if (brand == "GGRI" || brand == "CHCI")
      *type = LAYOUT_EXPERIMENT_VERBOSE;
    else if (brand == "GGRJ" || brand == "CHCJ")
      *type = LAYOUT_EXPERIMENT_SIMPLE;
    else if (brand == "GGRL" || brand == "CHCL")
      *type = LAYOUT_EXPERIMENT_NONE;
    else if (brand == "GGRK" || brand == "CHCK")
      *type = LAYOUT_EXPERIMENT_DIALOG;
    else
      return false;
  } else {
    if (!base::FieldTrialList::TrialExists(kLayoutExperimentTrialName))
      return false;
    int value = base::FieldTrialList::FindValue(kLayoutExperimentTrialName) -
        base::FieldTrial::kDefaultGroupNumber;
    *type = static_cast<LayoutExperimentType>(value);
  }

  return true;
}

} // namespace

namespace sync_promo_trial {

void Activate() {
  DCHECK(!sync_promo_trial_initialized);
  sync_promo_trial_initialized = true;

  // For stable builds we'll use brand codes to enroll uesrs into experiments.
  // For dev and beta we don't have brand codes so we randomly enroll users.
  if (chrome::VersionInfo::GetChannel() !=
      chrome::VersionInfo::CHANNEL_STABLE) {
    // Create a field trial that expires in August 8, 2012. It contains 6 groups
    // with each group having an equal chance of enrollment.
    scoped_refptr<base::FieldTrial> trial(new base::FieldTrial(
        kLayoutExperimentTrialName, 5, "default", 2012, 8, 1));
    if (base::FieldTrialList::IsOneTimeRandomizationEnabled())
      trial->UseOneTimeRandomization();
    trial->AppendGroup("", 1);
    trial->AppendGroup("", 1);
    trial->AppendGroup("", 1);
    trial->AppendGroup("", 1);
    trial->AppendGroup("", 1);
  }
}

StartupOverride GetStartupOverrideForCurrentTrial() {
  DCHECK(sync_promo_trial_initialized);

  LayoutExperimentType type;
  if (GetActiveLayoutExperiment(&type)) {
    return type == LAYOUT_EXPERIMENT_NONE ? STARTUP_OVERRIDE_HIDE :
                                            STARTUP_OVERRIDE_SHOW;
  }
  return STARTUP_OVERRIDE_NONE;
}

void RecordUserShownPromo(content::WebUI* web_ui) {
  DCHECK(sync_promo_trial_initialized);

  LayoutExperimentType type;
  if (GetActiveLayoutExperiment(&type)) {
    bool is_at_startup = SyncPromoUI::GetIsLaunchPageForSyncPromoURL(
        web_ui->GetWebContents()->GetURL());
    if (is_at_startup) {
      DCHECK(SyncPromoUI::HasShownPromoAtStartup(Profile::FromWebUI(web_ui)));
      UMA_HISTOGRAM_ENUMERATION("SyncPromo.ShownPromoWithLayoutExpAtStartup",
                                type, LAYOUT_EXPERIMENT_BOUNDARY);
    } else {
      UMA_HISTOGRAM_ENUMERATION("SyncPromo.ShownPromoWithLayoutExp",
                                type, LAYOUT_EXPERIMENT_BOUNDARY);
    }
  }
}

void RecordUserSignedIn(content::WebUI* web_ui) {
  DCHECK(sync_promo_trial_initialized);

  LayoutExperimentType type;
  if (GetActiveLayoutExperiment(&type)) {
    bool is_at_startup = SyncPromoUI::GetIsLaunchPageForSyncPromoURL(
        web_ui->GetWebContents()->GetURL());
    if (is_at_startup) {
      DCHECK(SyncPromoUI::HasShownPromoAtStartup(Profile::FromWebUI(web_ui)));
      UMA_HISTOGRAM_ENUMERATION("SyncPromo.SignedInWithLayoutExpAtStartup",
                                type, LAYOUT_EXPERIMENT_BOUNDARY);
    } else {
      UMA_HISTOGRAM_ENUMERATION("SyncPromo.SignedInWithLayoutExp",
                                type, LAYOUT_EXPERIMENT_BOUNDARY);
    }
  }
}

bool GetSyncPromoVersionForCurrentTrial(SyncPromoUI::Version* version) {
  DCHECK(sync_promo_trial_initialized);
  DCHECK(version);

  LayoutExperimentType type;
  if (!GetActiveLayoutExperiment(&type))
    return false;

  switch (type) {
    case LAYOUT_EXPERIMENT_DEFAULT:
      *version = SyncPromoUI::VERSION_DEFAULT;
      return true;
    case LAYOUT_EXPERIMENT_DEVICES:
      *version = SyncPromoUI::VERSION_DEVICES;
      return true;
    case LAYOUT_EXPERIMENT_VERBOSE:
      *version = SyncPromoUI::VERSION_VERBOSE;
      return true;
    case LAYOUT_EXPERIMENT_SIMPLE:
      *version = SyncPromoUI::VERSION_SIMPLE;
      return true;
    case LAYOUT_EXPERIMENT_DIALOG:
      *version = SyncPromoUI::VERSION_DIALOG;
      return true;
    default:
      return false;
  }
}

}  // namespace sync_promo_trial
