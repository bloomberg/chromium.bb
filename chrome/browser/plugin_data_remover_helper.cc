// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_data_remover_helper.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/plugin_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/plugin_data_remover.h"

using content::BrowserThread;

PluginDataRemoverHelper::PluginDataRemoverHelper()
    : profile_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {}

PluginDataRemoverHelper::~PluginDataRemoverHelper() {
}

void PluginDataRemoverHelper::Init(const char* pref_name,
                                   Profile* profile,
                                   content::NotificationObserver* observer) {
  pref_.Init(pref_name, profile->GetPrefs(), observer);
  profile_ = profile;
  registrar_.Add(this, chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
                 content::Source<Profile>(profile));
  StartUpdate();
}

// static
bool PluginDataRemoverHelper::IsSupported(PluginPrefs* plugin_prefs) {
  webkit::WebPluginInfo plugin;
  return content::PluginDataRemover::IsSupported(&plugin) &&
      plugin_prefs->IsPluginEnabled(plugin);
}

void PluginDataRemoverHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED) {
    StartUpdate();
  } else {
    NOTREACHED();
  }
}

void PluginDataRemoverHelper::StartUpdate() {
  PluginService::GetInstance()->GetPlugins(
      base::Bind(&PluginDataRemoverHelper::GotPlugins, factory_.GetWeakPtr(),
                 make_scoped_refptr(PluginPrefs::GetForProfile(profile_))));
}

void PluginDataRemoverHelper::GotPlugins(
    scoped_refptr<PluginPrefs> plugin_prefs,
    const std::vector<webkit::WebPluginInfo>& plugins) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool supported = IsSupported(plugin_prefs);
  // Set the value on the PrefService instead of through the PrefMember to
  // notify observers if it changed.
  profile_->GetPrefs()->SetBoolean(pref_.GetPrefName().c_str(), supported);
}
