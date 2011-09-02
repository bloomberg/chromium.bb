// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_prefs.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
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

}

// How long to wait to save the plugin enabled information, which might need to
// go to disk.
#define kPluginUpdateDelayMs (60 * 1000)

class PluginPrefs::Factory : public ProfileKeyedServiceFactory {
 public:
  static Factory* GetInstance();

  PluginPrefsWrapper* GetWrapperForProfile(Profile* profile);

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
  PluginPrefs* plugin_prefs =
      Factory::GetInstance()->GetWrapperForProfile(profile)->plugin_prefs();
  DCHECK(plugin_prefs);
  return plugin_prefs;
}

DictionaryValue* PluginPrefs::CreatePluginFileSummary(
    const webkit::WebPluginInfo& plugin) {
  DictionaryValue* data = new DictionaryValue();
  data->SetString("path", plugin.path.value());
  data->SetString("name", plugin.name);
  data->SetString("version", plugin.version);
  data->SetBoolean("enabled", IsPluginEnabled(plugin));
  return data;
}

void PluginPrefs::EnablePluginGroup(bool enable, const string16& group_name) {
  webkit::npapi::PluginList::Singleton()->EnableGroup(enable, group_name);
  NotifyPluginStatusChanged();
}

void PluginPrefs::EnablePlugin(bool enable, const FilePath& path) {
  if (enable)
    webkit::npapi::PluginList::Singleton()->EnablePlugin(path);
  else
    webkit::npapi::PluginList::Singleton()->DisablePlugin(path);

  NotifyPluginStatusChanged();
}

bool PluginPrefs::IsPluginEnabled(const webkit::WebPluginInfo& plugin) {
  // If enabling NaCl, make sure the plugin is also enabled. See bug
  // http://code.google.com/p/chromium/issues/detail?id=81010 for more
  // information.
  // TODO(dspringer): When NaCl is on by default, remove this code.
  if ((plugin.name ==
       ASCIIToUTF16(chrome::ChromeContentClient::kNaClPluginName)) &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableNaCl)) {
    return true;
  }
  return webkit::IsPluginEnabled(plugin);
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
  if (*pref_name == prefs::kPluginsDisabledPlugins ||
      *pref_name == prefs::kPluginsDisabledPluginsExceptions ||
      *pref_name == prefs::kPluginsEnabledPlugins) {
    const ListValue* disabled_list =
        prefs_->GetList(prefs::kPluginsDisabledPlugins);
    const ListValue* exceptions_list =
        prefs_->GetList(prefs::kPluginsDisabledPluginsExceptions);
    const ListValue* enabled_list =
        prefs_->GetList(prefs::kPluginsEnabledPlugins);
    UpdatePluginsStateFromPolicy(disabled_list, exceptions_list, enabled_list);
  }
}

void PluginPrefs::UpdatePluginsStateFromPolicy(
    const ListValue* disabled_list,
    const ListValue* exceptions_list,
    const ListValue* enabled_list) {
  std::set<string16> disabled_plugin_patterns;
  std::set<string16> disabled_plugin_exception_patterns;
  std::set<string16> enabled_plugin_patterns;

  ListValueToStringSet(disabled_list, &disabled_plugin_patterns);
  ListValueToStringSet(exceptions_list, &disabled_plugin_exception_patterns);
  ListValueToStringSet(enabled_list, &enabled_plugin_patterns);

  webkit::npapi::PluginGroup::SetPolicyEnforcedPluginPatterns(
      disabled_plugin_patterns,
      disabled_plugin_exception_patterns,
      enabled_plugin_patterns);

  NotifyPluginStatusChanged();
}

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

