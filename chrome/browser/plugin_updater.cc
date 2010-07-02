// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_updater.h"

#include <string>
#include <vector>

#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugininfo.h"

/*private*/
PluginGroup::PluginGroup(const string16& group_name,
                         const string16& name_matcher,
                         const std::string& version_range_low,
                         const std::string& version_range_high,
                         const std::string& min_version,
                         const std::string& update_url) {
  group_name_ = group_name;
  name_matcher_ = name_matcher;
  version_range_low_str_ = version_range_low;
  if (!version_range_low.empty()) {
    version_range_low_.reset(
        Version::GetVersionFromString(version_range_low));
  }
  version_range_high_str_ = version_range_high;
  if (!version_range_high.empty()) {
    version_range_high_.reset(
        Version::GetVersionFromString(version_range_high));
  }
  min_version_str_ = min_version;
  if (!min_version.empty()) {
    min_version_.reset(Version::GetVersionFromString(min_version));
  }
  update_url_ = update_url;
  enabled_ = false;
  max_version_.reset(Version::GetVersionFromString("0"));
}

PluginGroup* PluginGroup::FromPluginGroupDefinition(
    const PluginGroupDefinition& definition) {
  return new PluginGroup(ASCIIToUTF16(definition.name),
                         ASCIIToUTF16(definition.name_matcher),
                         definition.version_matcher_low,
                         definition.version_matcher_high,
                         definition.min_version,
                         definition.update_url);
}

PluginGroup* PluginGroup::FromWebPluginInfo(const WebPluginInfo& wpi) {
  // Create a matcher from the name of this plugin.
  return new PluginGroup(wpi.name, wpi.name,
                         "", "", "", "");
}

PluginGroup* PluginGroup::Copy() {
  return new PluginGroup(group_name_, name_matcher_, version_range_low_str_,
                         version_range_high_str_, min_version_str_,
                         update_url_);
}

const string16 PluginGroup::GetGroupName() const {
  return group_name_;
}

bool PluginGroup::Match(const WebPluginInfo& plugin) const {
  if (name_matcher_.empty()) {
    return false;
  }

  // Look for the name matcher anywhere in the plugin name.
  if (plugin.name.find(name_matcher_) == string16::npos) {
    return false;
  }

  if (version_range_low_.get() == NULL ||
      version_range_high_.get() == NULL) {
    return true;
  }

  // There's a version range, we must be in it.
  scoped_ptr<Version> plugin_version(
      Version::GetVersionFromString(UTF16ToWide(plugin.version)));
  if (plugin_version.get() == NULL) {
    // No version could be extracted, assume we don't match the range.
    return false;
  }

  // We match if we are in the range: [low, high)
  return (version_range_low_->CompareTo(*plugin_version) <= 0 &&
          version_range_high_->CompareTo(*plugin_version) > 0);
}

void PluginGroup::AddPlugin(const WebPluginInfo& plugin, int position) {
  web_plugin_infos_.push_back(plugin);
  // The position of this plugin relative to the global list of plugins.
  web_plugin_positions_.push_back(position);
  description_ = plugin.desc;

  // A group is enabled if any of the files are enabled.
  if (plugin.enabled) {
    enabled_ = true;
  }

  // update max_version_. Remove spaces and ')' from the version string,
  // Replace any instances of 'r', ',' or '(' with a dot.
  std::wstring version = UTF16ToWide(plugin.version);
  RemoveChars(version, L") ", &version);
  std::replace(version.begin(), version.end(), 'r', '.');
  std::replace(version.begin(), version.end(), ',', '.');
  std::replace(version.begin(), version.end(), '(', '.');

  scoped_ptr<Version> plugin_version(
      Version::GetVersionFromString(version));
  if (plugin_version.get() != NULL) {
    if (plugin_version->CompareTo(*(max_version_)) > 0) {
      max_version_.reset(plugin_version.release());
    }
  }
}

DictionaryValue* PluginGroup::GetSummary() const {
  DictionaryValue* result = new DictionaryValue();
  result->SetStringFromUTF16(L"name", group_name_);
  result->SetBoolean(L"enabled", enabled_);
  return result;
}

