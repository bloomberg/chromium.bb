// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"

#include "base/json/json_writer.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_util.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"

using feedback::FeedbackData;

namespace {

// Feedback bucket labels.
const char kProfileResetPromptBucket[] = "SamplingOfSettingsResetPrompt";
const char kProfileResetWebUIBucket[] = "ProfileResetReport";

// Dictionary keys for feedback report.
const char kDefaultSearchEnginePath[] = "default_search_engine";
const char kEnabledExtensions[] = "enabled_extensions";
const char kHomepageIsNewTabPage[] = "homepage_is_ntp";
const char kHomepagePath[] = "homepage";
const char kShortcuts[] = "shortcuts";
const char kStartupTypePath[] = "startup_type";
const char kStartupURLPath[] = "startup_urls";

template <class StringType>
void AddPair(base::ListValue* list,
             const base::string16& key,
             const StringType& value) {
  base::DictionaryValue* results = new base::DictionaryValue();
  results->SetString("key", key);
  results->SetString("value", value);
  list->Append(results);
}

}  // namespace

ResettableSettingsSnapshot::ResettableSettingsSnapshot(
    Profile* profile)
    : startup_(SessionStartupPref::GetStartupPref(profile)),
      shortcuts_determined_(false),
      weak_ptr_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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

  const extensions::ExtensionSet& enabled_ext =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  enabled_extensions_.reserve(enabled_ext.size());

  for (extensions::ExtensionSet::const_iterator it = enabled_ext.begin();
       it != enabled_ext.end(); ++it)
    enabled_extensions_.push_back(std::make_pair((*it)->id(), (*it)->name()));

  // ExtensionSet is sorted but it seems to be an implementation detail.
  std::sort(enabled_extensions_.begin(), enabled_extensions_.end());
}

ResettableSettingsSnapshot::~ResettableSettingsSnapshot() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (cancellation_flag_)
    cancellation_flag_->data.Set();
}

void ResettableSettingsSnapshot::Subtract(
    const ResettableSettingsSnapshot& snapshot) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ExtensionList extensions = base::STLSetDifference<ExtensionList>(
      enabled_extensions_, snapshot.enabled_extensions_);
  enabled_extensions_.swap(extensions);
}

int ResettableSettingsSnapshot::FindDifferentFields(
    const ResettableSettingsSnapshot& snapshot) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  int bit_mask = 0;

  if (startup_.type != snapshot.startup_.type ||
      startup_.urls != snapshot.startup_.urls)
    bit_mask |= STARTUP_MODE;

  if (homepage_is_ntp_ != snapshot.homepage_is_ntp_ ||
      homepage_ != snapshot.homepage_)
    bit_mask |= HOMEPAGE;

  if (dse_url_ != snapshot.dse_url_)
    bit_mask |= DSE_URL;

  if (enabled_extensions_ != snapshot.enabled_extensions_)
    bit_mask |= EXTENSIONS;

  if (shortcuts_ != snapshot.shortcuts_)
    bit_mask |= SHORTCUTS;

  COMPILE_ASSERT(ResettableSettingsSnapshot::ALL_FIELDS == 31,
                 add_new_field_here);

  return bit_mask;
}

void ResettableSettingsSnapshot::RequestShortcuts(
    const base::Closure& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!cancellation_flag_ && !shortcuts_determined());

  cancellation_flag_ = new SharedCancellationFlag;
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&GetChromeLaunchShortcuts, cancellation_flag_),
      base::Bind(&ResettableSettingsSnapshot::SetShortcutsAndReport,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void ResettableSettingsSnapshot::SetShortcutsAndReport(
    const base::Closure& callback,
    const std::vector<ShortcutCommand>& shortcuts) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  shortcuts_ = shortcuts;
  shortcuts_determined_ = true;
  cancellation_flag_ = NULL;

  if (!callback.is_null())
    callback.Run();
}

std::string SerializeSettingsReport(const ResettableSettingsSnapshot& snapshot,
                                    int field_mask) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::DictionaryValue dict;

  if (field_mask & ResettableSettingsSnapshot::STARTUP_MODE) {
    base::ListValue* list = new base::ListValue;
    const std::vector<GURL>& urls = snapshot.startup_urls();
    for (std::vector<GURL>::const_iterator i = urls.begin();
         i != urls.end(); ++i)
      list->AppendString(i->spec());
    dict.Set(kStartupURLPath, list);
    dict.SetInteger(kStartupTypePath, snapshot.startup_type());
  }

  if (field_mask & ResettableSettingsSnapshot::HOMEPAGE) {
    dict.SetString(kHomepagePath, snapshot.homepage());
    dict.SetBoolean(kHomepageIsNewTabPage, snapshot.homepage_is_ntp());
  }

  if (field_mask & ResettableSettingsSnapshot::DSE_URL)
    dict.SetString(kDefaultSearchEnginePath, snapshot.dse_url());

  if (field_mask & ResettableSettingsSnapshot::EXTENSIONS) {
    base::ListValue* list = new base::ListValue;
    const ResettableSettingsSnapshot::ExtensionList& extensions =
        snapshot.enabled_extensions();
    for (ResettableSettingsSnapshot::ExtensionList::const_iterator i =
         extensions.begin(); i != extensions.end(); ++i) {
      // Replace "\"" to simplify server-side analysis.
      std::string ext_name;
      base::ReplaceChars(i->second, "\"", "\'", &ext_name);
      list->AppendString(i->first + ";" + ext_name);
    }
    dict.Set(kEnabledExtensions, list);
  }

  if (field_mask & ResettableSettingsSnapshot::SHORTCUTS) {
    base::ListValue* list = new base::ListValue;
    const std::vector<ShortcutCommand>& shortcuts = snapshot.shortcuts();
    for (std::vector<ShortcutCommand>::const_iterator i = shortcuts.begin();
         i != shortcuts.end(); ++i) {
      base::string16 arguments;
      // Replace "\"" to simplify server-side analysis.
      base::ReplaceChars(i->second, base::ASCIIToUTF16("\""),
                         base::ASCIIToUTF16("\'"), &arguments);
      list->AppendString(arguments);
    }
    dict.Set(kShortcuts, list);
  }

  COMPILE_ASSERT(ResettableSettingsSnapshot::ALL_FIELDS == 31,
                 serialize_new_field_here);

  std::string json;
  base::JSONWriter::Write(&dict, &json);
  return json;
}

