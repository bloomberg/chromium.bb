// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_service.h"

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/managed_mode/managed_mode_site_list.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

ManagedUserService::URLFilterContext::URLFilterContext()
    : ui_url_filter_(new ManagedModeURLFilter),
      io_url_filter_(new ManagedModeURLFilter) {}
ManagedUserService::URLFilterContext::~URLFilterContext() {}

ManagedModeURLFilter*
ManagedUserService::URLFilterContext::ui_url_filter() const {
  return ui_url_filter_.get();
}

ManagedModeURLFilter*
ManagedUserService::URLFilterContext::io_url_filter() const {
  return io_url_filter_.get();
}

void ManagedUserService::URLFilterContext::SetDefaultFilteringBehavior(
    ManagedModeURLFilter::FilteringBehavior behavior) {
  ui_url_filter_->SetDefaultFilteringBehavior(behavior);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeURLFilter::SetDefaultFilteringBehavior,
                 io_url_filter_.get(),
                 behavior));
}

void ManagedUserService::URLFilterContext::LoadWhitelists(
    ScopedVector<ManagedModeSiteList> site_lists) {
  // ManagedModeURLFilter::LoadWhitelists takes ownership of |site_lists|,
  // so we make an additional copy of it.
  /// TODO(bauerb): This is kinda ugly.
  ScopedVector<ManagedModeSiteList> site_lists_copy;
  for (ScopedVector<ManagedModeSiteList>::iterator it = site_lists.begin();
       it != site_lists.end(); ++it) {
    site_lists_copy.push_back((*it)->Clone());
  }
  ui_url_filter_->LoadWhitelists(site_lists.Pass());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeURLFilter::LoadWhitelists,
                 io_url_filter_,
                 base::Passed(&site_lists_copy)));
}

void ManagedUserService::URLFilterContext::SetManualLists(
    scoped_ptr<ListValue> whitelist,
    scoped_ptr<ListValue> blacklist) {
  ui_url_filter_->SetManualLists(whitelist.get(), blacklist.get());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeURLFilter::SetManualLists,
                 io_url_filter_,
                 base::Owned(whitelist.release()),
                 base::Owned(blacklist.release())));
}

void ManagedUserService::URLFilterContext::AddURLPatternToManualList(
    const bool is_whitelist,
    const std::string& url) {
  ui_url_filter_->AddURLPatternToManualList(is_whitelist, url);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeURLFilter::AddURLPatternToManualList,
                 io_url_filter_,
                 is_whitelist,
                 url));
}

ManagedUserService::ManagedUserService(Profile* profile)
    : profile_(profile),
      is_elevated_(false) {
  Init();
}

ManagedUserService::~ManagedUserService() {
}

bool ManagedUserService::ProfileIsManaged() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kProfileIsManaged);
}

// static
void ManagedUserService::RegisterUserPrefs(PrefServiceSyncable* prefs) {
  prefs->RegisterListPref(prefs::kManagedModeWhitelist,
                          PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kManagedModeBlacklist,
                          PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDefaultManagedModeFilteringBehavior,
                             ManagedModeURLFilter::BLOCK,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
}

scoped_refptr<const ManagedModeURLFilter>
ManagedUserService::GetURLFilterForIOThread() {
  return url_filter_context_.io_url_filter();
}

ManagedModeURLFilter* ManagedUserService::GetURLFilterForUIThread() {
  return url_filter_context_.ui_url_filter();
}

// Items not on any list must return -1 (CATEGORY_NOT_ON_LIST in history.js).
// Items on a list, but with no category, must return 0 (CATEGORY_OTHER).
#define CATEGORY_NOT_ON_LIST -1;
#define CATEGORY_OTHER 0;

int ManagedUserService::GetCategory(const GURL& url) {
  std::vector<ManagedModeSiteList::Site*> sites;
  GetURLFilterForUIThread()->GetSites(url, &sites);
  if (sites.empty())
    return CATEGORY_NOT_ON_LIST;

  return (*sites.begin())->category_id;
}

// static
void ManagedUserService::GetCategoryNames(CategoryList* list) {
  ManagedModeSiteList::GetCategoryNames(list);
};

void ManagedUserService::AddToManualList(bool is_whitelist,
                                         const base::ListValue& list) {
  ListPrefUpdate pref_update(profile_->GetPrefs(),
                             is_whitelist ? prefs::kManagedModeWhitelist :
                                            prefs::kManagedModeBlacklist);
  ListValue* pref_list = pref_update.Get();

  for (size_t i = 0; i < list.GetSize(); ++i) {
    std::string url_pattern;
    list.GetString(i, &url_pattern);

    if (!IsInManualList(is_whitelist, url_pattern)) {
      pref_list->AppendString(url_pattern);
      AddURLPatternToManualList(is_whitelist, url_pattern);
    }
  }
}

void ManagedUserService::RemoveFromManualList(bool is_whitelist,
                                              const base::ListValue& list) {
  ListPrefUpdate pref_update(profile_->GetPrefs(),
                             is_whitelist ? prefs::kManagedModeWhitelist :
                                            prefs::kManagedModeBlacklist);
  ListValue* pref_list = pref_update.Get();

  for (size_t i = 0; i < list.GetSize(); ++i) {
    std::string pattern;
    size_t out_index;
    list.GetString(i, &pattern);
    StringValue value_to_remove(pattern);

    pref_list->Remove(value_to_remove, &out_index);
  }
}

bool ManagedUserService::IsInManualList(bool is_whitelist,
                                        const std::string& url_pattern) {
  StringValue pattern(url_pattern);
  const ListValue* list = profile_->GetPrefs()->GetList(
      is_whitelist ? prefs::kManagedModeWhitelist :
                     prefs::kManagedModeBlacklist);
  return list->Find(pattern) != list->end();
}

// static
scoped_ptr<base::ListValue> ManagedUserService::GetBlacklist() {
  return make_scoped_ptr(
      profile_->GetPrefs()->GetList(prefs::kManagedModeBlacklist)->DeepCopy());
}

std::string ManagedUserService::GetDebugPolicyProviderName() const {
  // Save the string space in official builds.
#ifdef NDEBUG
  NOTREACHED();
  return std::string();
#else
  return "Managed User Service";
#endif
}

bool ManagedUserService::UserMayLoad(const extensions::Extension* extension,
                                     string16* error) const {
  string16 tmp_error;
  if (ExtensionManagementPolicyImpl(&tmp_error))
    return true;

  // If the extension is already loaded, we allow it, otherwise we'd unload
  // all existing extensions.
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();

  // |extension_service| can be NULL in a unit test.
  if (extension_service &&
      extension_service->GetInstalledExtension(extension->id()))
    return true;

  if (error)
    *error = tmp_error;
  return false;
}

bool ManagedUserService::UserMayModifySettings(
    const extensions::Extension* extension,
    string16* error) const {
  return ExtensionManagementPolicyImpl(error);
}

void ManagedUserService::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const extensions::Extension* extension =
          content::Details<extensions::Extension>(details).ptr();
      if (!extension->GetContentPackSiteList().empty())
        UpdateSiteLists();

      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const extensions::UnloadedExtensionInfo* extension_info =
          content::Details<extensions::UnloadedExtensionInfo>(details).ptr();
      if (!extension_info->extension->GetContentPackSiteList().empty())
        UpdateSiteLists();

      break;
    }
    default:
      NOTREACHED();
  }
}

