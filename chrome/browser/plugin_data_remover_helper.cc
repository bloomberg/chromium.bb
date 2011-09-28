// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_data_remover_helper.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/plugin_data_remover.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"

// The internal class is refcounted so it can outlive PluginDataRemoverHelper.
class PluginDataRemoverHelper::Internal
    : public base::RefCountedThreadSafe<PluginDataRemoverHelper::Internal> {
 public:
  Internal(const char* pref_name, Profile* profile)
      : pref_name_(pref_name), profile_(profile) {}

  void StartUpdate() {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&PluginDataRemoverHelper::Internal::UpdateOnFileThread,
                   this,
                   make_scoped_refptr(PluginPrefs::GetForProfile(profile_))));
  }

  void Invalidate() {
    profile_ = NULL;
  }

 private:
  friend class base::RefCountedThreadSafe<Internal>;

  ~Internal() {}

  void UpdateOnFileThread(scoped_refptr<PluginPrefs> plugin_prefs) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    bool result = PluginDataRemover::IsSupported(plugin_prefs);
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&PluginDataRemoverHelper::Internal::SetPrefOnUIThread,
                   this,
                   result));
  }

  void SetPrefOnUIThread(bool value) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (profile_)
      profile_->GetPrefs()->SetBoolean(pref_name_.c_str(), value);
  }

  std::string pref_name_;
  // Weak pointer.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(Internal);
};

PluginDataRemoverHelper::PluginDataRemoverHelper() {}

PluginDataRemoverHelper::~PluginDataRemoverHelper() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (internal_)
    internal_->Invalidate();
}

void PluginDataRemoverHelper::Init(const char* pref_name,
                                   Profile* profile,
                                   NotificationObserver* observer) {
  pref_.Init(pref_name, profile->GetPrefs(), observer);
  registrar_.Add(this, chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
                 Source<PluginPrefs>(PluginPrefs::GetForProfile(profile)));
  internal_ = make_scoped_refptr(new Internal(pref_name, profile));
  internal_->StartUpdate();
}

void PluginDataRemoverHelper::Observe(int type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED) {
    internal_->StartUpdate();
  } else {
    NOTREACHED();
  }
}
