// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_service.h"

#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/managed_mode/managed_mode_site_list.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
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
                 io_url_filter_.get(), behavior));
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
                 io_url_filter_, base::Passed(&site_lists_copy)));
}

void ManagedUserService::URLFilterContext::SetManualHosts(
    scoped_ptr<std::map<std::string, bool> > host_map) {
  ui_url_filter_->SetManualHosts(host_map.get());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeURLFilter::SetManualHosts,
                 io_url_filter_, base::Owned(host_map.release())));
}

void ManagedUserService::URLFilterContext::SetManualURLs(
    scoped_ptr<std::map<GURL, bool> > url_map) {
  ui_url_filter_->SetManualURLs(url_map.get());
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ManagedModeURLFilter::SetManualURLs,
                 io_url_filter_, base::Owned(url_map.release())));
}

ManagedUserService::ManagedUserService(Profile* profile)
    : profile_(profile),
      is_elevated_(false) {
}

ManagedUserService::~ManagedUserService() {
}

bool ManagedUserService::ProfileIsManaged() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kProfileIsManaged);
}

bool ManagedUserService::IsElevated() const {
  PrefService* pref_service = profile_->GetPrefs();
  // If there is no passphrase set, the profile is considered to be elevated.
  if (pref_service->GetString(prefs::kManagedModeLocalPassphrase).empty())
    return true;
  return is_elevated_;
}

// static
void ManagedUserService::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kManagedModeManualHosts,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kManagedModeManualURLs,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kDefaultManagedModeFilteringBehavior,
                                ManagedModeURLFilter::BLOCK,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kManagedModeLocalPassphrase,
                               "",
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kManagedModeLocalSalt,
                               "",
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
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

  if (extension) {
    bool was_installed_by_default = extension->was_installed_by_default();
#ifdef OS_CHROMEOS
    // On Chrome OS all external sources are controlled by us so it means that
    // they are "default". Method was_installed_by_default returns false because
    // extensions creation flags are ignored in case of default extensions with
    // update URL(the flags aren't passed to OnExternalExtensionUpdateUrlFound).
    // TODO(dpolukhin): remove this Chrome OS specific code as soon as creation
    // flags are not ignored.
    was_installed_by_default =
        extensions::Manifest::IsExternalLocation(extension->location());
#endif
    if (extension->location() == extensions::Manifest::COMPONENT ||
        was_installed_by_default) {
      return true;
    }
  }

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

ManagedUserService::ManualBehavior ManagedUserService::GetManualBehaviorForHost(
    const std::string& hostname) {
  const DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kManagedModeManualHosts);
  bool allow = false;
  if (!dict->GetBooleanWithoutPathExpansion(hostname, &allow))
    return MANUAL_NONE;

  return allow ? MANUAL_ALLOW : MANUAL_BLOCK;
}

void ManagedUserService::SetManualBehaviorForHosts(
    const std::vector<std::string>& hostnames,
    ManualBehavior behavior) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kManagedModeManualHosts);
  DictionaryValue* dict = update.Get();
  for (std::vector<std::string>::const_iterator it = hostnames.begin();
       it != hostnames.end(); ++it) {
    if (behavior == MANUAL_NONE)
      dict->RemoveWithoutPathExpansion(*it, NULL);
    else
      dict->SetBooleanWithoutPathExpansion(*it, behavior == MANUAL_ALLOW);
  }

  UpdateManualHosts();
}

ManagedUserService::ManualBehavior ManagedUserService::GetManualBehaviorForURL(
    const GURL& url) {
  const DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kManagedModeManualURLs);
  GURL normalized_url = ManagedModeURLFilter::Normalize(url);
  bool allow = false;
  if (!dict->GetBooleanWithoutPathExpansion(normalized_url.spec(), &allow))
    return MANUAL_NONE;

  return allow ? MANUAL_ALLOW : MANUAL_BLOCK;
}

void ManagedUserService::SetManualBehaviorForURLs(const std::vector<GURL>& urls,
                                                  ManualBehavior behavior) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kManagedModeManualURLs);
  DictionaryValue* dict = update.Get();

  for (std::vector<GURL>::const_iterator it = urls.begin(); it != urls.end();
       ++it) {
    GURL url = ManagedModeURLFilter::Normalize(*it);
    if (behavior == MANUAL_NONE) {
      dict->RemoveWithoutPathExpansion(url.spec(), NULL);
    } else {
      dict->SetBooleanWithoutPathExpansion(url.spec(),
                                           behavior == MANUAL_ALLOW);
    }
  }

  UpdateManualURLs();
}

void ManagedUserService::SetElevated(bool is_elevated) {
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
  UpdateManualHosts();
  UpdateManualURLs();
}

void ManagedUserService::UpdateManualHosts() {
  const DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kManagedModeManualHosts);
  scoped_ptr<std::map<std::string, bool> > host_map(
      new std::map<std::string, bool>());
  for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    bool allow = false;
    bool result = it.value().GetAsBoolean(&allow);
    DCHECK(result);
    (*host_map)[it.key()] = allow;
  }
  url_filter_context_.SetManualHosts(host_map.Pass());
}

void ManagedUserService::UpdateManualURLs() {
  const DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kManagedModeManualURLs);
  scoped_ptr<std::map<GURL, bool> > url_map(new std::map<GURL, bool>());
  for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    bool allow = false;
    bool result = it.value().GetAsBoolean(&allow);
    DCHECK(result);
    (*url_map)[GURL(it.key())] = allow;
  }
  url_filter_context_.SetManualURLs(url_map.Pass());
}
