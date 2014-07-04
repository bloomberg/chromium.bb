// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include <string>

#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/synchronization/cancellation_flag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/browser_distribution.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"

#if defined(OS_WIN)
#include "base/base_paths.h"
#include "base/path_service.h"
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#include "chrome/installer/util/shell_util.h"

namespace {

void ResetShortcutsOnFileThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  // Get full path of chrome.
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return;
  BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BROWSER);
  for (int location = ShellUtil::SHORTCUT_LOCATION_FIRST;
       location < ShellUtil::NUM_SHORTCUT_LOCATIONS; ++location) {
    ShellUtil::ShortcutListMaybeRemoveUnknownArgs(
        static_cast<ShellUtil::ShortcutLocation>(location),
        dist,
        ShellUtil::CURRENT_USER,
        chrome_exe,
        true,
        NULL,
        NULL);
  }
}

}  // namespace
#endif  // defined(OS_WIN)

ProfileResetter::ProfileResetter(Profile* profile)
    : profile_(profile),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile_)),
      pending_reset_flags_(0),
      cookies_remover_(NULL),
      weak_ptr_factory_(this) {
  DCHECK(CalledOnValidThread());
  DCHECK(profile_);
}

ProfileResetter::~ProfileResetter() {
  if (cookies_remover_)
    cookies_remover_->RemoveObserver(this);
}

void ProfileResetter::Reset(
    ProfileResetter::ResettableFlags resettable_flags,
    scoped_ptr<BrandcodedDefaultSettings> master_settings,
    bool accepted_send_feedback,
    const base::Closure& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(master_settings);

  // We should never be called with unknown flags.
  CHECK_EQ(static_cast<ResettableFlags>(0), resettable_flags & ~ALL);

  // We should never be called when a previous reset has not finished.
  CHECK_EQ(static_cast<ResettableFlags>(0), pending_reset_flags_);

  if (!resettable_flags) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     callback);
    return;
  }

  master_settings_.swap(master_settings);
  callback_ = callback;

  // These flags are set to false by the individual reset functions.
  pending_reset_flags_ = resettable_flags;

  struct {
    Resettable flag;
    void (ProfileResetter::*method)();
  } flagToMethod[] = {
    {DEFAULT_SEARCH_ENGINE, &ProfileResetter::ResetDefaultSearchEngine},
    {HOMEPAGE, &ProfileResetter::ResetHomepage},
    {CONTENT_SETTINGS, &ProfileResetter::ResetContentSettings},
    {COOKIES_AND_SITE_DATA, &ProfileResetter::ResetCookiesAndSiteData},
    {EXTENSIONS, &ProfileResetter::ResetExtensions},
    {STARTUP_PAGES, &ProfileResetter::ResetStartupPages},
    {PINNED_TABS, &ProfileResetter::ResetPinnedTabs},
    {SHORTCUTS, &ProfileResetter::ResetShortcuts},
  };

  ResettableFlags reset_triggered_for_flags = 0;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(flagToMethod); ++i) {
    if (resettable_flags & flagToMethod[i].flag) {
      reset_triggered_for_flags |= flagToMethod[i].flag;
      (this->*flagToMethod[i].method)();
    }
  }

// When the user resets any of their settings on Windows and agreed to sending
// feedback, run the software reporter tool to see if it could find the reason
// why the user wanted a reset.
#if defined(OS_WIN)
  // The browser process and / or local_state can be NULL when running tests.
  if (accepted_send_feedback && g_browser_process &&
      g_browser_process->local_state() &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kMetricsReportingEnabled)) {
    ExecuteSwReporter(g_browser_process->component_updater(),
                      g_browser_process->local_state());
  }
#endif

  DCHECK_EQ(resettable_flags, reset_triggered_for_flags);
}

bool ProfileResetter::IsActive() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return pending_reset_flags_ != 0;
}

