// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_DATA_REMOVER_HELPER_H_
#define CHROME_BROWSER_PLUGIN_DATA_REMOVER_HELPER_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PluginPrefs;
class Profile;

namespace webkit {
struct WebPluginInfo;
}

// Helper class modeled after BooleanPrefMember to (asynchronously) update
// the preference specifying whether clearing plug-in data is supported
// by an installed plug-in.
// It should only be used from the UI thread. The client has to make sure that
// the passed profile outlives this object.
class PluginDataRemoverHelper : public content::NotificationObserver {
 public:
  PluginDataRemoverHelper();
  virtual ~PluginDataRemoverHelper();

  // Binds this object to the |pref_name| preference in the profile's
  // PrefService, notifying |observer| if the value changes.
  // This asynchronously calls the PluginService to get the list of installed
  // plug-ins.
  void Init(const char* pref_name,
            Profile* profile,
            content::NotificationObserver* observer);

  bool GetValue() const { return pref_.GetValue(); }

  // Like PluginDataRemover::IsSupported, but checks that the returned plugin
  // is enabled by PluginPrefs
  static bool IsSupported(PluginPrefs* plugin_prefs);

  // content::NotificationObserver methods:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  void StartUpdate();
  void GotPlugins(scoped_refptr<PluginPrefs> plugin_prefs,
                  const std::vector<webkit::WebPluginInfo>& plugins);

  BooleanPrefMember pref_;
  content::NotificationRegistrar registrar_;
  // Weak pointer.
  Profile* profile_;
  base::WeakPtrFactory<PluginDataRemoverHelper> factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginDataRemoverHelper);
};

#endif  // CHROME_BROWSER_PLUGIN_DATA_REMOVER_HELPER_H_
