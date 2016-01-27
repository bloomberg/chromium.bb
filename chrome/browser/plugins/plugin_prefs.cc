// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_prefs.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/plugins/plugin_installer.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#endif

using content::BrowserThread;
using content::PluginService;

namespace {

bool IsComponentUpdatedPepperFlash(const base::FilePath& plugin) {
  if (plugin.BaseName().value() == chrome::kPepperFlashPluginFilename) {
    base::FilePath component_updated_pepper_flash_dir;
    if (PathService::Get(chrome::DIR_COMPONENT_UPDATED_PEPPER_FLASH_PLUGIN,
                         &component_updated_pepper_flash_dir) &&
        component_updated_pepper_flash_dir.IsParent(plugin)) {
      return true;
    }
  }

  return false;
}

}  // namespace

PluginPrefs::PluginState::PluginState() {
}

PluginPrefs::PluginState::~PluginState() {
}

bool PluginPrefs::PluginState::Get(const base::FilePath& plugin,
                                   bool* enabled) const {
  base::FilePath key = ConvertMapKey(plugin);
  std::map<base::FilePath, bool>::const_iterator iter = state_.find(key);
  if (iter != state_.end()) {
    *enabled = iter->second;
    return true;
  }
  return false;
}

void PluginPrefs::PluginState::Set(const base::FilePath& plugin, bool enabled) {
  state_[ConvertMapKey(plugin)] = enabled;
}

base::FilePath PluginPrefs::PluginState::ConvertMapKey(
    const base::FilePath& plugin) const {
  // Keep the state of component-updated and bundled Pepper Flash in sync.
  if (IsComponentUpdatedPepperFlash(plugin)) {
    base::FilePath bundled_pepper_flash;
    if (PathService::Get(chrome::FILE_PEPPER_FLASH_PLUGIN,
                         &bundled_pepper_flash)) {
      return bundled_pepper_flash;
    }
  }

  return plugin;
}

// static
scoped_refptr<PluginPrefs> PluginPrefs::GetForProfile(Profile* profile) {
  return PluginPrefsFactory::GetPrefsForProfile(profile);
}

// static
scoped_refptr<PluginPrefs> PluginPrefs::GetForTestingProfile(
    Profile* profile) {
  return static_cast<PluginPrefs*>(
      PluginPrefsFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, &PluginPrefsFactory::CreateForTestingProfile).get());
}

void PluginPrefs::EnablePluginGroup(bool enabled,
                                    const base::string16& group_name) {
  PluginService::GetInstance()->GetPlugins(
      base::Bind(&PluginPrefs::EnablePluginGroupInternal,
                 this, enabled, group_name));
}

void PluginPrefs::EnablePluginGroupInternal(
    bool enabled,
    const base::string16& group_name,
    const std::vector<content::WebPluginInfo>& plugins) {
  base::AutoLock auto_lock(lock_);
  PluginFinder* finder = PluginFinder::GetInstance();

  // Set the desired state for the group.
  plugin_group_state_[group_name] = enabled;

  // Update the state for all plugins in the group.
  for (size_t i = 0; i < plugins.size(); ++i) {
    scoped_ptr<PluginMetadata> plugin(finder->GetPluginMetadata(plugins[i]));
    if (group_name != plugin->name())
      continue;
    plugin_state_.Set(plugins[i].path, enabled);
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&PluginPrefs::OnUpdatePreferences, this, plugins));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&PluginPrefs::NotifyPluginStatusChanged, this));
}

void PluginPrefs::EnablePlugin(
    bool enabled, const base::FilePath& path,
    const base::Callback<void(bool)>& callback) {
  PluginFinder* finder = PluginFinder::GetInstance();
  content::WebPluginInfo plugin;
  bool can_enable = true;
  if (PluginService::GetInstance()->GetPluginInfoByPath(path, &plugin)) {
    scoped_ptr<PluginMetadata> plugin_metadata(
        finder->GetPluginMetadata(plugin));
    PolicyStatus plugin_status = PolicyStatusForPlugin(plugin.name);
    PolicyStatus group_status = PolicyStatusForPlugin(plugin_metadata->name());
    if (enabled) {
      if (plugin_status == POLICY_DISABLED || group_status == POLICY_DISABLED)
        can_enable = false;
    } else {
      if (plugin_status == POLICY_ENABLED || group_status == POLICY_ENABLED)
        can_enable = false;
    }
  } else {
    NOTREACHED();
  }

  if (!can_enable) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }

  PluginService::GetInstance()->GetPlugins(
      base::Bind(&PluginPrefs::EnablePluginInternal, this,
                 enabled, path, finder, callback));
}

