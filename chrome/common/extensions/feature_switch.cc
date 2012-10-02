// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/feature_switch.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

namespace {

class CommonSwitches {
 public:
  CommonSwitches()
      : script_bubble(CommandLine::ForCurrentProcess(),
                      switches::kScriptBubbleEnabled,
                      FeatureSwitch::DEFAULT_DISABLED) {
  }
  FeatureSwitch script_bubble;
};

base::LazyInstance<CommonSwitches> g_common_switches =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace


FeatureSwitch* FeatureSwitch::GetScriptBubble() {
  return &g_common_switches.Get().script_bubble;
}

FeatureSwitch::ScopedOverride::ScopedOverride(FeatureSwitch* feature,
                                              bool override_value)
    : feature_(feature) {
  feature_->SetOverrideValue(
      override_value ? OVERRIDE_ENABLED : OVERRIDE_DISABLED);
}

FeatureSwitch::ScopedOverride::~ScopedOverride() {
  feature_->SetOverrideValue(OVERRIDE_NONE);
}

FeatureSwitch::FeatureSwitch(const CommandLine* command_line,
                             const char* switch_name,
                             DefaultValue default_value)
    : command_line_(command_line),
      switch_name_(switch_name),
      default_value_(default_value == DEFAULT_ENABLED),
      override_value_(OVERRIDE_NONE) {
}

bool FeatureSwitch::IsEnabled() const {
  if (override_value_ != OVERRIDE_NONE)
    return override_value_ == OVERRIDE_ENABLED;

  // TODO(aa): Consider supporting other values.
  std::string temp = command_line_->GetSwitchValueASCII(switch_name_);
  std::string switch_value;
  TrimWhitespaceASCII(temp, TRIM_ALL, &switch_value);
  if (switch_value == "1")
    return true;
  if (switch_value == "0")
    return false;
  return default_value_;
}

void FeatureSwitch::SetOverrideValue(OverrideValue override_value) {
  if (override_value_ != OVERRIDE_NONE)
    CHECK_EQ(OVERRIDE_NONE, override_value);

  override_value_ = override_value;
}

}  // namespace extensions
