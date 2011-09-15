// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_PREFS_H_
#define CHROME_BROWSER_PLUGIN_PREFS_H_
#pragma once

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/common/notification_observer.h"

class NotificationDetails;
class NotificationSource;
class Profile;

namespace content {
class ResourceContext;
}

namespace base {
class DictionaryValue;
class ListValue;
}

namespace webkit {
struct WebPluginInfo;
namespace npapi {
class PluginGroup;
}
}

// This class stores information about whether a plug-in or a plug-in group is
// enabled or disabled.
// Except for the |IsPluginEnabled| method, it should only be used on the UI
// thread.
class PluginPrefs : public base::RefCountedThreadSafe<PluginPrefs>,
                    public NotificationObserver {
 public:
  // Initializes the factory for this class for dependency tracking.
  // This should be called before the first profile is created.
  static void Initialize();

  // Returns the instance associated with |profile|, creating it if necessary.
  static PluginPrefs* GetForProfile(Profile* profile);

  // Creates a new instance. This method should only be used for testing.
  PluginPrefs();

  // Associates this instance with |prefs|. This enables or disables
  // plugin groups as defined by the user's preferences.
  void SetPrefs(PrefService* prefs);

  // Enable or disable a plugin group.
  void EnablePluginGroup(bool enable, const string16& group_name);

  // Enable or disable a specific plugin file.
  void EnablePlugin(bool enable, const FilePath& file_path);

  // Returns whether the plugin is enabled or not.
  bool IsPluginEnabled(const webkit::WebPluginInfo& plugin);

  // Write the enable/disable status to the user's preference file.
  void UpdatePreferences(int delay_ms);

  // NotificationObserver method overrides
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  static void RegisterPrefs(PrefService* prefs);

  void ShutdownOnUIThread();

 private:
  friend class base::RefCountedThreadSafe<PluginPrefs>;

  class Factory;

  virtual ~PluginPrefs();

  // Called on the file thread to get the data necessary to update the saved
  // preferences.
  void GetPreferencesDataOnFileThread();

  // Called on the UI thread with the plugin data to save the preferences.
  void OnUpdatePreferences(std::vector<webkit::WebPluginInfo> plugins,
                           std::vector<webkit::npapi::PluginGroup> groups);

  // Queues sending the notification that plugin data has changed.  This is done
  // so that if a bunch of changes happen, we only send one notification.
  void NotifyPluginStatusChanged();

  // Used for the post task to notify that plugin enabled status changed.
  void OnNotifyPluginStatusChanged();

  base::DictionaryValue* CreatePluginFileSummary(
      const webkit::WebPluginInfo& plugin);

  // Force plugins to be enabled or disabled due to policy.
  // |disabled_list| contains the list of StringValues of the names of the
  // policy-disabled plugins, |exceptions_list| the policy-allowed plugins,
  // and |enabled_list| the policy-enabled plugins.
  void UpdatePluginsStateFromPolicy(const base::ListValue* disabled_list,
                                    const base::ListValue* exceptions_list,
                                    const base::ListValue* enabled_list);

  void ListValueToStringSet(const base::ListValue* src,
                            std::set<string16>* dest);

  // Weak pointer, owned by the profile.
  PrefService* prefs_;

  PrefChangeRegistrar registrar_;

  bool notify_pending_;

  DISALLOW_COPY_AND_ASSIGN(PluginPrefs);
};

#endif  // CHROME_BROWSER_PLUGIN_PREFS_H_
