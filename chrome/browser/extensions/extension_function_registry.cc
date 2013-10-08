// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_registry.h"

#include "chrome/browser/extensions/api/identity/experimental_identity_api.h"
#include "chrome/browser/extensions/api/preference/chrome_direct_setting.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/api/runtime/runtime_api.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/common/extensions/api/generated_api.h"

// static
ExtensionFunctionRegistry* ExtensionFunctionRegistry::GetInstance() {
  return Singleton<ExtensionFunctionRegistry>::get();
}

ExtensionFunctionRegistry::ExtensionFunctionRegistry() {
  ResetFunctions();
}

ExtensionFunctionRegistry::~ExtensionFunctionRegistry() {
}

void ExtensionFunctionRegistry::ResetFunctions() {
#if defined(ENABLE_EXTENSIONS)

  // Register all functions here.

  // WebRequest.
  RegisterFunction<WebRequestAddEventListener>();
  RegisterFunction<WebRequestEventHandled>();

  // Preferences.
  RegisterFunction<extensions::GetPreferenceFunction>();
  RegisterFunction<extensions::SetPreferenceFunction>();
  RegisterFunction<extensions::ClearPreferenceFunction>();

  // Direct Preference Access for Component Extensions.
  RegisterFunction<extensions::chromedirectsetting::GetDirectSettingFunction>();
  RegisterFunction<extensions::chromedirectsetting::SetDirectSettingFunction>();
  RegisterFunction<
      extensions::chromedirectsetting::ClearDirectSettingFunction>();

  // Runtime
  RegisterFunction<extensions::RuntimeGetBackgroundPageFunction>();
  RegisterFunction<extensions::RuntimeSetUninstallUrlFunction>();
  RegisterFunction<extensions::RuntimeReloadFunction>();
  RegisterFunction<extensions::RuntimeRequestUpdateCheckFunction>();
  RegisterFunction<extensions::RuntimeRestartFunction>();

  // ExperimentalIdentity.
  RegisterFunction<extensions::ExperimentalIdentityGetAuthTokenFunction>();
  RegisterFunction<extensions::ExperimentalIdentityLaunchWebAuthFlowFunction>();

  // Generated APIs
  extensions::api::GeneratedFunctionRegistry::RegisterAll(this);
#endif  // defined(ENABLE_EXTENSIONS)
}

void ExtensionFunctionRegistry::GetAllNames(std::vector<std::string>* names) {
  for (FactoryMap::iterator iter = factories_.begin();
       iter != factories_.end(); ++iter) {
    names->push_back(iter->first);
  }
}

bool ExtensionFunctionRegistry::OverrideFunction(
    const std::string& name,
    ExtensionFunctionFactory factory) {
  FactoryMap::iterator iter = factories_.find(name);
  if (iter == factories_.end()) {
    return false;
  } else {
    iter->second.factory_ = factory;
    return true;
  }
}

ExtensionFunction* ExtensionFunctionRegistry::NewFunction(
    const std::string& name) {
  FactoryMap::iterator iter = factories_.find(name);
  if (iter == factories_.end()) {
    return NULL;
  }
  ExtensionFunction* function = iter->second.factory_();
  function->set_name(name);
  function->set_histogram_value(iter->second.histogram_value_);
  return function;
}

ExtensionFunctionRegistry::FactoryEntry::FactoryEntry()
    : factory_(0), histogram_value_(extensions::functions::UNKNOWN) {
}

ExtensionFunctionRegistry::FactoryEntry::FactoryEntry(
    ExtensionFunctionFactory factory,
    extensions::functions::HistogramValue histogram_value)
    : factory_(factory), histogram_value_(histogram_value) {
}
