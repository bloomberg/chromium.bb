// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_policy_loader.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"

namespace extensions {

ExternalPolicyLoader::ExternalPolicyLoader(ExtensionManagement *settings)
    : settings_(settings) {
  settings_->AddObserver(this);
}

ExternalPolicyLoader::~ExternalPolicyLoader() {
  settings_->RemoveObserver(this);
}

void ExternalPolicyLoader::OnExtensionManagementSettingsChanged() {
  StartLoading();
}

// static
void ExternalPolicyLoader::AddExtension(base::DictionaryValue* dict,
                                        const std::string& extension_id,
                                        const std::string& update_url) {
  dict->SetString(base::StringPrintf("%s.%s", extension_id.c_str(),
                                     ExternalProviderImpl::kExternalUpdateUrl),
                  update_url);
}

void ExternalPolicyLoader::StartLoading() {
  prefs_ = settings_->GetForceInstallList();
  LoadFinished();
}

}  // namespace extensions