void PluginPrefs::EnablePluginInternal(
    bool enabled,
    const base::FilePath& path,
    PluginFinder* plugin_finder,
    const base::Callback<void(bool)>& callback,
    const std::vector<content::WebPluginInfo>& plugins) {
  {
    // Set the desired state for the plugin.
    base::AutoLock auto_lock(lock_);
    plugin_state_.Set(path, enabled);
  }

  base::string16 group_name;
  for (size_t i = 0; i < plugins.size(); ++i) {
    if (plugins[i].path == path) {
      scoped_ptr<PluginMetadata> plugin_metadata(
          plugin_finder->GetPluginMetadata(plugins[i]));
      // set the group name for this plugin.
      group_name = plugin_metadata->name();
      DCHECK_EQ(enabled, IsPluginEnabled(plugins[i]));
      break;
    }
  }

  bool all_disabled = true;
  for (size_t i = 0; i < plugins.size(); ++i) {
    scoped_ptr<PluginMetadata> plugin_metadata(
        plugin_finder->GetPluginMetadata(plugins[i]));
    DCHECK(!plugin_metadata->name().empty());
    if (group_name == plugin_metadata->name()) {
      all_disabled = all_disabled && !IsPluginEnabled(plugins[i]);
    }
  }

  if (!group_name.empty()) {
    // Update the state for the corresponding plugin group.
    base::AutoLock auto_lock(lock_);
    plugin_group_state_[group_name] = !all_disabled;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&PluginPrefs::OnUpdatePreferences, this, plugins));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&PluginPrefs::NotifyPluginStatusChanged, this));
  callback.Run(true);
}

PluginPrefs::PolicyStatus PluginPrefs::PolicyStatusForPlugin(
    const base::string16& name) const {
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

bool PluginPrefs::IsPluginEnabled(const content::WebPluginInfo& plugin) const {
  scoped_ptr<PluginMetadata> plugin_metadata(
      PluginFinder::GetInstance()->GetPluginMetadata(plugin));
  base::string16 group_name = plugin_metadata->name();

  // Check if the plugin or its group is enabled by policy.
  PolicyStatus plugin_status = PolicyStatusForPlugin(plugin.name);
  PolicyStatus group_status = PolicyStatusForPlugin(group_name);
  if (plugin_status == POLICY_ENABLED || group_status == POLICY_ENABLED)
    return true;

  // Check if the plugin or its group is disabled by policy.
  if (plugin_status == POLICY_DISABLED || group_status == POLICY_DISABLED)
    return false;

#if !defined(DISABLE_NACL)
  // If enabling NaCl, make sure the plugin is also enabled. See bug
  // http://code.google.com/p/chromium/issues/detail?id=81010 for more
  // information.
  // TODO(dspringer): When NaCl is on by default, remove this code.
  if ((plugin.name == base::ASCIIToUTF16(nacl::kNaClPluginName)) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNaCl)) {
    return true;
  }
#endif

  base::AutoLock auto_lock(lock_);
  // Check user preferences for the plugin.
  bool plugin_enabled = false;
  if (plugin_state_.Get(plugin.path, &plugin_enabled))
    return plugin_enabled;

  // Check user preferences for the plugin group.
  std::map<base::string16, bool>::const_iterator group_it(
      plugin_group_state_.find(group_name));
  if (group_it != plugin_group_state_.end())
    return group_it->second;

  // Default to enabled.
  return true;
}

void PluginPrefs::UpdatePatternsAndNotify(std::set<base::string16>* patterns,
                                          const std::string& pref_name) {
  base::AutoLock auto_lock(lock_);
  ListValueToStringSet(prefs_->GetList(pref_name.c_str()), patterns);

  NotifyPluginStatusChanged();
}

