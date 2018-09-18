// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_feature_list_creator.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/pref_names.h"

ChromeFeatureListCreator::ChromeFeatureListCreator() = default;
ChromeFeatureListCreator::~ChromeFeatureListCreator() = default;

void ChromeFeatureListCreator::CreatePrefService() {
  base::FilePath local_state_file;
  bool result =
      base::PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_file);
  DCHECK(result);

  pref_store_ = base::MakeRefCounted<JsonPrefStore>(local_state_file);
  pref_store_->ReadPrefs();

  PrefServiceFactory factory;
  factory.set_user_prefs(pref_store_);
  factory.set_command_line_prefs(
      base::MakeRefCounted<ChromeCommandLinePrefStore>(
          base::CommandLine::ForCurrentProcess()));
  factory.set_read_error_callback(base::BindRepeating(
      &chrome_prefs::HandlePersistentPrefStoreReadError, local_state_file));
  scoped_refptr<PrefRegistry> registry = new PrefRegistrySimple();
  simple_local_state_ = factory.Create(registry);
}

scoped_refptr<PersistentPrefStore> ChromeFeatureListCreator::GetPrefStore() {
  return pref_store_;
}

std::unique_ptr<PrefService> ChromeFeatureListCreator::TakePrefService() {
  return std::move(simple_local_state_);
}

void ChromeFeatureListCreator::CreateFeatureList() {
  CreatePrefService();
  // TODO(hanxi): Add implementation to create feature list.
}

void ChromeFeatureListCreator::CreatePrefServiceForTesting() {
  CreatePrefService();
}
