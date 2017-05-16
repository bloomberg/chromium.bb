// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/validation_rules_storage_factory.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/web/public/web_thread.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

namespace autofill {

using ::i18n::addressinput::Storage;

// static
std::unique_ptr<Storage> ValidationRulesStorageFactory::CreateStorage() {
  static base::LazyInstance<ValidationRulesStorageFactory>::DestructorAtExit
      instance = LAZY_INSTANCE_INITIALIZER;
  return std::unique_ptr<Storage>(
      new ChromeStorageImpl(instance.Get().json_pref_store_.get()));
}

ValidationRulesStorageFactory::ValidationRulesStorageFactory() {
  base::FilePath user_data_dir;
  bool success = PathService::Get(ios::DIR_USER_DATA, &user_data_dir);
  DCHECK(success);

  base::FilePath cache =
      user_data_dir.Append(FILE_PATH_LITERAL("Address Validation Rules"));

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      JsonPrefStore::GetTaskRunnerForFile(cache,
                                          web::WebThread::GetBlockingPool());

  json_pref_store_ = new JsonPrefStore(cache, task_runner.get(),
                                       std::unique_ptr<PrefFilter>());
  json_pref_store_->ReadPrefsAsync(NULL);
}

ValidationRulesStorageFactory::~ValidationRulesStorageFactory() {}

}  // namespace autofill