/*static*/
bool PluginPrefs::IsStringMatchedInSet(
    const base::string16& name,
    const std::set<base::string16>& pattern_set) {
  std::set<base::string16>::const_iterator pattern(pattern_set.begin());
  while (pattern != pattern_set.end()) {
    if (base::MatchPattern(name, *pattern))
      return true;
    ++pattern;
  }

  return false;
}

/* static */
void PluginPrefs::ListValueToStringSet(const base::ListValue* src,
                                       std::set<base::string16>* dest) {
  DCHECK(src);
  DCHECK(dest);
  dest->clear();
  base::ListValue::const_iterator end(src->end());
  for (base::ListValue::const_iterator current(src->begin());
       current != end; ++current) {
    base::string16 plugin_name;
    if ((*current)->GetAsString(&plugin_name)) {
      dest->insert(plugin_name);
    }
  }
}

void PluginPrefs::SetPrefs(PrefService* prefs) {
  prefs_ = prefs;
  bool update_internal_dir = false;
  base::FilePath last_internal_dir =
      prefs_->GetFilePath(prefs::kPluginsLastInternalDirectory);
  base::FilePath cur_internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &cur_internal_dir) &&
      cur_internal_dir != last_internal_dir) {
    update_internal_dir = true;
    prefs_->SetFilePath(
        prefs::kPluginsLastInternalDirectory, cur_internal_dir);
  }

  {  // Scoped update of prefs::kPluginsPluginsList.
    ListPrefUpdate update(prefs_, prefs::kPluginsPluginsList);
    base::ListValue* saved_plugins_list = update.Get();
    if (saved_plugins_list && !saved_plugins_list->empty()) {
      for (base::Value* plugin_value : *saved_plugins_list) {
        if (!plugin_value->IsType(base::Value::TYPE_DICTIONARY)) {
          LOG(WARNING) << "Invalid entry in " << prefs::kPluginsPluginsList;
          continue;  // Oops, don't know what to do with this item.
        }

        base::DictionaryValue* plugin =
            static_cast<base::DictionaryValue*>(plugin_value);
        base::string16 group_name;
        bool enabled;
        if (!plugin->GetBoolean("enabled", &enabled))
          enabled = true;

        base::FilePath::StringType path;
        // The plugin list constains all the plugin files in addition to the
        // plugin groups.
        if (plugin->GetString("path", &path)) {
          // Files have a path attribute, groups don't.
          base::FilePath plugin_path(path);

          // The path to the internal plugin directory changes everytime Chrome
          // is auto-updated, since it contains the current version number. For
          // example, it changes from foobar\Chrome\Application\21.0.1180.83 to
          // foobar\Chrome\Application\21.0.1180.89.
          // However, we would like the settings of internal plugins to persist
          // across Chrome updates. Therefore, we need to recognize those paths
          // that are within the previous internal plugin directory, and update
          // them in the prefs accordingly.
          if (update_internal_dir) {
            base::FilePath relative_path;

            // Extract the part of |plugin_path| that is relative to
            // |last_internal_dir|. For example, |relative_path| will be
            // foo\bar.dll if |plugin_path| is <last_internal_dir>\foo\bar.dll.
            //
            // Every iteration the last path component from |plugin_path| is
            // removed and prepended to |relative_path| until we get up to
            // |last_internal_dir|.
            while (last_internal_dir.IsParent(plugin_path)) {
              relative_path = plugin_path.BaseName().Append(relative_path);

              base::FilePath old_path = plugin_path;
              plugin_path = plugin_path.DirName();
              // To be extra sure that we won't end up in an infinite loop.
              if (old_path == plugin_path) {
                NOTREACHED();
                break;
              }
            }

            // If |relative_path| is empty, |plugin_path| is not within
            // |last_internal_dir|. We don't need to update it.
            if (!relative_path.empty()) {
              plugin_path = cur_internal_dir.Append(relative_path);
              path = plugin_path.value();
              plugin->SetString("path", path);
            }
          }

          plugin_state_.Set(plugin_path, enabled);
        } else if (!enabled && plugin->GetString("name", &group_name)) {
          // Otherwise this is a list of groups.
          plugin_group_state_[group_name] = false;
        }
      }
    } else {
      // If the saved plugin list is empty, then the call to UpdatePreferences()
      // below failed in an earlier run, possibly because the user closed the
      // browser too quickly.

      // Only want one PDF plugin enabled at a time. See http://crbug.com/50105
      // for background.
      plugin_group_state_[base::ASCIIToUTF16(
          PluginMetadata::kAdobeReaderGroupName)] = false;
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

  // Because pointers to our own members will remain unchanged for the
  // lifetime of |registrar_| (which we also own), we can bind their
  // pointer values directly in the callbacks to avoid string-based
  // lookups at notification time.
  registrar_.Add(prefs::kPluginsDisabledPlugins,
                 base::Bind(&PluginPrefs::UpdatePatternsAndNotify,
                            base::Unretained(this),
                            &policy_disabled_plugin_patterns_));
  registrar_.Add(prefs::kPluginsDisabledPluginsExceptions,
                 base::Bind(&PluginPrefs::UpdatePatternsAndNotify,
                            base::Unretained(this),
                            &policy_disabled_plugin_exception_patterns_));
  registrar_.Add(prefs::kPluginsEnabledPlugins,
                 base::Bind(&PluginPrefs::UpdatePatternsAndNotify,
                            base::Unretained(this),
                            &policy_enabled_plugin_patterns_));

  NotifyPluginStatusChanged();
}

void PluginPrefs::ShutdownOnUIThread() {
  prefs_ = NULL;
  registrar_.RemoveAll();
}

PluginPrefs::PluginPrefs() : profile_(NULL),
                             prefs_(NULL) {
}

PluginPrefs::~PluginPrefs() {
}

void PluginPrefs::SetPolicyEnforcedPluginPatterns(
    const std::set<base::string16>& disabled_patterns,
    const std::set<base::string16>& disabled_exception_patterns,
    const std::set<base::string16>& enabled_patterns) {
  policy_disabled_plugin_patterns_ = disabled_patterns;
  policy_disabled_plugin_exception_patterns_ = disabled_exception_patterns;
  policy_enabled_plugin_patterns_ = enabled_patterns;
}

void PluginPrefs::OnUpdatePreferences(
    const std::vector<content::WebPluginInfo>& plugins) {
  if (!prefs_)
    return;

  PluginFinder* finder = PluginFinder::GetInstance();
  ListPrefUpdate update(prefs_, prefs::kPluginsPluginsList);
  base::ListValue* plugins_list = update.Get();
  plugins_list->Clear();

  base::FilePath internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir))
    prefs_->SetFilePath(prefs::kPluginsLastInternalDirectory, internal_dir);

  base::AutoLock auto_lock(lock_);

  // Add the plugin files.
  std::set<base::string16> group_names;
  for (size_t i = 0; i < plugins.size(); ++i) {
    base::DictionaryValue* summary = new base::DictionaryValue();
    summary->SetString("path", plugins[i].path.value());
    summary->SetString("name", plugins[i].name);
    summary->SetString("version", plugins[i].version);
    bool enabled = true;
    plugin_state_.Get(plugins[i].path, &enabled);
    summary->SetBoolean("enabled", enabled);
    plugins_list->Append(summary);

    scoped_ptr<PluginMetadata> plugin_metadata(
        finder->GetPluginMetadata(plugins[i]));
    // Insert into a set of all group names.
    group_names.insert(plugin_metadata->name());
  }

  // Add the plugin groups.
  for (std::set<base::string16>::const_iterator it = group_names.begin();
      it != group_names.end(); ++it) {
    base::DictionaryValue* summary = new base::DictionaryValue();
    summary->SetString("name", *it);
    bool enabled = true;
    std::map<base::string16, bool>::iterator gstate_it =
        plugin_group_state_.find(*it);
    if (gstate_it != plugin_group_state_.end())
      enabled = gstate_it->second;
    summary->SetBoolean("enabled", enabled);
    plugins_list->Append(summary);
  }
}

void PluginPrefs::NotifyPluginStatusChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}
