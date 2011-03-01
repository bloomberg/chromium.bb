// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_extension_loader.h"

#include "app/app_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "content/browser/browser_thread.h"
#include "chrome/common/json_value_serializer.h"

namespace {

// Caller takes ownership of the returned dictionary.
DictionaryValue* ExtractPrefs(const FilePath& path,
                              ValueSerializer* serializer) {
  std::string error_msg;
  Value* extensions = serializer->Deserialize(NULL, &error_msg);
  if (!extensions) {
    LOG(WARNING) << "Unable to deserialize json data: " << error_msg
                 << " In file " << path.value() << " .";
  } else {
    if (!extensions->IsType(Value::TYPE_DICTIONARY)) {
      LOG(WARNING) << "Expected a JSON dictionary in file "
                   << path.value() << " .";
    } else {
      return static_cast<DictionaryValue*>(extensions);
    }
  }
  return new DictionaryValue;
}

}  // namespace

ExternalPrefExtensionLoader::ExternalPrefExtensionLoader(int base_path_key)
    : base_path_key_(base_path_key) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

const FilePath ExternalPrefExtensionLoader::GetBaseCrxFilePath() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // |base_path_| was set in LoadOnFileThread().
  return base_path_;
}

void ExternalPrefExtensionLoader::StartLoading() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this,
          &ExternalPrefExtensionLoader::LoadOnFileThread));
}

void ExternalPrefExtensionLoader::LoadOnFileThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // TODO(skerner): Some values of base_path_key_ will cause
  // PathService::Get() to return false, because the path does
  // not exist.  Find and fix the build/install scripts so that
  // this can become a CHECK().  Known examples include chrome
  // OS developer builds and linux install packages.
  // Tracked as crbug.com/70402 .

  scoped_ptr<DictionaryValue> prefs;
  if (PathService::Get(base_path_key_, &base_path_)) {
    FilePath json_file;
    json_file =
        base_path_.Append(FILE_PATH_LITERAL("external_extensions.json"));

    if (file_util::PathExists(json_file)) {
      JSONFileValueSerializer serializer(json_file);
      prefs.reset(ExtractPrefs(json_file, &serializer));
    }
  }

  if (!prefs.get())
    prefs.reset(new DictionaryValue());

  prefs_.reset(prefs.release());

  // If we have any records to process, then we must have
  // read the .json file.  If we read the .json file, then
  // we were should have set |base_path_|.
  if (!prefs_->empty())
    CHECK(!base_path_.empty());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this,
          &ExternalPrefExtensionLoader::LoadFinished));
}

ExternalTestingExtensionLoader::ExternalTestingExtensionLoader(
    const std::string& json_data,
    const FilePath& fake_base_path)
    : fake_base_path_(fake_base_path) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JSONStringValueSerializer serializer(json_data);
  FilePath fake_json_path = fake_base_path.AppendASCII("fake.json");
  testing_prefs_.reset(ExtractPrefs(fake_json_path, &serializer));
}

void ExternalTestingExtensionLoader::StartLoading() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prefs_.reset(testing_prefs_->DeepCopy());
  LoadFinished();
}

ExternalTestingExtensionLoader::~ExternalTestingExtensionLoader() {}

const FilePath ExternalTestingExtensionLoader::GetBaseCrxFilePath() {
  return fake_base_path_;
}
