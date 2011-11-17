// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for objects providing content setting rules.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PROVIDER_H_
#pragma once

#define NO_RESOURCE_IDENTIFIER ""

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/common/content_settings.h"

class ContentSettingsPattern;

namespace content_settings {

struct Rule;
class RuleIterator;

typedef std::string ResourceIdentifier;

class ProviderInterface {
 public:
  virtual ~ProviderInterface() {}

  // Returns a |RuleIterator| over the content setting rules stored by this
  // provider. If |incognito| is true, the iterator returns only the content
  // settings which are applicable to the incognito mode and differ from the
  // normal mode. Otherwise, it returns the content settings for the normal
  // mode. The caller takes the ownership of the returned |RuleIterator|. It is
  // not allowed to call other |ProviderInterface| functions (including
  // |GetRuleIterator|) for the same provider until the |RuleIterator| is
  // destroyed.
  virtual RuleIterator* GetRuleIterator(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      bool incognito) const = 0;

  // Askes the provider to set the website setting for a particular
  // |primary_pattern|, |secondary_pattern|, |content_type| tuple. If the
  // provider accepts the setting it returns true and takes the ownership of the
  // |value|. Otherwise false is returned and the ownership of the |value| stays
  // with the caller.
  //
  // This should only be called on the UI thread, and not after
  // ShutdownOnUIThread has been called.
  virtual bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      Value* value) = 0;

  // Resets all content settings for the given |content_type| and empty resource
  // identifier to CONTENT_SETTING_DEFAULT.
  //
  // This should only be called on the UI thread, and not after
  // ShutdownOnUIThread has been called.
  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type) = 0;

  // Detaches the Provider from all Profile-related objects like PrefService.
  // This methods needs to be called before destroying the Profile.
  // Afterwards, none of the methods above that should only be called on the UI
  // thread should be called anymore.
  virtual void ShutdownOnUIThread() = 0;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PROVIDER_H_