bool ManagedUserService::ExtensionManagementPolicyImpl(string16* error) const {
  if (!ProfileIsManaged())
    return true;

  if (is_elevated_)
    return true;

  if (error)
    *error = l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOCKED_MANAGED_MODE);
  return false;
}

ScopedVector<ManagedModeSiteList> ManagedUserService::GetActiveSiteLists() {
  ScopedVector<ManagedModeSiteList> site_lists;
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  // Can be NULL in unit tests.
  if (!extension_service)
    return site_lists.Pass();

  const ExtensionSet* extensions = extension_service->extensions();
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const extensions::Extension* extension = *it;
    if (!extension_service->IsExtensionEnabled(extension->id()))
      continue;

    ExtensionResource site_list = extension->GetContentPackSiteList();
    if (!site_list.empty())
      site_lists.push_back(new ManagedModeSiteList(extension->id(), site_list));
  }

  return site_lists.Pass();
}

void ManagedUserService::OnDefaultFilteringBehaviorChanged() {
  DCHECK(ProfileIsManaged());

  int behavior_value = profile_->GetPrefs()->GetInteger(
      prefs::kDefaultManagedModeFilteringBehavior);
  ManagedModeURLFilter::FilteringBehavior behavior =
      ManagedModeURLFilter::BehaviorFromInt(behavior_value);
  url_filter_context_.SetDefaultFilteringBehavior(behavior);
}

void ManagedUserService::UpdateSiteLists() {
  url_filter_context_.LoadWhitelists(GetActiveSiteLists());
}

void ManagedUserService::UpdateManualLists() {
  url_filter_context_.SetManualLists(GetWhitelist(), GetBlacklist());
}

void ManagedUserService::SetElevatedForTesting(bool is_elevated) {
  is_elevated_ = is_elevated;
}

void ManagedUserService::Init() {
  if (!ProfileIsManaged())
    return;

  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile_);
  extensions::ManagementPolicy* management_policy =
      extension_system->management_policy();
  if (management_policy)
    extension_system->management_policy()->RegisterProvider(this);

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kDefaultManagedModeFilteringBehavior,
      base::Bind(
          &ManagedUserService::OnDefaultFilteringBehaviorChanged,
          base::Unretained(this)));

  // Initialize the filter.
  OnDefaultFilteringBehaviorChanged();
  UpdateSiteLists();
  UpdateManualLists();
}

scoped_ptr<base::ListValue> ManagedUserService::GetWhitelist() {
  return make_scoped_ptr(
      profile_->GetPrefs()->GetList(prefs::kManagedModeWhitelist)->DeepCopy());
}

void ManagedUserService::AddURLPatternToManualList(
    bool is_whitelist,
    const std::string& url_pattern) {
  url_filter_context_.AddURLPatternToManualList(true, url_pattern);
}
