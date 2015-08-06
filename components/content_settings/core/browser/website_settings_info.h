// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace base {
class Value;
}  // namespace base

namespace content_settings {

// This class stores the properties related to a website setting.
// TODO(raymes): Move more properties into this class.
class WebsiteSettingsInfo {
 public:
  WebsiteSettingsInfo(ContentSettingsType type,
                      const std::string& name,
                      scoped_ptr<base::Value> initial_default_value);
  ~WebsiteSettingsInfo();

  ContentSettingsType type() const { return type_; }
  const std::string& name() const { return name_; }

  const std::string& pref_name() const { return pref_name_; }
  const std::string& default_value_pref_name() const {
    return default_value_pref_name_;
  }
  const base::Value* initial_default_value() const {
    return initial_default_value_.get();
  }

 private:
  const ContentSettingsType type_;
  const std::string name_;

  const std::string pref_name_;
  const std::string default_value_pref_name_;
  const scoped_ptr<base::Value> initial_default_value_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsInfo);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_
