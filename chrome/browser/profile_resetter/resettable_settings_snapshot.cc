// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"

#include "base/json/json_writer.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/feedback/feedback_data.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Feedback bucket label.
const char kProfileResetFeedbackBucket[] = "ProfileResetReport";

// Dictionary keys for feedback report.
const char kDefaultSearchEnginePath[] = "default_search_engine";
const char kEnabledExtensions[] = "enabled_extensions";
const char kFeedbackCaller[] = "initiator";
const char kHomepageIsNewTabPage[] = "homepage_is_ntp";
const char kHomepagePath[] = "homepage";
const char kStartupTypePath[] = "startup_type";
const char kStartupURLPath[] = "startup_urls";

template <class StringType>
void AddPair(ListValue* list, const string16& key, const StringType& value) {
  DictionaryValue* results = new DictionaryValue();
  results->SetString("key", key);
  results->SetString("value", value);
  list->Append(results);
}

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
    enabled_extensions_.push_back(std::make_pair((*it)->id(), (*it)->name()));

  // ExtensionSet is sorted but it seems to be an implementation detail.
  std::sort(enabled_extensions_.begin(), enabled_extensions_.end());
}

ResettableSettingsSnapshot::~ResettableSettingsSnapshot() {}

void ResettableSettingsSnapshot::Subtract(
    const ResettableSettingsSnapshot& snapshot) {
  std::vector<GURL> urls = base::STLSetDifference<std::vector<GURL> >(
      startup_.urls, snapshot.startup_.urls);
  startup_.urls.swap(urls);

  ExtensionList extensions = base::STLSetDifference<ExtensionList>(
      enabled_extensions_, snapshot.enabled_extensions_);
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
                                    int field_mask,
                                    SnapshotCaller source) {
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
    const ResettableSettingsSnapshot::ExtensionList& extensions =
        snapshot.enabled_extensions();
    for (ResettableSettingsSnapshot::ExtensionList::const_iterator i =
         extensions.begin(); i != extensions.end(); ++i) {
      // Replace "\"" to simplify server-side analysis.
      std::string ext_name;
      ReplaceChars(i->second, "\"", "\'", &ext_name);
      list->AppendString(i->first + ";" + ext_name);
    }
    dict.Set(kEnabledExtensions, list);
  }

  COMPILE_ASSERT(ResettableSettingsSnapshot::ALL_FIELDS == 63,
                 serialize_new_field_here);

  dict.SetInteger(kFeedbackCaller, source);

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

ListValue* GetReadableFeedback(Profile* profile) {
  DCHECK(profile);
  ListValue* list = new ListValue;
  AddPair(list, l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_LOCALE),
          g_browser_process->GetApplicationLocale());
  AddPair(list,
          l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_USER_AGENT),
          content::GetUserAgent(GURL()));
  chrome::VersionInfo version_info;
  std::string version = version_info.Version();
  version += chrome::VersionInfo::GetVersionStringModifier();
  AddPair(list,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          version);

  // Add snapshot data.
  ResettableSettingsSnapshot snapshot(profile);
  const std::vector<GURL>& urls = snapshot.startup_urls();
  std::string startup_urls;
  for (std::vector<GURL>::const_iterator i = urls.begin();
       i != urls.end(); ++i) {
    (startup_urls += i->host()) += ' ';
  }
  if (!startup_urls.empty()) {
    startup_urls.erase(startup_urls.end() - 1);
    AddPair(list,
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_STARTUP_URLS),
            startup_urls);
  }

  string16 startup_type;
  switch (snapshot.startup_type()) {
    case SessionStartupPref::DEFAULT:
      startup_type = l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_SHOW_NEWTAB);
      break;
    case SessionStartupPref::LAST:
      startup_type = l10n_util::GetStringUTF16(
          IDS_OPTIONS_STARTUP_RESTORE_LAST_SESSION);
      break;
    case SessionStartupPref::URLS:
      startup_type = l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_SHOW_PAGES);
      break;
    default:
      break;
  }
  AddPair(list,
          l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_STARTUP_TYPE),
          startup_type);

  if (!snapshot.homepage().empty()) {
    AddPair(list,
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_HOMEPAGE),
            snapshot.homepage());
  }

  int is_ntp_message_id = snapshot.homepage_is_ntp() ?
      IDS_RESET_PROFILE_SETTINGS_HOMEPAGE_IS_NTP_TRUE :
      IDS_RESET_PROFILE_SETTINGS_HOMEPAGE_IS_NTP_FALSE;
  AddPair(list,
          l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_HOMEPAGE_IS_NTP),
          l10n_util::GetStringUTF16(is_ntp_message_id));

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(service);
  TemplateURL* dse = service->GetDefaultSearchProvider();
  if (dse) {
    AddPair(list,
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_DSE),
            TemplateURLService::GenerateSearchURL(dse).host());
  }

  const ResettableSettingsSnapshot::ExtensionList& extensions =
      snapshot.enabled_extensions();
  std::string extension_names;
  for (ResettableSettingsSnapshot::ExtensionList::const_iterator i =
       extensions.begin(); i != extensions.end(); ++i) {
    (extension_names += i->second) += '\n';
  }
  if (!extension_names.empty()) {
    extension_names.erase(extension_names.end() - 1);
    AddPair(list,
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_EXTENSIONS),
            extension_names);
  }
  return list;
}
