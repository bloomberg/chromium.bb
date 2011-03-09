// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_DATA_REMOVER_HELPER_H_
#define CHROME_BROWSER_PLUGIN_DATA_REMOVER_HELPER_H_
#pragma once

#include "base/ref_counted.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Profile;

// Helper class modeled after BooleanPrefMember to (asynchronously) update
// the preference specifying whether clearing plug-in data is supported
// by an installed plug-in.
// It should only be used from the UI thread. The client has to make sure that
// the passed PrefService outlives this object.
class PluginDataRemoverHelper : public NotificationObserver {
 public:
  PluginDataRemoverHelper();
  ~PluginDataRemoverHelper();

  // Binds this object to the |pref_name| preference in |prefs|, notifying
  // |observer| if the value changes.
  // This fires off a request to the NPAPI::PluginList (via PluginDataRemover)
  // on the FILE thread to get the list of installed plug-ins.
  void Init(const char* pref_name,
            PrefService* prefs,
            NotificationObserver* observer);

  bool GetValue() const { return pref_.GetValue(); }

  // NotificationObserver methods:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  class Internal;

  BooleanPrefMember pref_;
  NotificationRegistrar registrar_;
  scoped_refptr<Internal> internal_;

  DISALLOW_COPY_AND_ASSIGN(PluginDataRemoverHelper);
};

#endif  // CHROME_BROWSER_PLUGIN_DATA_REMOVER_HELPER_H_
