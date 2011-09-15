// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_prefs.h"

#include <string>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/webplugininfo.h"

namespace {

class PluginPrefsWrapper : public ProfileKeyedService {
 public:
  explicit PluginPrefsWrapper(scoped_refptr<PluginPrefs> plugin_prefs)
      : plugin_prefs_(plugin_prefs) {}
  virtual ~PluginPrefsWrapper() {}

  PluginPrefs* plugin_prefs() { return plugin_prefs_.get(); }

 private:
  // ProfileKeyedService methods:
  virtual void Shutdown() OVERRIDE {
    plugin_prefs_->ShutdownOnUIThread();
  }

  scoped_refptr<PluginPrefs> plugin_prefs_;
};

// Default state for a plug-in (not state of the default plug-in!).
// Accessed only on the UI thread.
base::LazyInstance<std::map<FilePath, bool> > g_default_plugin_state(
    base::LINKER_INITIALIZED);

}

// How long to wait to save the plugin enabled information, which might need to
// go to disk.
#define kPluginUpdateDelayMs (60 * 1000)

class PluginPrefs::Factory : public ProfileKeyedServiceFactory {
 public:
  static Factory* GetInstance();

  PluginPrefsWrapper* GetWrapperForProfile(Profile* profile);

  // Factory function for use with
  // ProfileKeyedServiceFactory::SetTestingFactory.
  static ProfileKeyedService* CreateWrapperForProfile(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<Factory>;

  Factory();
  virtual ~Factory() {}

  // ProfileKeyedServiceFactory methods:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() OVERRIDE { return true; }
  virtual bool ServiceIsNULLWhileTesting() OVERRIDE { return true; }
  virtual bool ServiceIsCreatedWithProfile() OVERRIDE { return true; }
};

// static
void PluginPrefs::Initialize() {
  Factory::GetInstance();
}

// static
PluginPrefs* PluginPrefs::GetForProfile(Profile* profile) {
  PluginPrefsWrapper* wrapper =
      Factory::GetInstance()->GetWrapperForProfile(profile);
  if (!wrapper)
    return NULL;
  return wrapper->plugin_prefs();
}

// static
PluginPrefs* PluginPrefs::GetForTestingProfile(Profile* profile) {
  ProfileKeyedService* wrapper =
      Factory::GetInstance()->SetTestingFactoryAndUse(
          profile, &Factory::CreateWrapperForProfile);
  return static_cast<PluginPrefsWrapper*>(wrapper)->plugin_prefs();
}

void PluginPrefs::EnablePluginGroup(bool enabled, const string16& group_name) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &PluginPrefs::EnablePluginGroup,
                          enabled, group_name));
    return;
  }

  webkit::npapi::PluginList* plugin_list =
      webkit::npapi::PluginList::Singleton();
  std::vector<webkit::npapi::PluginGroup> groups;
  plugin_list->GetPluginGroups(true, &groups);

  base::AutoLock auto_lock(lock_);

  // Set the desired state for the group.
  plugin_group_state_[group_name] = enabled;

  // Update the state for all plug-ins in the group.
  for (size_t i = 0; i < groups.size(); ++i) {
    if (groups[i].GetGroupName() != group_name)
      continue;
    const std::vector<webkit::WebPluginInfo>& plugins =
        groups[i].web_plugin_infos();
    for (size_t j = 0; j < plugins.size(); ++j)
      plugin_state_[plugins[j].path] = enabled;
    break;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &PluginPrefs::OnUpdatePreferences, groups));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &PluginPrefs::NotifyPluginStatusChanged));
}

