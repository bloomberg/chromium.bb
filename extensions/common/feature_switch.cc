// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/feature_switch.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

// The switch load-media-router-component-extension is defined in
// chrome/common/chrome_switches.cc, but we can't depend on chrome here.
const char kLoadMediaRouterComponentExtensionFlag[] =
    "load-media-router-component-extension";

const char kYieldBetweenContentScriptRunsFieldTrial[] =
    "YieldBetweenContentScriptRuns";

class CommonSwitches {
 public:
  CommonSwitches()
      : force_dev_mode_highlighting(switches::kForceDevModeHighlighting,
                                    FeatureSwitch::DEFAULT_DISABLED),
        prompt_for_external_extensions(
#if defined(CHROMIUM_BUILD)
            switches::kPromptForExternalExtensions,
#else
            nullptr,
#endif
#if defined(OS_WIN) || defined(OS_MACOSX)
            FeatureSwitch::DEFAULT_ENABLED),
#else
            FeatureSwitch::DEFAULT_DISABLED),
#endif
        error_console(switches::kErrorConsole, FeatureSwitch::DEFAULT_DISABLED),
        enable_override_bookmarks_ui(switches::kEnableOverrideBookmarksUI,
                                     FeatureSwitch::DEFAULT_DISABLED),
        scripts_require_action(switches::kScriptsRequireAction,
                               FeatureSwitch::DEFAULT_DISABLED),
        embedded_extension_options(switches::kEmbeddedExtensionOptions,
                                   FeatureSwitch::DEFAULT_DISABLED),
        trace_app_source(switches::kTraceAppSource,
                         FeatureSwitch::DEFAULT_ENABLED),
        load_media_router_component_extension(
            kLoadMediaRouterComponentExtensionFlag,
#if defined(GOOGLE_CHROME_BUILD)
            FeatureSwitch::DEFAULT_ENABLED),
#else
            FeatureSwitch::DEFAULT_DISABLED),
#endif  // defined(GOOGLE_CHROME_BUILD)
        yield_between_content_script_runs(
            switches::kYieldBetweenContentScriptRuns,
            kYieldBetweenContentScriptRunsFieldTrial,
            FeatureSwitch::DEFAULT_DISABLED) {
  }

  FeatureSwitch force_dev_mode_highlighting;

  // Should we prompt the user before allowing external extensions to install?
  // Default is yes.
  FeatureSwitch prompt_for_external_extensions;

  FeatureSwitch error_console;
  FeatureSwitch enable_override_bookmarks_ui;
  FeatureSwitch scripts_require_action;
  FeatureSwitch embedded_extension_options;
  FeatureSwitch trace_app_source;
  FeatureSwitch load_media_router_component_extension;
  FeatureSwitch yield_between_content_script_runs;
};

base::LazyInstance<CommonSwitches>::DestructorAtExit g_common_switches =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FeatureSwitch* FeatureSwitch::force_dev_mode_highlighting() {
  return &g_common_switches.Get().force_dev_mode_highlighting;
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
FeatureSwitch* FeatureSwitch::scripts_require_action() {
  return &g_common_switches.Get().scripts_require_action;
}
FeatureSwitch* FeatureSwitch::embedded_extension_options() {
  return &g_common_switches.Get().embedded_extension_options;
}
FeatureSwitch* FeatureSwitch::trace_app_source() {
  return &g_common_switches.Get().trace_app_source;
}
FeatureSwitch* FeatureSwitch::load_media_router_component_extension() {
  return &g_common_switches.Get().load_media_router_component_extension;
}
FeatureSwitch* FeatureSwitch::yield_between_content_script_runs() {
  return &g_common_switches.Get().yield_between_content_script_runs;
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
                             DefaultValue default_value)
    : FeatureSwitch(base::CommandLine::ForCurrentProcess(),
                    switch_name,
                    default_value) {}

FeatureSwitch::FeatureSwitch(const char* switch_name,
                             const char* field_trial_name,
                             DefaultValue default_value)
    : FeatureSwitch(base::CommandLine::ForCurrentProcess(),
                    switch_name,
                    field_trial_name,
                    default_value) {}

FeatureSwitch::FeatureSwitch(const base::CommandLine* command_line,
                             const char* switch_name,
                             DefaultValue default_value)
    : FeatureSwitch(command_line, switch_name, nullptr, default_value) {}

FeatureSwitch::FeatureSwitch(const base::CommandLine* command_line,
                             const char* switch_name,
                             const char* field_trial_name,
                             DefaultValue default_value)
    : command_line_(command_line),
      switch_name_(switch_name),
      field_trial_name_(field_trial_name),
      default_value_(default_value == DEFAULT_ENABLED),
      override_value_(OVERRIDE_NONE) {}

bool FeatureSwitch::IsEnabled() const {
  if (override_value_ != OVERRIDE_NONE)
    return override_value_ == OVERRIDE_ENABLED;
  if (!cached_value_.has_value())
    cached_value_ = ComputeValue();
  return cached_value_.value();
}

bool FeatureSwitch::ComputeValue() const {
  if (!switch_name_)
    return default_value_;

  std::string temp = command_line_->GetSwitchValueASCII(switch_name_);
  std::string switch_value;
  base::TrimWhitespaceASCII(temp, base::TRIM_ALL, &switch_value);

  if (switch_value == "1")
    return true;

  if (switch_value == "0")
    return false;

  // TODO(imcheng): Don't check |default_value_|. Otherwise, we could improperly
  // ignore an enable/disable switch if there is a field trial active.
  // crbug.com/585569
  if (!default_value_ && command_line_->HasSwitch(GetLegacyEnableFlag()))
    return true;

  if (default_value_ && command_line_->HasSwitch(GetLegacyDisableFlag()))
    return false;

  if (field_trial_name_) {
    std::string group_name =
        base::FieldTrialList::FindFullName(field_trial_name_);
    if (base::StartsWith(group_name, "Enabled", base::CompareCase::SENSITIVE))
      return true;
    if (base::StartsWith(group_name, "Disabled", base::CompareCase::SENSITIVE))
      return false;
  }

  return default_value_;
}

bool FeatureSwitch::HasValue() const {
  return override_value_ != OVERRIDE_NONE ||
         command_line_->HasSwitch(switch_name_) ||
         command_line_->HasSwitch(GetLegacyEnableFlag()) ||
         command_line_->HasSwitch(GetLegacyDisableFlag());
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
