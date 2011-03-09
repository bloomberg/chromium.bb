// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_data_remover_helper.h"

#include <string>

#include "base/ref_counted.h"
#include "chrome/browser/plugin_data_remover.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"

// The internal class is refcounted so it can outlive PluginDataRemoverHelper.
class PluginDataRemoverHelper::Internal
    : public base::RefCountedThreadSafe<PluginDataRemoverHelper::Internal> {
 public:
  explicit Internal(const char* pref_name, PrefService* prefs)
      : pref_name_(pref_name), prefs_(prefs) {}

  void StartUpdate() {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        NewRunnableMethod(
            this,
            &PluginDataRemoverHelper::Internal::UpdateOnFileThread));
  }

  void Invalidate() {
    prefs_ = NULL;
  }

 private:
  friend class base::RefCountedThreadSafe<Internal>;

  ~Internal() {}

  void UpdateOnFileThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    bool result = PluginDataRemover::IsSupported();
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &PluginDataRemoverHelper::Internal::SetPrefOnUIThread,
                          result));
  }

  void SetPrefOnUIThread(bool value) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (prefs_)
      prefs_->SetBoolean(pref_name_.c_str(), value);
  }

  std::string pref_name_;
  // Weak pointer.
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(Internal);
};

PluginDataRemoverHelper::PluginDataRemoverHelper() {}

PluginDataRemoverHelper::~PluginDataRemoverHelper() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (internal_)
    internal_->Invalidate();
}

void PluginDataRemoverHelper::Init(const char* pref_name,
                                   PrefService* prefs,
                                   NotificationObserver* observer) {
  pref_.Init(pref_name, prefs, observer);
  registrar_.Add(this, NotificationType::PLUGIN_ENABLE_STATUS_CHANGED,
                 NotificationService::AllSources());
  internal_ = make_scoped_refptr(new Internal(pref_name, prefs));
  internal_->StartUpdate();
}

void PluginDataRemoverHelper::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  if (type.value == NotificationType::PLUGIN_ENABLE_STATUS_CHANGED) {
    internal_->StartUpdate();
  } else {
    NOTREACHED();
  }
}