void PluginPrefs::EnablePlugin(bool enabled, const FilePath& path) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &PluginPrefs::EnablePlugin, enabled, path));
    return;
  }

  {
    // Set the desired state for the plug-in.
    base::AutoLock auto_lock(lock_);
    plugin_state_[path] = enabled;
  }

  webkit::npapi::PluginList* plugin_list =
      webkit::npapi::PluginList::Singleton();
  std::vector<webkit::npapi::PluginGroup> groups;
  plugin_list->GetPluginGroups(true, &groups);

  bool found_group = false;
  for (size_t i = 0; i < groups.size(); ++i) {
    bool all_disabled = true;
    const std::vector<webkit::WebPluginInfo>& plugins =
        groups[i].web_plugin_infos();
    for (size_t j = 0; j < plugins.size(); ++j) {
      all_disabled = all_disabled && !IsPluginEnabled(plugins[j]);
      if (plugins[j].path == path) {
        found_group = true;
        DCHECK_EQ(enabled, IsPluginEnabled(plugins[j]));
      }
    }
    if (found_group) {
      // Update the state for the corresponding plug-in group.
      base::AutoLock auto_lock(lock_);
      plugin_group_state_[groups[i].GetGroupName()] = !all_disabled;
      break;
    }
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &PluginPrefs::OnUpdatePreferences, groups));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &PluginPrefs::NotifyPluginStatusChanged));
}

// static
void PluginPrefs::EnablePluginGlobally(bool enable, const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_default_plugin_state.Get()[file_path] = enable;
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (std::vector<Profile*>::iterator it = profiles.begin();
       it != profiles.end(); ++it) {
    PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(*it);
    DCHECK(plugin_prefs);
    plugin_prefs->EnablePlugin(enable, file_path);
  }
}

PluginPrefs::PolicyStatus PluginPrefs::PolicyStatusForPlugin(
    const string16& name) {
  base::AutoLock auto_lock(lock_);
  if (IsStringMatchedInSet(name, policy_enabled_plugin_patterns_)) {
    return POLICY_ENABLED;
  } else if (IsStringMatchedInSet(name, policy_disabled_plugin_patterns_) &&
             !IsStringMatchedInSet(
                 name, policy_disabled_plugin_exception_patterns_)) {
    return POLICY_DISABLED;
  } else {
    return NO_POLICY;
  }
}

bool PluginPrefs::IsPluginEnabled(const webkit::WebPluginInfo& plugin) {
  scoped_ptr<webkit::npapi::PluginGroup> group(
      webkit::npapi::PluginList::Singleton()->GetPluginGroup(plugin));
  string16 group_name = group->GetGroupName();

  // Check if the plug-in or its group is enabled by policy.
  PolicyStatus plugin_status = PolicyStatusForPlugin(plugin.name);
  PolicyStatus group_status = PolicyStatusForPlugin(group_name);
  if (plugin_status == POLICY_ENABLED || group_status == POLICY_ENABLED)
    return true;

  // Check if the plug-in or its group is disabled by policy.
  if (plugin_status == POLICY_DISABLED || group_status == POLICY_DISABLED)
    return false;

  // If enabling NaCl, make sure the plugin is also enabled. See bug
  // http://code.google.com/p/chromium/issues/detail?id=81010 for more
  // information.
  // TODO(dspringer): When NaCl is on by default, remove this code.
  if ((plugin.name ==
       ASCIIToUTF16(chrome::ChromeContentClient::kNaClPluginName)) &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableNaCl)) {
    return true;
  }

  base::AutoLock auto_lock(lock_);
  // Check user preferences for the plug-in.
  std::map<FilePath, bool>::iterator plugin_it =
      plugin_state_.find(plugin.path);
  if (plugin_it != plugin_state_.end())
    return plugin_it->second;

  // Check user preferences for the plug-in group.
  std::map<string16, bool>::iterator group_it(
      plugin_group_state_.find(plugin.name));
  if (group_it != plugin_group_state_.end())
    return group_it->second;

  // Default to enabled.
  return true;
}

void PluginPrefs::Observe(int type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PREF_CHANGED, type);
  const std::string* pref_name = Details<std::string>(details).ptr();
  if (!pref_name) {
    NOTREACHED();
    return;
  }
  DCHECK_EQ(prefs_, Source<PrefService>(source).ptr());
  if (*pref_name == prefs::kPluginsDisabledPlugins) {
    base::AutoLock auto_lock(lock_);
    ListValueToStringSet(prefs_->GetList(prefs::kPluginsDisabledPlugins),
                         &policy_disabled_plugin_patterns_);
  } else if (*pref_name == prefs::kPluginsDisabledPluginsExceptions) {
    base::AutoLock auto_lock(lock_);
    ListValueToStringSet(
        prefs_->GetList(prefs::kPluginsDisabledPluginsExceptions),
        &policy_disabled_plugin_exception_patterns_);
  } else if (*pref_name == prefs::kPluginsEnabledPlugins) {
    base::AutoLock auto_lock(lock_);
    ListValueToStringSet(prefs_->GetList(prefs::kPluginsEnabledPlugins),
                         &policy_enabled_plugin_patterns_);
  } else {
    NOTREACHED();
  }
  NotifyPluginStatusChanged();
}