DictionaryValue* PluginGroup::GetData() const {
  DictionaryValue* result = new DictionaryValue();
  result->SetStringFromUTF16(L"name", group_name_);
  result->SetStringFromUTF16(L"description", description_);
  result->SetString(L"version", max_version_->GetString());
  result->SetString(L"update_url", update_url_);
  result->SetBoolean(L"critical", IsVulnerable());
  result->SetBoolean(L"enabled", enabled_);

  ListValue* plugin_files = new ListValue();
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    const WebPluginInfo& web_plugin = web_plugin_infos_[i];
    int priority = web_plugin_positions_[i];
    DictionaryValue* plugin_file = new DictionaryValue();
    plugin_file->SetStringFromUTF16(L"name", web_plugin.name);
    plugin_file->SetStringFromUTF16(L"description", web_plugin.desc);
    plugin_file->SetString(L"path", web_plugin.path.value());
    plugin_file->SetStringFromUTF16(L"version", web_plugin.version);
    plugin_file->SetBoolean(L"enabled", web_plugin.enabled);
    plugin_file->SetInteger(L"priority", priority);

    ListValue* mime_types = new ListValue();
    for (std::vector<WebPluginMimeType>::const_iterator type_it =
         web_plugin.mime_types.begin();
         type_it != web_plugin.mime_types.end();
         ++type_it) {
      DictionaryValue* mime_type = new DictionaryValue();
      mime_type->SetString(L"mimeType", type_it->mime_type);
      mime_type->SetStringFromUTF16(L"description", type_it->description);

      ListValue* file_extensions = new ListValue();
      for (std::vector<std::string>::const_iterator ext_it =
           type_it->file_extensions.begin();
           ext_it != type_it->file_extensions.end();
           ++ext_it) {
        file_extensions->Append(new StringValue(*ext_it));
      }
      mime_type->Set(L"fileExtensions", file_extensions);

      mime_types->Append(mime_type);
    }
    plugin_file->Set(L"mimeTypes", mime_types);

    plugin_files->Append(plugin_file);
  }
  result->Set(L"plugin_files", plugin_files);

  return result;
}

// Returns true if the latest version of this plugin group is vulnerable.
bool PluginGroup::IsVulnerable() const {
  if (min_version_.get() == NULL || max_version_->GetString() == "0") {
    return false;
  }
  return max_version_->CompareTo(*min_version_) < 0;
}

void PluginGroup::Enable(bool enable) {
  for (std::vector<WebPluginInfo>::const_iterator it =
       web_plugin_infos_.begin();
       it != web_plugin_infos_.end(); ++it) {
    if (enable) {
      NPAPI::PluginList::Singleton()->EnablePlugin(
          FilePath(it->path));
    } else {
      NPAPI::PluginList::Singleton()->DisablePlugin(
          FilePath(it->path));
    }
  }
}

#if defined(OS_MACOSX)
// Plugin Groups for Mac.
// Plugins are listed here as soon as vulnerabilities and solutions
// (new versions) are published.
// TODO(panayiotis): Track Java as soon as it's supported on Chrome Mac.
// TODO(panayiotis): Get the Real Player version on Mac, somehow.
static const PluginGroupDefinition kGroupDefinitions[] = {
  { "Quicktime", "QuickTime Plug-in", "", "", "7.6.6",
    "http://www.apple.com/quicktime/download/" },
  { "Flash", "Shockwave Flash", "", "", "10.0.45",
    "http://get.adobe.com/flashplayer/" },
  { "Silverlight 3", "Silverlight", "0", "4", "3.0.50106.0",
    "http://go.microsoft.com/fwlink/?LinkID=185927" },
  { "Silverlight 4", "Silverlight", "4", "5", "",
    "http://go.microsoft.com/fwlink/?LinkID=185927" },
  { "Flip4Mac", "Flip4Mac", "",  "", "2.2.1",
    "http://www.telestream.net/flip4mac-wmv/overview.htm" },
  { "Shockwave", "Shockwave for Director", "",  "", "11.5.7.609",
    "http://www.adobe.com/shockwave/download/" }
};

