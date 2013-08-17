// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"

#include "base/json/json_writer.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/feedback/feedback_data.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"

namespace {

// Feedback bucket label.
const char kProfileResetFeedbackBucket[] = "ProfileResetReport";

// Dictionary keys for feedback report.
const char kDefaultSearchEnginePath[] = "default_search_engine";
const char kEnabledExtensions[] = "enabled_extensions";
const char kHomepageIsNewTabPage[] = "homepage_is_ntp";
const char kHomepagePath[] = "homepage";
const char kStartupTypePath[] = "startup_type";
const char kStartupURLPath[] = "startup_urls";

}  // namespace

ResettableSettingsSnapshot::ResettableSettingsSnapshot(Profile* profile)
    : startup_(SessionStartupPref::GetStartupPref(profile)) {
  // URLs are always stored sorted.
  std::sort(startup_.urls.begin(), startup_.urls.end());

  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);
  homepage_ = prefs->GetString(prefs::kHomePage);
  homepage_is_ntp_ = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(service);
  TemplateURL* dse = service->GetDefaultSearchProvider();
  if (dse)
    dse_url_ = dse->url();

  ExtensionService* extension_service = profile->GetExtensionService();
  DCHECK(extension_service);
  const ExtensionSet* enabled_ext = extension_service->extensions();
  enabled_extensions_.reserve(enabled_ext->size());

  for (ExtensionSet::const_iterator it = enabled_ext->begin();
       it != enabled_ext->end(); ++it)
    enabled_extensions_.push_back((*it)->id());

  // ExtensionSet is sorted but it seems to be an implementation detail.
  std::sort(enabled_extensions_.begin(), enabled_extensions_.end());
}

ResettableSettingsSnapshot::~ResettableSettingsSnapshot() {}

void ResettableSettingsSnapshot::Subtract(
    const ResettableSettingsSnapshot& snapshot) {
  std::vector<GURL> urls;
  std::set_difference(startup_.urls.begin(), startup_.urls.end(),
                      snapshot.startup_.urls.begin(),
                      snapshot.startup_.urls.end(),
                      std::back_inserter(urls));
  startup_.urls.swap(urls);

  std::vector<std::string> extensions;
  std::set_difference(enabled_extensions_.begin(), enabled_extensions_.end(),
                      snapshot.enabled_extensions_.begin(),
                      snapshot.enabled_extensions_.end(),
                      std::back_inserter(extensions));
  enabled_extensions_.swap(extensions);
}

int ResettableSettingsSnapshot::FindDifferentFields(
    const ResettableSettingsSnapshot& snapshot) const {
  int bit_mask = 0;

  if (startup_.urls != snapshot.startup_.urls) {
    bit_mask |= STARTUP_URLS;
  }

  if (startup_.type != snapshot.startup_.type)
    bit_mask |= STARTUP_TYPE;

  if (homepage_ != snapshot.homepage_)
    bit_mask |= HOMEPAGE;

  if (homepage_is_ntp_ != snapshot.homepage_is_ntp_)
    bit_mask |= HOMEPAGE_IS_NTP;

  if (dse_url_ != snapshot.dse_url_)
    bit_mask |= DSE_URL;

  if (enabled_extensions_ != snapshot.enabled_extensions_)
    bit_mask |= EXTENSIONS;

  COMPILE_ASSERT(ResettableSettingsSnapshot::ALL_FIELDS == 63,
                 add_new_field_here);

  return bit_mask;
}

std::string SerializeSettingsReport(const ResettableSettingsSnapshot& snapshot,
                                    int field_mask) {
  DictionaryValue dict;

  if (field_mask & ResettableSettingsSnapshot::STARTUP_URLS) {
    ListValue* list = new ListValue;
    const std::vector<GURL>& urls = snapshot.startup_urls();
    for (std::vector<GURL>::const_iterator i = urls.begin();
         i != urls.end(); ++i)
      list->AppendString(i->spec());
    dict.Set(kStartupURLPath, list);
  }

  if (field_mask & ResettableSettingsSnapshot::STARTUP_TYPE)
    dict.SetInteger(kStartupTypePath, snapshot.startup_type());

  if (field_mask & ResettableSettingsSnapshot::HOMEPAGE)
    dict.SetString(kHomepagePath, snapshot.homepage());

  if (field_mask & ResettableSettingsSnapshot::HOMEPAGE_IS_NTP)
    dict.SetBoolean(kHomepageIsNewTabPage, snapshot.homepage_is_ntp());

  if (field_mask & ResettableSettingsSnapshot::DSE_URL)
    dict.SetString(kDefaultSearchEnginePath, snapshot.dse_url());

  if (field_mask & ResettableSettingsSnapshot::EXTENSIONS) {
    ListValue* list = new ListValue;
    const std::vector<std::string>& extensions = snapshot.enabled_extensions();
    for (std::vector<std::string>::const_iterator i = extensions.begin();
         i != extensions.end(); ++i)
      list->AppendString(*i);
    dict.Set(kEnabledExtensions, list);
  }

  COMPILE_ASSERT(ResettableSettingsSnapshot::ALL_FIELDS == 63,
                 serialize_new_field_here);

  std::string json;
  base::JSONWriter::Write(&dict, &json);
  return json;
}

void SendSettingsFeedback(const std::string& report, Profile* profile) {
  scoped_refptr<FeedbackData> feedback_data = new FeedbackData();
  feedback_data->set_category_tag(kProfileResetFeedbackBucket);
  feedback_data->set_description(report);

  feedback_data->set_image(scoped_ptr<std::string>(new std::string));
  feedback_data->set_profile(profile);

  feedback_data->set_page_url("");
  feedback_data->set_user_email("");

  feedback_util::SendReport(feedback_data);
}
