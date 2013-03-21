// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_STATUS_PREF_SETTER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_STATUS_PREF_SETTER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PluginPrefs;
class PrefService;
class Profile;

namespace webkit {
struct WebPluginInfo;
}

// Helper class modeled after BooleanPrefMember to (asynchronously) update
// preferences related to plugin enable status.
// It should only be used from the UI thread. The client has to make sure that
// the passed profile outlives this object.
class PluginStatusPrefSetter : public content::NotificationObserver {
 public:
  PluginStatusPrefSetter();
  virtual ~PluginStatusPrefSetter();

  // Binds the preferences in the profile's PrefService, notifying |observer| if
  // any value changes.
  // This asynchronously calls the PluginService to get the list of installed
  // plug-ins.
  void Init(Profile* profile,
            const BooleanPrefMember::NamedChangeCallback& observer);

  bool IsClearPluginLSODataEnabled() const {
    return clear_plugin_lso_data_enabled_.GetValue();
  }

  bool IsPepperFlashSettingsEnabled() const {
    return pepper_flash_settings_enabled_.GetValue();
  }

  // content::NotificationObserver methods:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void StartUpdate();
  void GotPlugins(scoped_refptr<PluginPrefs> plugin_prefs,
                  const std::vector<webkit::WebPluginInfo>& plugins);

  content::NotificationRegistrar registrar_;
  // Weak pointer.
  Profile* profile_;
  base::WeakPtrFactory<PluginStatusPrefSetter> factory_;

  // Whether clearing LSO data is supported.
  BooleanPrefMember clear_plugin_lso_data_enabled_;
  // Whether we should show Pepper Flash-specific settings.
  BooleanPrefMember pepper_flash_settings_enabled_;

  DISALLOW_COPY_AND_ASSIGN(PluginStatusPrefSetter);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_STATUS_PREF_SETTER_H_