/*static*/
bool PluginPrefs::IsStringMatchedInSet(const string16& name,
                                       const std::set<string16>& pattern_set) {
  std::set<string16>::const_iterator pattern(pattern_set.begin());
  while (pattern != pattern_set.end()) {
    if (MatchPattern(name, *pattern))
      return true;
    ++pattern;
  }

  return false;
}

/* static */
void PluginPrefs::ListValueToStringSet(const ListValue* src,
                                       std::set<string16>* dest) {
  DCHECK(src);
  DCHECK(dest);
  ListValue::const_iterator end(src->end());
  for (ListValue::const_iterator current(src->begin());
       current != end; ++current) {
    string16 plugin_name;
    if ((*current)->GetAsString(&plugin_name)) {
      dest->insert(plugin_name);
    }
  }
}

void PluginPrefs::SetPrefs(PrefService* prefs) {
  prefs_ = prefs;
  bool update_internal_dir = false;
  FilePath last_internal_dir =
      prefs_->GetFilePath(prefs::kPluginsLastInternalDirectory);
  FilePath cur_internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &cur_internal_dir) &&
      cur_internal_dir != last_internal_dir) {
    update_internal_dir = true;
    prefs_->SetFilePath(
        prefs::kPluginsLastInternalDirectory, cur_internal_dir);
  }

  bool force_enable_internal_pdf = false;
  bool internal_pdf_enabled = false;
  string16 pdf_group_name =
      ASCIIToUTF16(chrome::ChromeContentClient::kPDFPluginName);
  FilePath pdf_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path);
  FilePath::StringType pdf_path_str = pdf_path.value();
  if (!prefs_->GetBoolean(prefs::kPluginsEnabledInternalPDF)) {
    // We switched to the internal pdf plugin being on by default, and so we
    // need to force it to be enabled.  We only want to do it this once though,
    // i.e. we don't want to enable it again if the user disables it afterwards.
    prefs_->SetBoolean(prefs::kPluginsEnabledInternalPDF, true);
    force_enable_internal_pdf = true;
  }

  bool force_enable_nacl = false;
  string16 nacl_group_name =
      ASCIIToUTF16(chrome::ChromeContentClient::kNaClPluginName);
  // Since the NaCl Plugin changed names between Chrome 13 and 14, we need to
  // check for both because either could be stored as the plugin group name.
  string16 old_nacl_group_name =
      ASCIIToUTF16(chrome::ChromeContentClient::kNaClOldPluginName);
  FilePath nacl_path;
  PathService::Get(chrome::FILE_NACL_PLUGIN, &nacl_path);
  FilePath::StringType nacl_path_str = nacl_path.value();
  if (!prefs_->GetBoolean(prefs::kPluginsEnabledNaCl)) {
    // We switched to the nacl plugin being on by default, and so we need to
    // force it to be enabled.  We only want to do it this once though, i.e.
    // we don't want to enable it again if the user disables it afterwards.
    prefs_->SetBoolean(prefs::kPluginsEnabledNaCl, true);
    force_enable_nacl = true;
  }

  {  // Scoped update of prefs::kPluginsPluginsList.
    ListPrefUpdate update(prefs_, prefs::kPluginsPluginsList);
    ListValue* saved_plugins_list = update.Get();
    if (saved_plugins_list && !saved_plugins_list->empty()) {
      for (ListValue::const_iterator it = saved_plugins_list->begin();
           it != saved_plugins_list->end();
           ++it) {
        if (!(*it)->IsType(Value::TYPE_DICTIONARY)) {
          LOG(WARNING) << "Invalid entry in " << prefs::kPluginsPluginsList;
          continue;  // Oops, don't know what to do with this item.
        }

        DictionaryValue* plugin = static_cast<DictionaryValue*>(*it);
        string16 group_name;
        bool enabled;
        if (!plugin->GetBoolean("enabled", &enabled))
          enabled = true;

        FilePath::StringType path;
        // The plugin list constains all the plugin files in addition to the
        // plugin groups.
        if (plugin->GetString("path", &path)) {
          // Files have a path attribute, groups don't.
          FilePath plugin_path(path);
          if (update_internal_dir &&
              FilePath::CompareIgnoreCase(plugin_path.DirName().value(),
                  last_internal_dir.value()) == 0) {
            // If the internal plugin directory has changed and if the plugin
            // looks internal, update its path in the prefs.
            plugin_path = cur_internal_dir.Append(plugin_path.BaseName());
            path = plugin_path.value();
            plugin->SetString("path", path);
          }

          if (FilePath::CompareIgnoreCase(path, pdf_path_str) == 0) {
            if (!enabled && force_enable_internal_pdf) {
              enabled = true;
              plugin->SetBoolean("enabled", true);
            }

            internal_pdf_enabled = enabled;
          } else if (FilePath::CompareIgnoreCase(path, nacl_path_str) == 0) {
            if (!enabled && force_enable_nacl) {
              enabled = true;
              plugin->SetBoolean("enabled", true);
            }
          }

          plugin_state_[plugin_path] = enabled;
        } else if (!enabled && plugin->GetString("name", &group_name)) {
          // Don't disable this group if it's for the pdf or nacl plugins and
          // we just forced it on.
          if (force_enable_internal_pdf && pdf_group_name == group_name)
            continue;
          if (force_enable_nacl && (nacl_group_name == group_name ||
                                    old_nacl_group_name == group_name))
            continue;

          // Otherwise this is a list of groups.
          plugin_group_state_[group_name] = false;
        }
      }
    } else {
      // If the saved plugin list is empty, then the call to UpdatePreferences()
      // below failed in an earlier run, possibly because the user closed the
      // browser too quickly. Try to force enable the internal PDF and nacl
      // plugins again.
      force_enable_internal_pdf = true;
      force_enable_nacl = true;
    }
  }  // Scoped update of prefs::kPluginsPluginsList.

  // Build the set of policy enabled/disabled plugin patterns once and cache it.
  // Don't do this in the constructor, there's no profile available there.
  ListValueToStringSet(prefs_->GetList(prefs::kPluginsDisabledPlugins),
                       &policy_disabled_plugin_patterns_);
  ListValueToStringSet(
      prefs_->GetList(prefs::kPluginsDisabledPluginsExceptions),
      &policy_disabled_plugin_exception_patterns_);
  ListValueToStringSet(prefs_->GetList(prefs::kPluginsEnabledPlugins),
                       &policy_enabled_plugin_patterns_);

  registrar_.Init(prefs_);
  registrar_.Add(prefs::kPluginsDisabledPlugins, this);
  registrar_.Add(prefs::kPluginsDisabledPluginsExceptions, this);
  registrar_.Add(prefs::kPluginsEnabledPlugins, this);

  if (force_enable_internal_pdf || internal_pdf_enabled) {
    // See http://crbug.com/50105 for background.
    plugin_group_state_[ASCIIToUTF16(
        webkit::npapi::PluginGroup::kAdobeReaderGroupName)] = false;
  }

  if (force_enable_internal_pdf || force_enable_nacl) {
    // We want to save this, but doing so requires loading the list of plugins,
    // so do it after a minute as to not impact startup performance.  Note that
    // plugins are loaded after 30s by the metrics service.
    BrowserThread::PostDelayedTask(
        BrowserThread::FILE,
        FROM_HERE,
        NewRunnableMethod(this, &PluginPrefs::GetPreferencesDataOnFileThread),
        kPluginUpdateDelayMs);
  }

  NotifyPluginStatusChanged();
}