#elif defined(OS_WIN)
// TODO(panayiotis): We should group "RealJukebox NS Plugin" with the rest of
// the RealPlayer files.
static const PluginGroupDefinition kGroupDefinitions[] = {
  { "Quicktime", "QuickTime Plug-in", "", "", "7.6.6",
    "http://www.apple.com/quicktime/download/" },
  { "Java 6", "Java", "", "6", "6.0.200",
    "http://www.java.com/" },
  { "Adobe Reader 9", "Adobe Acrobat", "9", "10", "9.3.2",
    "http://get.adobe.com/reader/" },
  { "Adobe Reader 8", "Adobe Acrobat", "0", "9",  "8.2.2",
    "http://get.adobe.com/reader/" },
  { "Flash", "Shockwave Flash", "", "", "10.0.45",
    "http://get.adobe.com/flashplayer/" },
  { "Silverlight 3", "Silverlight", "0", "4", "3.0.50106.0",
    "http://go.microsoft.com/fwlink/?LinkID=185927" },
  { "Silverlight 4", "Silverlight", "4", "5", "",
    "http://go.microsoft.com/fwlink/?LinkID=185927" },
  { "Shockwave", "Shockwave for Director", "", "", "11.5.7.609",
    "http://www.adobe.com/shockwave/download/" },
  { "DivX Player", "DivX Web Player", "",  "", "1.4.3.4",
    "http://download.divx.com/divx/autoupdate/player/DivXWebPlayerInstaller.exe" },
  // These are here for grouping, no vulnerabilies known.
  { "Windows Media Player",  "Windows Media Player",  "",  "",   "", "" },
  { "Microsoft Office",  "Microsoft Office",  "",  "",   "", "" },
  // TODO(panayiotis): The vulnerable versions are
  //  (v >=  6.0.12.1040 && v <= 6.0.12.1663)
  //  || v == 6.0.12.1698  || v == 6.0.12.1741
  { "RealPlayer", "RealPlayer", "",  "",  "",
    "http://www.adobe.com/shockwave/download/" },
};

#else
static const PluginGroupDefinition kGroupDefinitions[] = {};
#endif

/*static*/
const PluginGroupDefinition* PluginUpdater::GetPluginGroupDefinitions() {
  return kGroupDefinitions;
}

/*static*/
const size_t PluginUpdater::GetPluginGroupDefinitionsSize() {
  // TODO(viettrungluu): |arraysize()| doesn't work with zero-size arrays.
  return ARRAYSIZE_UNSAFE(kGroupDefinitions);
}

// static
PluginUpdater* PluginUpdater::GetInstance() {
  return Singleton<PluginUpdater>::get();
}

PluginUpdater::PluginUpdater() {
  const PluginGroupDefinition* definitions = GetPluginGroupDefinitions();
  for (size_t i = 0; i < GetPluginGroupDefinitionsSize(); ++i) {
    PluginGroup* definition_group = PluginGroup::FromPluginGroupDefinition(
        definitions[i]);
    plugin_group_definitions_.push_back(linked_ptr<PluginGroup>(
        definition_group));
  }
}

PluginUpdater::~PluginUpdater() {
}

