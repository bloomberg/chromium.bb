// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function_registry.h"

#include "base/memory/singleton.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extensions_browser_client.h"

// static
ExtensionFunctionRegistry* ExtensionFunctionRegistry::GetInstance() {
  return Singleton<ExtensionFunctionRegistry>::get();
}

ExtensionFunctionRegistry::ExtensionFunctionRegistry() {
  extensions::ExtensionsBrowserClient* client =
      extensions::ExtensionsBrowserClient::Get();
  if (client) {
    client->RegisterExtensionFunctions(this);
  }
}

ExtensionFunctionRegistry::~ExtensionFunctionRegistry() {}

void ExtensionFunctionRegistry::GetAllNames(std::vector<std::string>* names) {
  for (FactoryMap::iterator iter = factories_.begin(); iter != factories_.end();
       ++iter) {
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
    : factory_(0), histogram_value_(extensions::functions::UNKNOWN) {}

ExtensionFunctionRegistry::FactoryEntry::FactoryEntry(
    ExtensionFunctionFactory factory,
    extensions::functions::HistogramValue histogram_value)
    : factory_(factory), histogram_value_(histogram_value) {}
