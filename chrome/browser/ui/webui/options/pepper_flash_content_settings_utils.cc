// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/pepper_flash_content_settings_utils.h"

#include <algorithm>

#include "base/memory/scoped_ptr.h"

namespace options {

namespace {

int CompareMediaException(const MediaException& i, const MediaException& j) {
  return i.pattern.Compare(j.pattern);
}

bool MediaExceptionSortFunc(const MediaException& i, const MediaException& j) {
  return CompareMediaException(i, j) < 0;
}

}  // namespace

MediaException::MediaException(const ContentSettingsPattern& in_pattern,
                               ContentSetting in_setting)
    : pattern(in_pattern),
      setting(in_setting) {
}

MediaException::~MediaException() {
}

bool MediaException::operator==(const MediaException& other) const {
  return pattern == other.pattern && setting == other.setting;
}

// static
ContentSetting PepperFlashContentSettingsUtils::FlashPermissionToContentSetting(
    PP_Flash_BrowserOperations_Permission permission) {
  switch (permission) {
    case PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT:
      return CONTENT_SETTING_DEFAULT;
    case PP_FLASH_BROWSEROPERATIONS_PERMISSION_ALLOW:
      return CONTENT_SETTING_ALLOW;
    case PP_FLASH_BROWSEROPERATIONS_PERMISSION_BLOCK:
      return CONTENT_SETTING_BLOCK;
    case PP_FLASH_BROWSEROPERATIONS_PERMISSION_ASK:
      return CONTENT_SETTING_ASK;
    // No default so the compiler will warn us if a new type is added.
  }
  return CONTENT_SETTING_DEFAULT;
}

// static
void PepperFlashContentSettingsUtils::FlashSiteSettingsToMediaExceptions(
    const ppapi::FlashSiteSettings& site_settings,
    MediaExceptions* media_exceptions) {
  media_exceptions->clear();

  scoped_ptr<ContentSettingsPattern::BuilderInterface> builder(
      ContentSettingsPattern::CreateBuilder(false));
  builder->WithSchemeWildcard()->WithPortWildcard();
  for (ppapi::FlashSiteSettings::const_iterator iter = site_settings.begin();
       iter != site_settings.end(); ++iter) {
    builder->WithHost(iter->site);

    ContentSettingsPattern pattern = builder->Build();
    if (!pattern.IsValid())
      continue;

    ContentSetting setting = FlashPermissionToContentSetting(iter->permission);

    media_exceptions->push_back(MediaException(pattern, setting));
  }
}

// static
void PepperFlashContentSettingsUtils::SortMediaExceptions(
    MediaExceptions* media_exceptions) {
  std::sort(media_exceptions->begin(), media_exceptions->end(),
            MediaExceptionSortFunc);
}

// static
bool PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
    ContentSetting default_setting_1,
    const MediaExceptions& exceptions_1,
    ContentSetting default_setting_2,
    const MediaExceptions& exceptions_2) {
  MediaExceptions::const_iterator iter_1 = exceptions_1.begin();
  MediaExceptions::const_iterator iter_2 = exceptions_2.begin();

  MediaException default_exception_1(ContentSettingsPattern(),
                                     default_setting_1);
  MediaException default_exception_2(ContentSettingsPattern(),
                                     default_setting_2);

  while (iter_1 != exceptions_1.end() && iter_2 != exceptions_2.end()) {
    int compare_result = CompareMediaException(*iter_1, *iter_2);
    if (compare_result < 0) {
      if (iter_1->setting != default_exception_2.setting)
        return false;
      ++iter_1;
    } else if (compare_result > 0) {
      if (iter_2->setting != default_exception_1.setting) {
        return false;
      }
      ++iter_2;
    } else {
      if (iter_1->setting != iter_2->setting)
        return false;
      ++iter_1;
      ++iter_2;
    }
  }

  while (iter_1 != exceptions_1.end()) {
    if (iter_1->setting != default_exception_2.setting)
      return false;
    ++iter_1;
  }
  while (iter_2 != exceptions_2.end()) {
    if (iter_2->setting != default_exception_1.setting)
      return false;
    ++iter_2;
  }
  return true;
}

}  // namespace options