// Convert to a List of Groups
void PluginUpdater::GetPluginGroups(
    std::vector<linked_ptr<PluginGroup> >* plugin_groups) {
  // Read all plugins and convert them to plugin groups
  std::vector<WebPluginInfo> web_plugins;
  NPAPI::PluginList::Singleton()->GetPlugins(false, &web_plugins);

  // We first search for an existing group that matches our name,
  // and only create a new group if we can't find any.
  for (size_t i = 0; i < web_plugins.size(); ++i) {
    const WebPluginInfo& web_plugin = web_plugins[i];
    bool found = false;
    for (std::vector<linked_ptr<PluginGroup> >::iterator existing_it =
         plugin_groups->begin();
         existing_it != plugin_groups->end();
         ++existing_it) {
      if ((*existing_it)->Match(web_plugin)) {
        (*existing_it)->AddPlugin(web_plugin, i);
        found = true;
        break;
      }
    }

    if (!found) {
      // See if this plugin matches any of the hardcoded groups.
      for (std::vector<linked_ptr<PluginGroup> >::iterator defs_it =
           plugin_group_definitions_.begin();
           defs_it != plugin_group_definitions_.end();
           ++defs_it) {
        if ((*defs_it)->Match(web_plugin)) {
          // Make a copy, otherwise we'd be modifying plugin_group_defs_ every
          // time this method is called.
          PluginGroup* copy = (*defs_it)->Copy();
          copy->AddPlugin(web_plugin, i);
          plugin_groups->push_back(linked_ptr<PluginGroup>(copy));
          found = true;
          break;
        }
      }
    }

    // Not found in our hardcoded list, create a new one.
    if (!found) {
      PluginGroup* plugin_group = PluginGroup::FromWebPluginInfo(web_plugin);
      plugin_group->AddPlugin(web_plugin, i);
      plugin_groups->push_back(linked_ptr<PluginGroup>(plugin_group));
    }
  }
}

ListValue* PluginUpdater::GetPluginGroupsData() {
  std::vector<linked_ptr<PluginGroup> > plugin_groups;
  GetPluginGroups(&plugin_groups);

  // Construct DictionaryValues to return to the UI
  ListValue* plugin_groups_data = new ListValue();
  for (std::vector<linked_ptr<PluginGroup> >::iterator it =
       plugin_groups.begin();
       it != plugin_groups.end();
       ++it) {
    plugin_groups_data->Append((*it)->GetData());
  }
  return plugin_groups_data;
}

ListValue* PluginUpdater::GetPluginGroupsSummary() {
  std::vector<linked_ptr<PluginGroup> > plugin_groups;
  GetPluginGroups(&plugin_groups);

  // Construct DictionaryValues to return to the UI
  ListValue* plugin_groups_data = new ListValue();
  for (std::vector<linked_ptr<PluginGroup> >::iterator it =
       plugin_groups.begin();
       it != plugin_groups.end();
       ++it) {
    plugin_groups_data->Append((*it)->GetSummary());
  }
  return plugin_groups_data;
}

void PluginUpdater::EnablePluginGroup(bool enable,
                                      const string16& group_name) {
  std::vector<linked_ptr<PluginGroup> > plugin_groups;
  GetPluginGroups(&plugin_groups);

  for (std::vector<linked_ptr<PluginGroup> >::iterator it =
       plugin_groups.begin();
       it != plugin_groups.end();
       ++it) {
    if ((*it)->GetGroupName() == group_name) {
      (*it)->Enable(enable);
    }
  }
}

void PluginUpdater::EnablePluginFile(bool enable,
                                     const FilePath::StringType& file_path) {
  if (enable)
    NPAPI::PluginList::Singleton()->EnablePlugin(FilePath(file_path));
  else
    NPAPI::PluginList::Singleton()->DisablePlugin(FilePath(file_path));
}

// static
#if defined(OS_CHROMEOS)
bool PluginUpdater::enable_internal_pdf_ = true;
#else
bool PluginUpdater::enable_internal_pdf_ = false;
#endif

