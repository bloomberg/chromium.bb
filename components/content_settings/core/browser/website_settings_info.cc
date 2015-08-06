// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_info.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace {

const char* kPrefPrefix = "profile.content_settings.exceptions.";
const char* kDefaultPrefPrefix = "profile.default_content_setting_values.";

std::string GetPrefName(const std::string& name, const char* prefix) {
  std::string pref_name = name;
  base::ReplaceChars(pref_name, "-", "_", &pref_name);
  return std::string(prefix).append(pref_name);
}

}  // namespace

namespace content_settings {

WebsiteSettingsInfo::WebsiteSettingsInfo(
    ContentSettingsType type,
    const std::string& name,
    scoped_ptr<base::Value> initial_default_value)
    : type_(type),
      name_(name),
      pref_name_(GetPrefName(name, kPrefPrefix)),
      default_value_pref_name_(GetPrefName(name, kDefaultPrefPrefix)),
      initial_default_value_(initial_default_value.Pass()) {
  // For legacy reasons the default value is currently restricted to be an int.
  // TODO(raymes): We should migrate the underlying pref to be a dictionary
  // rather than an int.
  DCHECK(!initial_default_value_ ||
         initial_default_value_->IsType(base::Value::TYPE_INTEGER));
}

WebsiteSettingsInfo::~WebsiteSettingsInfo() {}

}  // namespace content_settings