void SendSettingsFeedback(const std::string& report,
                          Profile* profile,
                          SnapshotCaller caller) {
  scoped_refptr<FeedbackData> feedback_data = new FeedbackData();
  std::string bucket;
  switch (caller) {
    case PROFILE_RESET_WEBUI:
      bucket = kProfileResetWebUIBucket;
      break;
    case PROFILE_RESET_PROMPT:
      bucket = kProfileResetPromptBucket;
      break;
  }
  feedback_data->set_category_tag(bucket);
  feedback_data->set_description(report);

  feedback_data->set_image(make_scoped_ptr(new std::string));
  feedback_data->set_context(profile);

  feedback_data->set_page_url("");
  feedback_data->set_user_email("");

  feedback_util::SendReport(feedback_data);
}

scoped_ptr<base::ListValue> GetReadableFeedbackForSnapshot(
    Profile* profile,
    const ResettableSettingsSnapshot& snapshot) {
  DCHECK(profile);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scoped_ptr<base::ListValue> list(new base::ListValue);
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_LOCALE),
          g_browser_process->GetApplicationLocale());
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_USER_AGENT),
          GetUserAgent());
  chrome::VersionInfo version_info;
  std::string version = version_info.Version();
  version += chrome::VersionInfo::GetVersionStringModifier();
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          version);

  // Add snapshot data.
  const std::vector<GURL>& urls = snapshot.startup_urls();
  std::string startup_urls;
  for (std::vector<GURL>::const_iterator i = urls.begin();
       i != urls.end(); ++i) {
    if (!startup_urls.empty())
      startup_urls += ' ';
    startup_urls += i->host();
  }
  if (!startup_urls.empty()) {
    AddPair(list.get(),
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_STARTUP_URLS),
            startup_urls);
  }

  base::string16 startup_type;
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
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_STARTUP_TYPE),
          startup_type);

  if (!snapshot.homepage().empty()) {
    AddPair(list.get(),
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_HOMEPAGE),
            snapshot.homepage());
  }

  int is_ntp_message_id = snapshot.homepage_is_ntp() ?
      IDS_RESET_PROFILE_SETTINGS_HOMEPAGE_IS_NTP_TRUE :
      IDS_RESET_PROFILE_SETTINGS_HOMEPAGE_IS_NTP_FALSE;
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_HOMEPAGE_IS_NTP),
          l10n_util::GetStringUTF16(is_ntp_message_id));

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(service);
  TemplateURL* dse = service->GetDefaultSearchProvider();
  if (dse) {
    AddPair(list.get(),
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_DSE),
            dse->GenerateSearchURL(service->search_terms_data()).host());
  }

  if (snapshot.shortcuts_determined()) {
    base::string16 shortcut_targets;
    const std::vector<ShortcutCommand>& shortcuts = snapshot.shortcuts();
    for (std::vector<ShortcutCommand>::const_iterator i =
         shortcuts.begin(); i != shortcuts.end(); ++i) {
      if (!shortcut_targets.empty())
        shortcut_targets += base::ASCIIToUTF16("\n");
      shortcut_targets += base::ASCIIToUTF16("chrome.exe ");
      shortcut_targets += i->second;
    }
    if (!shortcut_targets.empty()) {
      AddPair(list.get(),
              l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_SHORTCUTS),
              shortcut_targets);
    }
  } else {
    AddPair(list.get(),
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_SHORTCUTS),
            l10n_util::GetStringUTF16(
                IDS_RESET_PROFILE_SETTINGS_PROCESSING_SHORTCUTS));
  }

  const ResettableSettingsSnapshot::ExtensionList& extensions =
      snapshot.enabled_extensions();
  std::string extension_names;
  for (ResettableSettingsSnapshot::ExtensionList::const_iterator i =
       extensions.begin(); i != extensions.end(); ++i) {
    if (!extension_names.empty())
      extension_names += '\n';
    extension_names += i->second;
  }
  if (!extension_names.empty()) {
    AddPair(list.get(),
            l10n_util::GetStringUTF16(IDS_RESET_PROFILE_SETTINGS_EXTENSIONS),
            extension_names);
  }
  return list.Pass();
}
