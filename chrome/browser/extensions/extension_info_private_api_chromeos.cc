// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_info_private_api_chromeos.h"

#include <map>
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/system_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/browser/browser_thread.h"

namespace {

// Key which corresponds to the HWID setting.
const char kPropertyHWID[] = "hwid";

// Path to OEM partner startup customization manifest.
const char kStartupCustomizationManifestPath[] =
    "/opt/oem/etc/startup_manifest.json";

// Keeps cached values to avoid unnecessary file operations. Should only be
// used from the UI thread.
class CachedProperties : base::NonThreadSafe {
 public:
  ~CachedProperties();
  // Gets value for the property with the given name. Return whether value has
  // been found.
  bool GetValue(const std::string& property_name, std::string* value);

  // Updates cached properties with the given item.
  void Update(const std::pair<std::string, std::string>& item);

 private:
  typedef std::map<std::string, std::string> PropertyMap;
  PropertyMap cache_;
};

CachedProperties::~CachedProperties() {
  // It is safe to delete this class on any thread since class is used
  // through LazyInstance.
  DetachFromThread();
}

bool CachedProperties::GetValue(const std::string& property_name,
                                std::string* value) {
  DCHECK(CalledOnValidThread());
  PropertyMap::iterator iter = cache_.find(property_name);
  if (iter != cache_.end()) {
    (*value) = iter->second;
    return true;
  }
  return false;
}

// Updates cached properties with the given value of the property.
void CachedProperties::Update(
    const std::pair<std::string, std::string>& item) {
  DCHECK(CalledOnValidThread());
  cache_.insert(item);
}

// Provides customization properties. Should be used only on the FILE thread.
class CustomizationData : base::NonThreadSafe {
 public:
  CustomizationData();
  ~CustomizationData();

  // Gets value for the property with the given name. Return whether value has
  // been found.
  bool GetValue(const std::string& property_name, std::string* value);

 private:
  // Keeps customization document from which properties are extracted.
  chromeos::StartupCustomizationDocument document_;

  DISALLOW_COPY_AND_ASSIGN(CustomizationData);
};

CustomizationData::CustomizationData() {
  DCHECK(CalledOnValidThread());
  document_.LoadManifestFromFile(FilePath(kStartupCustomizationManifestPath));
}

CustomizationData::~CustomizationData() {
  // It is safe to delete this class on any thread since class is used
  // through LazyInstance.
  DetachFromThread();
}

bool CustomizationData::GetValue(const std::string& property_name,
                                 std::string* value) {
  DCHECK(CalledOnValidThread());
  if (property_name == kPropertyHWID) {
    (*value) = document_.GetHWID();
  } else {
    LOG(ERROR) << "Unknown property request: " << property_name;
    return false;
  }
  return true;
}

// Shared instances.
base::LazyInstance<CachedProperties> g_cached_properties(
    base::LINKER_INITIALIZED);
base::LazyInstance<CustomizationData> g_customization(base::LINKER_INITIALIZED);

}  // namespace

GetChromeosInfoFunction::GetChromeosInfoFunction()
    : result_dictionary_(new DictionaryValue) {
}

GetChromeosInfoFunction::~GetChromeosInfoFunction() {
}

bool GetChromeosInfoFunction::RunImpl() {
  ListValue* list = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(0, &list));
  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string property_name;
    EXTENSION_FUNCTION_VALIDATE(list->GetString(i, &property_name));
    std::string value;
    if (g_cached_properties.Get().GetValue(property_name, &value)) {
      result_dictionary_->Set(property_name, Value::CreateStringValue(value));
    } else {
      properties_.push_back(property_name);
    }
  }

  if (!properties_.empty()) {
    // This will calls us back on UI thread.
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        NewRunnableMethod(this,
                          &GetChromeosInfoFunction::LoadValues));
  } else {
    // Early answer is ready.
    result_.reset(result_dictionary_.release());
    SendResponse(true);
  }
  return true;
}

void GetChromeosInfoFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (size_t i = 0; i < new_results_.size(); ++i) {
    result_dictionary_->Set(new_results_[i].first,
                            Value::CreateStringValue(new_results_[i].second));
    g_cached_properties.Get().Update(new_results_[i]);
  }
  result_.reset(result_dictionary_.release());
  SendResponse(true);
}

void GetChromeosInfoFunction::LoadValues() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  new_results_.clear();
  for (size_t i = 0; i < properties_.size(); ++i) {
    std::string value;
    if (g_customization.Get().GetValue(properties_[i], &value))
      new_results_.push_back(std::make_pair(properties_[i], value));
  }
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &GetChromeosInfoFunction::RespondOnUIThread));
}
