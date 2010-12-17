// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_extension_provider.h"

#include "app/app_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/stateful_external_extension_provider.h"
#include "chrome/common/json_value_serializer.h"

ExternalPrefExtensionProvider::ExternalPrefExtensionProvider()
  : StatefulExternalExtensionProvider(Extension::EXTERNAL_PREF,
                                      Extension::EXTERNAL_PREF_DOWNLOAD) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FilePath json_file;
  PathService::Get(app::DIR_EXTERNAL_EXTENSIONS, &json_file);
  json_file = json_file.Append(FILE_PATH_LITERAL("external_extensions.json"));

  if (file_util::PathExists(json_file)) {
    JSONFileValueSerializer serializer(json_file);
    SetPreferences(&serializer);
  } else {
    set_prefs(new DictionaryValue());
  }
}

ExternalPrefExtensionProvider::~ExternalPrefExtensionProvider() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ExternalPrefExtensionProvider::SetPreferencesForTesting(
    const std::string& json_data_for_testing) {
  JSONStringValueSerializer serializer(json_data_for_testing);
  SetPreferences(&serializer);
}

void ExternalPrefExtensionProvider::SetPreferences(
    ValueSerializer* serializer) {
  std::string error_msg;
  Value* extensions = serializer->Deserialize(NULL, &error_msg);
  scoped_ptr<DictionaryValue> dictionary(new DictionaryValue());
  if (!extensions) {
    LOG(WARNING) << "Unable to deserialize json data: " << error_msg;
  } else {
    if (!extensions->IsType(Value::TYPE_DICTIONARY)) {
      NOTREACHED() << "Invalid json data";
    } else {
      dictionary.reset(static_cast<DictionaryValue*>(extensions));
    }
  }
  set_prefs(dictionary.release());
}
