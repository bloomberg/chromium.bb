// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"

class Profile;

namespace base {
class ListValue;
}

namespace webkit {
struct WebPluginInfo;
namespace npapi {
class PluginList;
}
}

// This class stores information about whether a plug-in or a plug-in group is
// enabled or disabled.
// Except where otherwise noted, it can be used on every thread.
class PluginPrefs : public RefcountedProfileKeyedService {
 public:
  enum PolicyStatus {
    NO_POLICY = 0,  // Neither enabled or disabled by policy.
    POLICY_ENABLED,
    POLICY_DISABLED,
  };

  // Returns the instance associated with |profile|, creating it if necessary.
  static scoped_refptr<PluginPrefs> GetForProfile(Profile* profile);

  // Usually the PluginPrefs associated with a TestingProfile is NULL.
  // This method overrides that for a given TestingProfile, returning the newly
  // created PluginPrefs object.
  static scoped_refptr<PluginPrefs> GetForTestingProfile(Profile* profile);

  // Sets the plug-in list for tests.
  void SetPluginListForTesting(webkit::npapi::PluginList* plugin_list);

  // Creates a new instance. This method should only be used for testing.
  PluginPrefs();

  // Associates this instance with |prefs|. This enables or disables
  // plugin groups as defined by the user's preferences.
  // This method should only be called on the UI thread.
  void SetPrefs(PrefService* prefs);

  // Enable or disable a plugin group.
  void EnablePluginGroup(bool enable, const string16& group_name);

  // Enables or disables a specific plug-in file, if possible.
  // If the plug-in state can't be changed (because of a policy for example)
  // then enabling/disabling the plug-in is ignored and |callback| is run
  // with 'false' passed to it. Otherwise the plug-in state is changed
  // and |callback| is run with 'true' passed to it.
  void EnablePlugin(bool enable, const base::FilePath& file_path,
                    const base::Callback<void(bool)>& callback);

  // Returns whether there is a policy enabling or disabling plug-ins of the
  // given name.
  PolicyStatus PolicyStatusForPlugin(const string16& name) const;

  // Returns whether the plugin is enabled or not.
  bool IsPluginEnabled(const webkit::WebPluginInfo& plugin) const;

  void set_profile(Profile* profile) { profile_ = profile; }

  // RefCountedProfileKeyedBase method override.
  virtual void ShutdownOnUIThread() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<PluginPrefs>;
  friend class PluginPrefsTest;

  // PluginState stores a mapping from plugin path to enable/disable state. We
  // don't simply use a std::map, because we would like to keep the state of
  // some plugins in sync with each other.
  class PluginState {
   public:
    PluginState();
    ~PluginState();

    // Returns whether |plugin| is found. If |plugin| cannot be found,
    // |*enabled| won't be touched.
    bool Get(const base::FilePath& plugin, bool* enabled) const;
    void Set(const base::FilePath& plugin, bool enabled);
    // It is similar to Set(), except that it does nothing if |plugin| needs to
    // be converted to a different key.
    void SetIgnorePseudoKey(const base::FilePath& plugin, bool enabled);

   private:
    base::FilePath ConvertMapKey(const base::FilePath& plugin) const;

    std::map<base::FilePath, bool> state_;
  };

  virtual ~PluginPrefs();

  // Called to update one of the policy_xyz patterns below when a
  // preference changes.
  void UpdatePatternsAndNotify(std::set<string16>* patterns,
                               const std::string& pref_name);

  // Allows unit tests to directly set enforced plug-in patterns.
  void SetPolicyEnforcedPluginPatterns(
      const std::set<string16>& disabled_patterns,
      const std::set<string16>& disabled_exception_patterns,
      const std::set<string16>& enabled_patterns);

  // Returns the plugin list to use, either the singleton or the override.
  webkit::npapi::PluginList* GetPluginList() const;

  // Callback for after the plugin groups have been loaded.
  void EnablePluginGroupInternal(
      bool enabled,
      const string16& group_name,
      const std::vector<webkit::WebPluginInfo>& plugins);
  void EnablePluginInternal(
      bool enabled,
      const base::FilePath& path,
      PluginFinder* plugin_finder,
      const base::Callback<void(bool)>& callback,
      const std::vector<webkit::WebPluginInfo>& plugins);

  // Called on the file thread to get the data necessary to update the saved
  // preferences.
  void GetPreferencesDataOnFileThread();

  // Called on the UI thread with the plugin data to save the preferences.
  void OnUpdatePreferences(const std::vector<webkit::WebPluginInfo>& plugins);

  // Sends the notification that plugin data has changed.
  void NotifyPluginStatusChanged();

  static void ListValueToStringSet(const base::ListValue* src,
                                   std::set<string16>* dest);

  // Checks if |name| matches any of the patterns in |pattern_set|.
  static bool IsStringMatchedInSet(const string16& name,
                                   const std::set<string16>& pattern_set);

  // Guards access to the following data structures.
  mutable base::Lock lock_;

  PluginState plugin_state_;
  std::map<string16, bool> plugin_group_state_;

  std::set<string16> policy_disabled_plugin_patterns_;
  std::set<string16> policy_disabled_plugin_exception_patterns_;
  std::set<string16> policy_enabled_plugin_patterns_;

  // Weak pointer, owns us. Only used as a notification source.
  Profile* profile_;

  // Weak pointer, owned by the profile.
  PrefService* prefs_;

  // PluginList to use for testing. If this is NULL, defaults to the global
  // singleton.
  webkit::npapi::PluginList* plugin_list_;

  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PluginPrefs);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_H_
