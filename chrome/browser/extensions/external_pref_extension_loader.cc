// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_extension_loader.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/browser_thread.h"
#include "content/common/json_value_serializer.h"

namespace {

// Caller takes ownership of the returned dictionary.
DictionaryValue* ExtractPrefs(const FilePath& path,
                              base::ValueSerializer* serializer) {
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

ExternalPrefExtensionLoader::ExternalPrefExtensionLoader(int base_path_key,
                                                         Options options)
    : base_path_key_(base_path_key),
      options_(options){
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

DictionaryValue* ExternalPrefExtensionLoader::ReadJsonPrefsFile() {
  // TODO(skerner): Some values of base_path_key_ will cause
  // PathService::Get() to return false, because the path does
  // not exist.  Find and fix the build/install scripts so that
  // this can become a CHECK().  Known examples include chrome
  // OS developer builds and linux install packages.
  // Tracked as crbug.com/70402 .
  if (!PathService::Get(base_path_key_, &base_path_)) {
    return NULL;
  }

  FilePath json_file = base_path_.Append(
      FILE_PATH_LITERAL("external_extensions.json"));

  if (!file_util::PathExists(json_file)) {
    // This is not an error.  The file does not exist by default.
    return NULL;
  }

  if (IsOptionSet(ENSURE_PATH_CONTROLLED_BY_ADMIN)) {
#if defined(OS_MACOSX)
    if (!file_util::VerifyPathControlledByAdmin(json_file)) {
      LOG(ERROR) << "Can not read external extensions source.  The file "
                 << json_file.value() << " and every directory in its path, "
                 << "must be owned by root, have group \"admin\", and not be "
                 << "writable by all users. These restrictions prevent "
                 << "unprivleged users from making chrome install extensions "
                 << "on other users' accounts.";
      return NULL;
    }
#else
    // The only platform that uses this check is Mac OS.  If you add one,
    // you need to implement file_util::VerifyPathControlledByAdmin() for
    // that platform.
    NOTREACHED();
#endif  // defined(OS_MACOSX)
  }

  JSONFileValueSerializer serializer(json_file);
  return ExtractPrefs(json_file, &serializer);
}

void ExternalPrefExtensionLoader::LoadOnFileThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  prefs_.reset(ReadJsonPrefsFile());
  if (!prefs_.get())
    prefs_.reset(new DictionaryValue());

  // We want to deprecate the external extensions file inside the app
  // bundle on mac os.  Use a histogram to see how many extensions
  // are installed using the deprecated path, and how many are installed
  // from the supported path.  We can use this data to measure the
  // effectiveness of asking developers to use the new path, or any
  // automatic migration methods we implement.
#if defined(OS_MACOSX)
  // The deprecated path only exists on mac for now.
  if (base_path_key_ == chrome::DIR_DEPRICATED_EXTERNAL_EXTENSIONS) {
    UMA_HISTOGRAM_COUNTS_100("Extensions.DepricatedExternalJsonCount",
                             prefs_->size());
  }
#endif  // defined(OS_MACOSX)
  if (base_path_key_ == chrome::DIR_EXTERNAL_EXTENSIONS) {
    UMA_HISTOGRAM_COUNTS_100("Extensions.ExternalJsonCount",
                             prefs_->size());
  }

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