void ProfileResetter::MarkAsDone(Resettable resettable) {
  DCHECK(CalledOnValidThread());

  // Check that we are never called twice or unexpectedly.
  CHECK(pending_reset_flags_ & resettable);

  pending_reset_flags_ &= ~resettable;

  if (!pending_reset_flags_) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     callback_);
    callback_.Reset();
    master_settings_.reset();
    template_url_service_sub_.reset();
  }
}

void ProfileResetter::ResetDefaultSearchEngine() {
  DCHECK(CalledOnValidThread());
  DCHECK(template_url_service_);
  // If TemplateURLServiceFactory is ready we can clean it right now.
  // Otherwise, load it and continue from ProfileResetter::Observe.
  if (template_url_service_->loaded()) {
    PrefService* prefs = profile_->GetPrefs();
    DCHECK(prefs);
    TemplateURLPrepopulateData::ClearPrepopulatedEnginesInPrefs(
        profile_->GetPrefs());
    scoped_ptr<base::ListValue> search_engines(
        master_settings_->GetSearchProviderOverrides());
    if (search_engines) {
      // This Chrome distribution channel provides a custom search engine. We
      // must reset to it.
      ListPrefUpdate update(prefs, prefs::kSearchProviderOverrides);
      update->Swap(search_engines.get());
    }

    template_url_service_->RepairPrepopulatedSearchEngines();

    // Reset Google search URL.
    prefs->ClearPref(prefs::kLastPromptedGoogleURL);
    const TemplateURL* default_search_provider =
        template_url_service_->GetDefaultSearchProvider();
    if (default_search_provider &&
        default_search_provider->HasGoogleBaseURLs(
            template_url_service_->search_terms_data())) {
      GoogleURLTracker* tracker =
          GoogleURLTrackerFactory::GetForProfile(profile_);
      if (tracker)
        tracker->RequestServerCheck(true);
    }

    MarkAsDone(DEFAULT_SEARCH_ENGINE);
  } else {
    template_url_service_sub_ =
        template_url_service_->RegisterOnLoadedCallback(
            base::Bind(&ProfileResetter::OnTemplateURLServiceLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    template_url_service_->Load();
  }
}

void ProfileResetter::ResetHomepage() {
  DCHECK(CalledOnValidThread());
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  std::string homepage;
  bool homepage_is_ntp, show_home_button;

  if (master_settings_->GetHomepage(&homepage))
    prefs->SetString(prefs::kHomePage, homepage);

  if (master_settings_->GetHomepageIsNewTab(&homepage_is_ntp))
    prefs->SetBoolean(prefs::kHomePageIsNewTabPage, homepage_is_ntp);
  else
    prefs->ClearPref(prefs::kHomePageIsNewTabPage);

  if (master_settings_->GetShowHomeButton(&show_home_button))
    prefs->SetBoolean(prefs::kShowHomeButton, show_home_button);
  else
    prefs->ClearPref(prefs::kShowHomeButton);
  MarkAsDone(HOMEPAGE);
}

void ProfileResetter::ResetContentSettings() {
  DCHECK(CalledOnValidThread());
  PrefService* prefs = profile_->GetPrefs();
  HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();

  for (int type = 0; type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    map->ClearSettingsForOneType(static_cast<ContentSettingsType>(type));
    if (HostContentSettingsMap::IsSettingAllowedForType(
            prefs,
            CONTENT_SETTING_DEFAULT,
            static_cast<ContentSettingsType>(type)))
      map->SetDefaultContentSetting(static_cast<ContentSettingsType>(type),
                                    CONTENT_SETTING_DEFAULT);
  }
  MarkAsDone(CONTENT_SETTINGS);
}

void ProfileResetter::ResetCookiesAndSiteData() {
  DCHECK(CalledOnValidThread());
  DCHECK(!cookies_remover_);

  cookies_remover_ = BrowsingDataRemover::CreateForUnboundedRange(profile_);
  cookies_remover_->AddObserver(this);
  int remove_mask = BrowsingDataRemover::REMOVE_SITE_DATA |
                    BrowsingDataRemover::REMOVE_CACHE;
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  // Don't try to clear LSO data if it's not supported.
  if (!prefs->GetBoolean(prefs::kClearPluginLSODataEnabled))
    remove_mask &= ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;
  cookies_remover_->Remove(remove_mask, BrowsingDataHelper::UNPROTECTED_WEB);
}

void ProfileResetter::ResetExtensions() {
  DCHECK(CalledOnValidThread());

  std::vector<std::string> brandcode_extensions;
  master_settings_->GetExtensions(&brandcode_extensions);

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(extension_service);
  extension_service->DisableUserExtensions(brandcode_extensions);

  MarkAsDone(EXTENSIONS);
}

void ProfileResetter::ResetStartupPages() {
  DCHECK(CalledOnValidThread());
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  scoped_ptr<base::ListValue> url_list(
      master_settings_->GetUrlsToRestoreOnStartup());
  if (url_list)
    ListPrefUpdate(prefs, prefs::kURLsToRestoreOnStartup)->Swap(url_list.get());

  int restore_on_startup;
  if (master_settings_->GetRestoreOnStartup(&restore_on_startup))
    prefs->SetInteger(prefs::kRestoreOnStartup, restore_on_startup);
  else
    prefs->ClearPref(prefs::kRestoreOnStartup);

  prefs->SetBoolean(prefs::kRestoreOnStartupMigrated, true);
  MarkAsDone(STARTUP_PAGES);
}

void ProfileResetter::ResetPinnedTabs() {
  // Unpin all the tabs.
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (it->is_type_tabbed() && it->profile() == profile_) {
      TabStripModel* tab_model = it->tab_strip_model();
      // Here we assume that indexof(any mini tab) < indexof(any normal tab).
      // If we unpin the tab, it can be moved to the right. Thus traversing in
      // reverse direction is correct.
      for (int i = tab_model->count() - 1; i >= 0; --i) {
        if (tab_model->IsTabPinned(i) && !tab_model->IsAppTab(i))
          tab_model->SetTabPinned(i, false);
      }
    }
  }
  MarkAsDone(PINNED_TABS);
}

void ProfileResetter::ResetShortcuts() {
#if defined(OS_WIN)
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ResetShortcutsOnFileThread),
      base::Bind(&ProfileResetter::MarkAsDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 SHORTCUTS));
