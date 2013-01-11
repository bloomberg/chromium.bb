// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/extension_settings_helper.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/storage/settings_frontend.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/browser/value_store/value_store.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using sync_datatype_helper::test;

namespace extension_settings_helper {

namespace {

std::string ToJson(const Value& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(&value,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);
  return json;
}

void GetAllSettingsOnFileThread(
    scoped_ptr<DictionaryValue>* out,
    base::WaitableEvent* signal,
    ValueStore* storage) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  out->swap(storage->Get()->settings());
  signal->Signal();
}

scoped_ptr<DictionaryValue> GetAllSettings(
    Profile* profile, const std::string& id) {
  base::WaitableEvent signal(false, false);
  scoped_ptr<DictionaryValue> settings;
  profile->GetExtensionService()->settings_frontend()->RunWithStorage(
      id,
      extensions::settings_namespace::SYNC,
      base::Bind(&GetAllSettingsOnFileThread, &settings, &signal));
  signal.Wait();
  return settings.Pass();
}

bool AreSettingsSame(Profile* expected_profile, Profile* actual_profile) {
  const ExtensionSet* extensions =
      expected_profile->GetExtensionService()->extensions();
  if (extensions->size() !=
      actual_profile->GetExtensionService()->extensions()->size()) {
    ADD_FAILURE();
    return false;
  }

  bool same = true;
  for (ExtensionSet::const_iterator it = extensions->begin();
      it != extensions->end(); ++it) {
    const std::string& id = (*it)->id();
    scoped_ptr<DictionaryValue> expected(GetAllSettings(expected_profile, id));
    scoped_ptr<DictionaryValue> actual(GetAllSettings(actual_profile, id));
    if (!expected->Equals(actual.get())) {
      ADD_FAILURE() <<
          "Expected " << ToJson(*expected) << " got " << ToJson(*actual);
      same = false;
    }
  }
  return same;
}

void SetSettingsOnFileThread(
    const DictionaryValue* settings,
    base::WaitableEvent* signal,
    ValueStore* storage) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  storage->Set(ValueStore::DEFAULTS, *settings);
  signal->Signal();
}

}  // namespace

void SetExtensionSettings(
    Profile* profile, const std::string& id, const DictionaryValue& settings) {
  base::WaitableEvent signal(false, false);
  profile->GetExtensionService()->settings_frontend()->RunWithStorage(
      id,
      extensions::settings_namespace::SYNC,
      base::Bind(&SetSettingsOnFileThread, &settings, &signal));
  signal.Wait();
}

void SetExtensionSettingsForAllProfiles(
    const std::string& id, const DictionaryValue& settings) {
  for (int i = 0; i < test()->num_clients(); ++i)
    SetExtensionSettings(test()->GetProfile(i), id, settings);
  SetExtensionSettings(test()->verifier(), id, settings);
}

bool AllExtensionSettingsSameAsVerifier() {
  bool all_profiles_same = true;
  for (int i = 0; i < test()->num_clients(); ++i) {
    // &= so that all profiles are tested; analogous to EXPECT over ASSERT.
    all_profiles_same &=
        AreSettingsSame(test()->verifier(), test()->GetProfile(i));
  }
  return all_profiles_same;
}

}  // namespace extension_settings_helper