void PluginPrefs::SetProfile(Profile* profile) {
  prefs_ = profile->GetPrefs();
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

          if (!enabled)
            webkit::npapi::PluginList::Singleton()->DisablePlugin(plugin_path);
        } else if (!enabled && plugin->GetString("name", &group_name)) {
          // Don't disable this group if it's for the pdf or nacl plugins and
          // we just forced it on.
          if (force_enable_internal_pdf && pdf_group_name == group_name)
            continue;
          if (force_enable_nacl && (nacl_group_name == group_name ||
                                    old_nacl_group_name == group_name))
            continue;

          // Otherwise this is a list of groups.
          EnablePluginGroup(false, group_name);
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
  const ListValue* disabled_plugins =
      prefs_->GetList(prefs::kPluginsDisabledPlugins);
  const ListValue* disabled_exception_plugins =
      prefs_->GetList(prefs::kPluginsDisabledPluginsExceptions);
  const ListValue* enabled_plugins =
      prefs_->GetList(prefs::kPluginsEnabledPlugins);
  UpdatePluginsStateFromPolicy(disabled_plugins,
                               disabled_exception_plugins,
                               enabled_plugins);

  registrar_.RemoveAll();
  registrar_.Init(prefs_);
  registrar_.Add(prefs::kPluginsDisabledPlugins, this);
  registrar_.Add(prefs::kPluginsDisabledPluginsExceptions, this);
  registrar_.Add(prefs::kPluginsEnabledPlugins, this);

  if (force_enable_internal_pdf || internal_pdf_enabled) {
    // See http://crbug.com/50105 for background.
    EnablePluginGroup(false, ASCIIToUTF16(
        webkit::npapi::PluginGroup::kAdobeReaderGroupName));
  }

  if (force_enable_internal_pdf || force_enable_nacl) {
    // We want to save this, but doing so requires loading the list of plugins,
    // so do it after a minute as to not impact startup performance.  Note that
    // plugins are loaded after 30s by the metrics service.
    UpdatePreferences(kPluginUpdateDelayMs);
  }
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

PluginPrefs::Factory::Factory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

ProfileKeyedService* PluginPrefs::Factory::BuildServiceInstanceFor(
      Profile* profile) const {
  scoped_refptr<PluginPrefs> plugin_prefs(new PluginPrefs());
  plugin_prefs->SetProfile(profile);
  return new PluginPrefsWrapper(plugin_prefs);
}

PluginPrefs::PluginPrefs() : notify_pending_(false) {
}

PluginPrefs::~PluginPrefs() {
}

void PluginPrefs::UpdatePreferences(int delay_ms) {
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE,
      FROM_HERE,
      NewRunnableMethod(this, &PluginPrefs::GetPreferencesDataOnFileThread),
      delay_ms);
}

void PluginPrefs::GetPreferencesDataOnFileThread() {
  std::vector<webkit::WebPluginInfo> plugins;
  std::vector<webkit::npapi::PluginGroup> groups;

  webkit::npapi::PluginList* plugin_list =
      webkit::npapi::PluginList::Singleton();
  plugin_list->GetPlugins(&plugins);
  plugin_list->GetPluginGroups(false, &groups);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &PluginPrefs::OnUpdatePreferences,
                        plugins, groups));
}

void PluginPrefs::OnUpdatePreferences(
    std::vector<webkit::WebPluginInfo> plugins,
    std::vector<webkit::npapi::PluginGroup> groups) {
  if (!prefs_)
    return;

  ListPrefUpdate update(prefs_, prefs::kPluginsPluginsList);
  ListValue* plugins_list = update.Get();
  plugins_list->Clear();

  FilePath internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir))
    prefs_->SetFilePath(prefs::kPluginsLastInternalDirectory, internal_dir);

  // Add the plugin files.
  for (size_t i = 0; i < plugins.size(); ++i) {
    DictionaryValue* summary = CreatePluginFileSummary(plugins[i]);
    // If the plugin is managed by policy, store the user preferred state
    // instead.
    if (plugins[i].enabled & webkit::WebPluginInfo::MANAGED_MASK) {
      bool user_enabled =
          (plugins[i].enabled & webkit::WebPluginInfo::USER_MASK) ==
              webkit::WebPluginInfo::USER_ENABLED;
      summary->SetBoolean("enabled", user_enabled);
    }
    plugins_list->Append(summary);
  }

  // Add the groups as well.
  for (size_t i = 0; i < groups.size(); ++i) {
      DictionaryValue* summary = groups[i].GetSummary();
      // If the plugin is disabled only by policy don't store this state in the
      // user pref store.
      if (!groups[i].Enabled() &&
          webkit::npapi::PluginGroup::IsPluginNameDisabledByPolicy(
              groups[i].GetGroupName()))
        summary->SetBoolean("enabled", true);
      plugins_list->Append(summary);
  }
}

void PluginPrefs::NotifyPluginStatusChanged() {
  if (notify_pending_)
    return;
  notify_pending_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PluginPrefs::OnNotifyPluginStatusChanged));
}

void PluginPrefs::OnNotifyPluginStatusChanged() {
  notify_pending_ = false;
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
  prefs->RegisterListPref(prefs::kPluginsDisabledPlugins,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPluginsDisabledPluginsExceptions,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPluginsEnabledPlugins,
                          PrefService::UNSYNCABLE_PREF);
}
