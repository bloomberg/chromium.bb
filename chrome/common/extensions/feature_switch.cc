// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/feature_switch.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

namespace {

class CommonSwitches {
 public:
  CommonSwitches()
      : action_box(
            switches::kActionBox,
            FeatureSwitch::DEFAULT_DISABLED),
        easy_off_store_install(
            switches::kEasyOffStoreExtensionInstall,
            FeatureSwitch::DEFAULT_DISABLED),
        script_badges(
            switches::kScriptBadges,
            FeatureSwitch::DEFAULT_DISABLED),
        script_bubble(
            switches::kScriptBubble,
            FeatureSwitch::DEFAULT_DISABLED),
        sideload_wipeout(
            switches::kSideloadWipeout,
            base::FieldTrialList::FindFullName("SideloadWipeout") == "Enabled" ?
                FeatureSwitch::DEFAULT_ENABLED :
                FeatureSwitch::DEFAULT_DISABLED),
        prompt_for_external_extensions(
            switches::kPromptForExternalExtensions,
#if defined(OS_WIN)
            FeatureSwitch::DEFAULT_ENABLED),
#else
            FeatureSwitch::DEFAULT_DISABLED),
#endif
        tab_capture(
            switches::kTabCapture,
            FeatureSwitch::DEFAULT_ENABLED) {}

  FeatureSwitch action_box;
  FeatureSwitch easy_off_store_install;
  FeatureSwitch script_badges;
  FeatureSwitch script_bubble;
  FeatureSwitch sideload_wipeout;
  FeatureSwitch prompt_for_external_extensions;
  FeatureSwitch tab_capture;
};

base::LazyInstance<CommonSwitches> g_common_switches =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace


FeatureSwitch* FeatureSwitch::action_box() {
  return &g_common_switches.Get().action_box;
}
FeatureSwitch* FeatureSwitch::easy_off_store_install() {
  return &g_common_switches.Get().easy_off_store_install;
}
FeatureSwitch* FeatureSwitch::script_badges() {
  return &g_common_switches.Get().script_badges;
}
FeatureSwitch* FeatureSwitch::script_bubble() {
  return &g_common_switches.Get().script_bubble;
}
FeatureSwitch* FeatureSwitch::sideload_wipeout() {
  return &g_common_switches.Get().sideload_wipeout;
}
FeatureSwitch* FeatureSwitch::prompt_for_external_extensions() {
  return &g_common_switches.Get().prompt_for_external_extensions;
}
FeatureSwitch* FeatureSwitch::tab_capture() {
  return &g_common_switches.Get().tab_capture;
}


FeatureSwitch::ScopedOverride::ScopedOverride(FeatureSwitch* feature,
                                              bool override_value)
    : feature_(feature),
      previous_value_(feature->GetOverrideValue()) {
  feature_->SetOverrideValue(
      override_value ? OVERRIDE_ENABLED : OVERRIDE_DISABLED);
}

FeatureSwitch::ScopedOverride::~ScopedOverride() {
  feature_->SetOverrideValue(previous_value_);
}

FeatureSwitch::FeatureSwitch(const char* switch_name,
                             DefaultValue default_value) {
  Init(CommandLine::ForCurrentProcess(), switch_name, default_value);
}

FeatureSwitch::FeatureSwitch(const CommandLine* command_line,
                             const char* switch_name,
                             DefaultValue default_value) {
  Init(command_line, switch_name, default_value);
}

void FeatureSwitch::Init(const CommandLine* command_line,
                         const char* switch_name,
                         DefaultValue default_value) {
  command_line_ = command_line;
  switch_name_ = switch_name;
  default_value_ = default_value == DEFAULT_ENABLED;
  override_value_ = OVERRIDE_NONE;
}

bool FeatureSwitch::IsEnabled() const {
  if (override_value_ != OVERRIDE_NONE)
    return override_value_ == OVERRIDE_ENABLED;

  std::string temp = command_line_->GetSwitchValueASCII(switch_name_);
  std::string switch_value;
  TrimWhitespaceASCII(temp, TRIM_ALL, &switch_value);

  if (switch_value == "1")
    return true;

  if (switch_value == "0")
    return false;

  if (!default_value_ && command_line_->HasSwitch(GetLegacyEnableFlag()))
    return true;

  if (default_value_ && command_line_->HasSwitch(GetLegacyDisableFlag()))
    return false;

  return default_value_;
}

std::string FeatureSwitch::GetLegacyEnableFlag() const {
  return std::string("enable-") + switch_name_;
}

std::string FeatureSwitch::GetLegacyDisableFlag() const {
  return std::string("disable-") + switch_name_;
}

void FeatureSwitch::SetOverrideValue(OverrideValue override_value) {
  override_value_ = override_value;
}

FeatureSwitch::OverrideValue FeatureSwitch::GetOverrideValue() const {
  return override_value_;
}

}  // namespace extensions
