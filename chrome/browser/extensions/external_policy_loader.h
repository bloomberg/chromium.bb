// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_LOADER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/extensions/external_loader.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class DictionaryValue;
}

namespace extensions {

// A specialization of the ExternalProvider that uses
// prefs::kExtensionInstallForceList to look up which external extensions are
// registered.
class ExternalPolicyLoader
    : public ExternalLoader,
      public content::NotificationObserver {
 public:
  explicit ExternalPolicyLoader(Profile* profile);

  // content::NotificationObserver implementation
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Adds an extension to be updated to the pref dictionary.
  static void AddExtension(base::DictionaryValue* dict,
                           const std::string& extension_id,
                           const std::string& update_url);

 protected:
  virtual void StartLoading() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  virtual ~ExternalPolicyLoader() {}

  PrefChangeRegistrar pref_change_registrar_;
  content::NotificationRegistrar notification_registrar_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPolicyLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_LOADER_H_
