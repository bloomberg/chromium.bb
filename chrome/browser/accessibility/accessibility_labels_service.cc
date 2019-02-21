// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_labels_service.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_action_data.h"

AccessibilityLabelsService::~AccessibilityLabelsService() {}

// static
void AccessibilityLabelsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAccessibilityImageLabelsEnabled, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityImageLabelsOptInAccepted,
                                false);
}

void AccessibilityLabelsService::Init() {
  // Hidden behind a feature flag.
  base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  if (!cmd.HasSwitch(::switches::kEnableExperimentalAccessibilityLabels))
    return;

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kAccessibilityImageLabelsEnabled,
      base::BindRepeating(
          &AccessibilityLabelsService::OnImageLabelsEnabledChanged,
          weak_factory_.GetWeakPtr()));

  // Log whether the feature is enabled after startup.
  content::BrowserAccessibilityState::GetInstance()->AddHistogramCallback(
      base::BindRepeating(
          &AccessibilityLabelsService::UpdateAccessibilityLabelsHistograms,
          weak_factory_.GetWeakPtr()));
}

AccessibilityLabelsService::AccessibilityLabelsService(Profile* profile)
    : profile_(profile), weak_factory_(this) {}

ui::AXMode AccessibilityLabelsService::GetAXMode() {
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();

  // Hidden behind a feature flag.
  base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  if (cmd.HasSwitch(::switches::kEnableExperimentalAccessibilityLabels)) {
    bool enabled = profile_->GetPrefs()->GetBoolean(
        prefs::kAccessibilityImageLabelsEnabled);
    ax_mode.set_mode(ui::AXMode::kLabelImages, enabled);
  }

  return ax_mode;
}

void AccessibilityLabelsService::EnableLabelsServiceOnce() {
  // TODO(crbug.com/905419): Implement for Android, which does not support
  // BrowserList::GetInstance.
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (!browser)
    return;
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  // Fire an AXAction on the active tab to enable this feature once only.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kAnnotatePageImages;
  web_contents->GetMainFrame()->AccessibilityPerformAction(action_data);
  // TODO(katie): Support iframes by doing this to all RenderFrameHosts within
  // the web_contents.
#endif
}

void AccessibilityLabelsService::OnImageLabelsEnabledChanged() {
  // TODO(dmazzoni) Implement for Android, which doesn't support
  // AllTabContentses(). crbug.com/905419
#if !defined(OS_ANDROID)
  bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kAccessibilityImageLabelsEnabled);

  for (auto* web_contents : AllTabContentses()) {
    if (web_contents->GetBrowserContext() != profile_)
      continue;

    ui::AXMode ax_mode = web_contents->GetAccessibilityMode();
    ax_mode.set_mode(ui::AXMode::kLabelImages, enabled);
    web_contents->SetAccessibilityMode(ax_mode);
  }
#endif
}

void AccessibilityLabelsService::UpdateAccessibilityLabelsHistograms() {
  base::UmaHistogramBoolean("Accessibility.ImageLabels",
                            profile_->GetPrefs()->GetBoolean(
                                prefs::kAccessibilityImageLabelsEnabled));
}