void PluginPrefs::ShutdownOnUIThread() {
  prefs_ = NULL;
  registrar_.RemoveAll();
}

// static
PluginPrefs::Factory* PluginPrefs::Factory::GetInstance() {
  return Singleton<PluginPrefs::Factory>::get();
}

PluginPrefsWrapper* PluginPrefs::Factory::GetWrapperForProfile(
    Profile* profile) {
  return static_cast<PluginPrefsWrapper*>(GetServiceForProfile(profile, true));
}

// static
ProfileKeyedService* PluginPrefs::Factory::CreateWrapperForProfile(
    Profile* profile) {
  return GetInstance()->BuildServiceInstanceFor(profile);
}

PluginPrefs::Factory::Factory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

ProfileKeyedService* PluginPrefs::Factory::BuildServiceInstanceFor(
    Profile* profile) const {
  scoped_refptr<PluginPrefs> plugin_prefs(new PluginPrefs());
  plugin_prefs->SetPrefs(profile->GetPrefs());
  return new PluginPrefsWrapper(plugin_prefs);
}

PluginPrefs::PluginPrefs() : plugin_state_(g_default_plugin_state.Get()),
                             prefs_(NULL) {
}

PluginPrefs::~PluginPrefs() {
}

void PluginPrefs::SetPolicyEnforcedPluginPatterns(
    const std::set<string16>& disabled_patterns,
    const std::set<string16>& disabled_exception_patterns,
    const std::set<string16>& enabled_patterns) {
  policy_disabled_plugin_patterns_ = disabled_patterns;
  policy_disabled_plugin_exception_patterns_ = disabled_exception_patterns;
  policy_enabled_plugin_patterns_ = enabled_patterns;
}