void PluginUpdater::DisablePluginGroupsFromPrefs(Profile* profile) {
  bool update_internal_dir = false;
  FilePath last_internal_dir =
  profile->GetPrefs()->GetFilePath(prefs::kPluginsLastInternalDirectory);
  FilePath cur_internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &cur_internal_dir) &&
      cur_internal_dir != last_internal_dir) {
    update_internal_dir = true;
    profile->GetPrefs()->SetFilePath(
        prefs::kPluginsLastInternalDirectory, cur_internal_dir);
  }

  bool found_internal_pdf = false;
  bool force_enable_internal_pdf = false;
  FilePath pdf_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path);
  FilePath::StringType pdf_path_str = pdf_path.value();
  if (enable_internal_pdf_ &&
      !profile->GetPrefs()->GetBoolean(prefs::kPluginsEnabledInternalPDF)) {
    // We switched to the internal pdf plugin being on by default, and so we
    // need to force it to be enabled.  We only want to do it this once though,
    // i.e. we don't want to enable it again if the user disables it afterwards.
    profile->GetPrefs()->SetBoolean(prefs::kPluginsEnabledInternalPDF, true);
    force_enable_internal_pdf = true;
  }

  if (ListValue* saved_plugins_list =
          profile->GetPrefs()->GetMutableList(prefs::kPluginsPluginsList)) {
    for (ListValue::const_iterator it = saved_plugins_list->begin();
         it != saved_plugins_list->end();
         ++it) {
      if (!(*it)->IsType(Value::TYPE_DICTIONARY)) {
        LOG(WARNING) << "Invalid entry in " << prefs::kPluginsPluginsList;
        continue;  // Oops, don't know what to do with this item.
      }

      DictionaryValue* plugin = static_cast<DictionaryValue*>(*it);
      string16 group_name;
      bool enabled = true;
      plugin->GetBoolean(L"enabled", &enabled);

      FilePath::StringType path;
      // The plugin list constains all the plugin files in addition to the
      // plugin groups.
      if (plugin->GetString(L"path", &path)) {
        // Files have a path attribute, groups don't.
        FilePath plugin_path(path);
        if (update_internal_dir &&
            FilePath::CompareIgnoreCase(plugin_path.DirName().value(),
                last_internal_dir.value()) == 0) {
          // If the internal plugin directory has changed and if the plugin
          // looks internal, update its path in the prefs.
          plugin_path = cur_internal_dir.Append(plugin_path.BaseName());
          path = plugin_path.value();
          plugin->SetString(L"path", path);
        }

        if (FilePath::CompareIgnoreCase(path, pdf_path_str) == 0) {
          found_internal_pdf = true;
          if (!enabled && force_enable_internal_pdf) {
            enabled = true;
            plugin->SetBoolean(L"enabled", true);
          }
        }
        if (!enabled)
          NPAPI::PluginList::Singleton()->DisablePlugin(plugin_path);
      } else if (!enabled && plugin->GetStringAsUTF16(L"name", &group_name)) {
        // Otherwise this is a list of groups.
        EnablePluginGroup(false, group_name);
      }
    }
  }

  if (!enable_internal_pdf_ && !found_internal_pdf) {
    // The internal PDF plugin is disabled by default, and the user hasn't
    // overridden the default.
    NPAPI::PluginList::Singleton()->DisablePlugin(pdf_path);
  }
}

DictionaryValue* PluginUpdater::CreatePluginFileSummary(
    const WebPluginInfo& plugin) {
  DictionaryValue* data = new DictionaryValue();
  data->SetString(L"path", plugin.path.value());
  data->SetStringFromUTF16(L"name", plugin.name);
  data->SetStringFromUTF16(L"version", plugin.version);
  data->SetBoolean(L"enabled", plugin.enabled);
  return data;
}

void PluginUpdater::UpdatePreferences(Profile* profile) {
  ListValue* plugins_list = profile->GetPrefs()->GetMutableList(
      prefs::kPluginsPluginsList);
  plugins_list->Clear();

  FilePath internal_dir;
  if (PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir))
    profile->GetPrefs()->SetFilePath(prefs::kPluginsLastInternalDirectory,
                                     internal_dir);

  // Add the plugin files.
  std::vector<WebPluginInfo> plugins;
  NPAPI::PluginList::Singleton()->GetPlugins(false, &plugins);
  for (std::vector<WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end();
       ++it) {
    plugins_list->Append(CreatePluginFileSummary(*it));
  }

  // Add the groups as well.
  ListValue* plugin_groups = GetPluginGroupsSummary();
  for (ListValue::const_iterator it = plugin_groups->begin();
       it != plugin_groups->end();
       ++it) {
    plugins_list->Append(*it);
  }
}
