// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_prefs.h"

#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_filter.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"

using user_prefs::PrefRegistrySyncable;

namespace extensions {

// static
scoped_ptr<PrefService> ShellPrefs::CreatePrefService(
    content::BrowserContext* browser_context) {
  base::PrefServiceFactory factory;

  base::FilePath filename =
      browser_context->GetPath().AppendASCII("user_prefs.json");
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      JsonPrefStore::GetTaskRunnerForFile(
          filename, content::BrowserThread::GetBlockingPool());
  scoped_refptr<JsonPrefStore> user_prefs =
      new JsonPrefStore(filename, task_runner, scoped_ptr<PrefFilter>());
  user_prefs->ReadPrefs();  // Synchronous.
  factory.set_user_prefs(user_prefs);

  // TODO(jamescook): If we want to support prefs that are set by extensions
  // via ChromeSettings properties (e.g. chrome.accessibilityFeatures or
  // chrome.proxy) then this should create an ExtensionPrefStore and attach it
  // with PrefServiceFactory::set_extension_prefs().
  // See https://developer.chrome.com/extensions/types#ChromeSetting

  // Prefs should be registered before the PrefService is created.
  PrefRegistrySyncable* pref_registry = new PrefRegistrySyncable;
  ExtensionPrefs::RegisterProfilePrefs(pref_registry);

  scoped_ptr<PrefService> pref_service = factory.Create(pref_registry);
  user_prefs::UserPrefs::Set(browser_context, pref_service.get());
  return pref_service;
}

}  // namespace extensions