#else
  MarkAsDone(SHORTCUTS);
#endif
}

void ProfileResetter::OnTemplateURLServiceLoaded() {
  // TemplateURLService has loaded. If we need to clean search engines, it's
  // time to go on.
  DCHECK(CalledOnValidThread());
  template_url_service_sub_.reset();
  if (pending_reset_flags_ & DEFAULT_SEARCH_ENGINE)
    ResetDefaultSearchEngine();
}

void ProfileResetter::OnBrowsingDataRemoverDone() {
  cookies_remover_ = NULL;
  MarkAsDone(COOKIES_AND_SITE_DATA);
}

std::vector<ShortcutCommand> GetChromeLaunchShortcuts(
    const scoped_refptr<SharedCancellationFlag>& cancel) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
#if defined(OS_WIN)
  // Get full path of chrome.
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return std::vector<ShortcutCommand>();
  BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BROWSER);
  std::vector<ShortcutCommand> shortcuts;
  for (int location = ShellUtil::SHORTCUT_LOCATION_FIRST;
       location < ShellUtil::NUM_SHORTCUT_LOCATIONS; ++location) {
    if (cancel && cancel->data.IsSet())
      break;
    ShellUtil::ShortcutListMaybeRemoveUnknownArgs(
        static_cast<ShellUtil::ShortcutLocation>(location),
        dist,
        ShellUtil::CURRENT_USER,
        chrome_exe,
        false,
        cancel,
        &shortcuts);
  }
  return shortcuts;
#else
  return std::vector<ShortcutCommand>();
#endif
}
