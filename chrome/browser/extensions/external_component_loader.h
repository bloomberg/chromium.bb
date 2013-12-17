// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_COMPONENT_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_COMPONENT_LOADER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/profiles/profile.h"

class PrefChangeRegistrar;

namespace extensions {

// A specialization of the ExternalLoader that loads a hard-coded list of
// external extensions, that should be considered components of chrome (but
// unlike Component extensions, these extensions are installed from the webstore
// and don't get access to component only APIs.
// Instances of this class are expected to be created and destroyed on the UI
// thread and they are expecting public method calls from the UI thread.
class ExternalComponentLoader
    : public ExternalLoader,
      public base::SupportsWeakPtr<ExternalComponentLoader> {
 public:
  explicit ExternalComponentLoader(Profile* profile);

  static bool IsEnhancedBookmarksExperimentEnabled();

  // Register speech synthesis prefs for a profile.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 protected:
  virtual void StartLoading() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;
  virtual ~ExternalComponentLoader();

  // The profile that this loader is associated with. It listens for
  // preference changes for that profile.
  Profile* profile_;

#if defined(OS_CHROMEOS)
  // The pref change registrar, so we can watch for pref changes.
  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // A map from language code to the extension id of the high-quality
  // extension for that language in the web store, if any - for loading
  // speech synthesis component extensions.
  base::hash_map<std::string, std::string> lang_to_extension_id_map_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ExternalComponentLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_COMPONENT_LOADER_H_
