// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_message_bubble_factory.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/extensions/dev_mode_bubble_controller.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/proxy_overridden_bubble_controller.h"
#include "chrome/browser/extensions/settings_api_bubble_controller.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "extensions/common/feature_switch.h"

namespace {

// A map of all profiles evaluated, so we can tell if it's the initial check.
// TODO(devlin): It would be nice to coalesce all the "profiles evaluated" maps
// that are in the different bubble controllers.
base::LazyInstance<std::set<Profile*> > g_profiles_evaluated =
    LAZY_INSTANCE_INITIALIZER;

// This is used to turn on override whether bubbles are enabled or disabled for
// testing.
ExtensionMessageBubbleFactory::OverrideForTesting g_override_for_testing =
    ExtensionMessageBubbleFactory::NO_OVERRIDE;

const char kEnableDevModeWarningExperimentName[] =
    "ExtensionDeveloperModeWarning";

#if !defined(OS_WIN)
const char kEnableProxyWarningExperimentName[] = "ExtensionProxyWarning";
#endif

bool IsExperimentEnabled(const char* experiment_name) {
  // Don't allow turning it off via command line.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceFieldTrials)) {
    std::string forced_trials =
        command_line->GetSwitchValueASCII(switches::kForceFieldTrials);
    if (forced_trials.find(experiment_name))
      return true;
  }
  return base::FieldTrialList::FindFullName(experiment_name) == "Enabled";
}

bool EnableSuspiciousExtensionsBubble() {
  return g_override_for_testing ==
             ExtensionMessageBubbleFactory::OVERRIDE_ENABLED ||
         extensions::InstallVerifier::ShouldEnforce();
}

bool EnableSettingsApiBubble() {
#if defined(OS_WIN)
  return true;
#else
  return g_override_for_testing ==
         ExtensionMessageBubbleFactory::OVERRIDE_ENABLED;
#endif
}

bool EnableProxyOverrideBubble() {
#if defined(OS_WIN)
  return true;
#else
  return g_override_for_testing ==
             ExtensionMessageBubbleFactory::OVERRIDE_ENABLED ||
         IsExperimentEnabled(kEnableProxyWarningExperimentName);
#endif
}

bool EnableDevModeBubble() {
  if (extensions::FeatureSwitch::force_dev_mode_highlighting()->IsEnabled())
    return true;

#if defined(OS_WIN)
  if (chrome::GetChannel() >= version_info::Channel::BETA)
    return true;
#endif

  return g_override_for_testing ==
             ExtensionMessageBubbleFactory::OVERRIDE_ENABLED ||
         IsExperimentEnabled(kEnableDevModeWarningExperimentName);
}

}  // namespace

ExtensionMessageBubbleFactory::ExtensionMessageBubbleFactory(Browser* browser)
    : browser_(browser) {
}

ExtensionMessageBubbleFactory::~ExtensionMessageBubbleFactory() {
}

scoped_ptr<extensions::ExtensionMessageBubbleController>
ExtensionMessageBubbleFactory::GetController() {
  Profile* original_profile = browser_->profile()->GetOriginalProfile();
  std::set<Profile*>& profiles_evaluated = g_profiles_evaluated.Get();
  bool is_initial_check = profiles_evaluated.count(original_profile) == 0;
  profiles_evaluated.insert(original_profile);

  if (g_override_for_testing == OVERRIDE_DISABLED)
    return scoped_ptr<extensions::ExtensionMessageBubbleController>();

  // The list of suspicious extensions takes priority over the dev mode bubble
  // and the settings API bubble, since that needs to be shown as soon as we
  // disable something. The settings API bubble is shown on first startup after
  // an extension has changed the startup pages and it is acceptable if that
  // waits until the next startup because of the suspicious extension bubble.
  // The dev mode bubble is not time sensitive like the other two so we'll catch
  // the dev mode extensions on the next startup/next window that opens. That
  // way, we're not too spammy with the bubbles.
  if (EnableSuspiciousExtensionsBubble()) {
    scoped_ptr<extensions::SuspiciousExtensionBubbleController> controller(
        new extensions::SuspiciousExtensionBubbleController(browser_));
    if (controller->ShouldShow())
      return controller.Pass();
  }

  if (EnableSettingsApiBubble()) {
    // No use showing this if it's not the startup of the profile.
    if (is_initial_check) {
      scoped_ptr<extensions::SettingsApiBubbleController> controller(
          new extensions::SettingsApiBubbleController(
              browser_, extensions::BUBBLE_TYPE_STARTUP_PAGES));
      if (controller->ShouldShow())
        return controller.Pass();
    }
  }

  if (EnableProxyOverrideBubble()) {
    // TODO(devlin): Move the "GetExtensionOverridingProxy" part into the
    // proxy bubble controller.
    const extensions::Extension* extension =
        extensions::GetExtensionOverridingProxy(browser_->profile());
    if (extension) {
      scoped_ptr<extensions::ProxyOverriddenBubbleController> controller(
         new extensions::ProxyOverriddenBubbleController(browser_));
      if (controller->ShouldShow(extension->id()))
        return controller.Pass();
    }
  }

  if (EnableDevModeBubble()) {
    scoped_ptr<extensions::DevModeBubbleController> controller(
        new extensions::DevModeBubbleController(browser_));
    if (controller->ShouldShow())
      return controller.Pass();
  }

  return scoped_ptr<extensions::ExtensionMessageBubbleController>();
}

// static
void ExtensionMessageBubbleFactory::set_override_for_tests(
    OverrideForTesting override) {
  g_override_for_testing = override;
}
