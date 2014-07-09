// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/feature_switch.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

class CommonSwitches {
 public:
  CommonSwitches()
      : easy_off_store_install(
            NULL,
            FeatureSwitch::DEFAULT_DISABLED),
        force_dev_mode_highlighting(
            switches::kForceDevModeHighlighting,
            FeatureSwitch::DEFAULT_DISABLED),
        prompt_for_external_extensions(
            NULL,
#if defined(OS_WIN)
            FeatureSwitch::DEFAULT_ENABLED),
#else
            FeatureSwitch::DEFAULT_DISABLED),
#endif
        error_console(
            switches::kErrorConsole,
            FeatureSwitch::DEFAULT_DISABLED),
        enable_override_bookmarks_ui(
            switches::kEnableOverrideBookmarksUI,
            FeatureSwitch::DEFAULT_DISABLED),
        extension_action_redesign(
            switches::kExtensionActionRedesign,
            FeatureSwitch::DEFAULT_DISABLED),
        scripts_require_action(switches::kScriptsRequireAction,
                               FeatureSwitch::DEFAULT_DISABLED),
        embedded_extension_options(
            switches::kEmbeddedExtensionOptions,
            FeatureSwitch::DEFAULT_DISABLED) {}

  // Enables extensions to be easily installed from sites other than the web
  // store.
  FeatureSwitch easy_off_store_install;

  FeatureSwitch force_dev_mode_highlighting;

  // Should we prompt the user before allowing external extensions to install?
  // Default is yes.
  FeatureSwitch prompt_for_external_extensions;

  FeatureSwitch error_console;
  FeatureSwitch enable_override_bookmarks_ui;
  FeatureSwitch extension_action_redesign;
  FeatureSwitch scripts_require_action;
  FeatureSwitch embedded_extension_options;
};

base::LazyInstance<CommonSwitches> g_common_switches =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FeatureSwitch* FeatureSwitch::force_dev_mode_highlighting() {
  return &g_common_switches.Get().force_dev_mode_highlighting;
}
FeatureSwitch* FeatureSwitch::easy_off_store_install() {
  return &g_common_switches.Get().easy_off_store_install;
}
FeatureSwitch* FeatureSwitch::prompt_for_external_extensions() {
  return &g_common_switches.Get().prompt_for_external_extensions;
}
FeatureSwitch* FeatureSwitch::error_console() {
  return &g_common_switches.Get().error_console;
}
FeatureSwitch* FeatureSwitch::enable_override_bookmarks_ui() {
  return &g_common_switches.Get().enable_override_bookmarks_ui;
}
FeatureSwitch* FeatureSwitch::extension_action_redesign() {
  return &g_common_switches.Get().extension_action_redesign;
}
FeatureSwitch* FeatureSwitch::scripts_require_action() {
  return &g_common_switches.Get().scripts_require_action;
}
FeatureSwitch* FeatureSwitch::embedded_extension_options() {
  return &g_common_switches.Get().embedded_extension_options;
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

  if (!switch_name_)
    return default_value_;

  std::string temp = command_line_->GetSwitchValueASCII(switch_name_);
  std::string switch_value;
  base::TrimWhitespaceASCII(temp, base::TRIM_ALL, &switch_value);

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
  DCHECK(switch_name_);
  return std::string("enable-") + switch_name_;
}

std::string FeatureSwitch::GetLegacyDisableFlag() const {
  DCHECK(switch_name_);
  return std::string("disable-") + switch_name_;
}

void FeatureSwitch::SetOverrideValue(OverrideValue override_value) {
  override_value_ = override_value;
}

FeatureSwitch::OverrideValue FeatureSwitch::GetOverrideValue() const {
  return override_value_;
}

}  // namespace extensions
