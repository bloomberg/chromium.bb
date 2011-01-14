// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_extension_loader.h"

#include "app/app_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/json_value_serializer.h"

namespace {

// Caller takes ownership of the returned dictionary
DictionaryValue* ExtractPrefs(ValueSerializer* serializer) {
  std::string error_msg;
  Value* extensions = serializer->Deserialize(NULL, &error_msg);
  if (!extensions) {
    LOG(WARNING) << "Unable to deserialize json data: " << error_msg;
  } else {
    if (!extensions->IsType(Value::TYPE_DICTIONARY)) {
      NOTREACHED() << "Invalid json data";
    } else {
      return static_cast<DictionaryValue*>(extensions);
    }
  }
  return new DictionaryValue;
}

}  // namespace

ExternalPrefExtensionLoader::ExternalPrefExtensionLoader() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  FilePath json_file;
  PathService::Get(app::DIR_EXTERNAL_EXTENSIONS, &json_file);
  json_file = json_file.Append(FILE_PATH_LITERAL("external_extensions.json"));
  scoped_ptr<DictionaryValue> prefs;

  if (file_util::PathExists(json_file)) {
    JSONFileValueSerializer serializer(json_file);
    prefs.reset(ExtractPrefs(&serializer));
  } else {
    prefs.reset(new DictionaryValue());
  }

  prefs_.reset(prefs.release());
  BrowserThread::PostTask(
       BrowserThread::UI, FROM_HERE,
       NewRunnableMethod(
           this,
           &ExternalPrefExtensionLoader::LoadFinished));
}

ExternalTestingExtensionLoader::ExternalTestingExtensionLoader(
    const std::string& json_data) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JSONStringValueSerializer serializer(json_data);
  testing_prefs_.reset(ExtractPrefs(&serializer));
}

void ExternalTestingExtensionLoader::StartLoading() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prefs_.reset(
      static_cast<DictionaryValue*>(testing_prefs_->DeepCopy()));
  LoadFinished();
}