void PluginPrefs::GetPreferencesDataOnFileThread() {
  std::vector<webkit::npapi::PluginGroup> groups;

  webkit::npapi::PluginList* plugin_list =
      webkit::npapi::PluginList::Singleton();
  plugin_list->GetPluginGroups(false, &groups);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &PluginPrefs::OnUpdatePreferences, groups));
}

void PluginPrefs::OnUpdatePreferences(
    std::vector<webkit::npapi::PluginGroup> groups) {
  if (!prefs_)
    return;

  ListPrefUpdate update(prefs_, prefs::kPluginsPluginsList);
  ListValue* plugins_list = update.Get();
  plugins_list->Clear();

  FilePath internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir))
    prefs_->SetFilePath(prefs::kPluginsLastInternalDirectory, internal_dir);

  base::AutoLock auto_lock(lock_);

  // Add the plug-in groups.
  for (size_t i = 0; i < groups.size(); ++i) {
    // Add the plugin files to the same list.
    const std::vector<webkit::WebPluginInfo>& plugins =
        groups[i].web_plugin_infos();
    for (size_t j = 0; j < plugins.size(); ++j) {
      DictionaryValue* summary = new DictionaryValue();
      summary->SetString("path", plugins[j].path.value());
      summary->SetString("name", plugins[j].name);
      summary->SetString("version", plugins[j].version);
      bool enabled = true;
      std::map<FilePath, bool>::iterator it =
          plugin_state_.find(plugins[j].path);
      if (it != plugin_state_.end())
        enabled = it->second;
      summary->SetBoolean("enabled", enabled);
      plugins_list->Append(summary);
    }

    DictionaryValue* summary = new DictionaryValue();
    string16 name = groups[i].GetGroupName();
    summary->SetString("name", name);
    bool enabled = true;
    std::map<string16, bool>::iterator it =
        plugin_group_state_.find(name);
    if (it != plugin_group_state_.end())
      enabled = it->second;
    summary->SetBoolean("enabled", enabled);
    plugins_list->Append(summary);
  }
}

void PluginPrefs::NotifyPluginStatusChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
      Source<PluginPrefs>(this),
      NotificationService::NoDetails());
}

/*static*/
void PluginPrefs::RegisterPrefs(PrefService* prefs) {
  FilePath internal_dir;
  PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir);
  prefs->RegisterFilePathPref(prefs::kPluginsLastInternalDirectory,
                              internal_dir,
                              PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsEnabledInternalPDF,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsEnabledNaCl,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPluginsPluginsList,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPluginsDisabledPlugins,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPluginsDisabledPluginsExceptions,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPluginsEnabledPlugins,
                          PrefService::UNSYNCABLE_PREF);
}
