// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"

namespace extensions_features {

// Enables enforcement of Cross-Origin Read Blocking (CORB) for most extension
// content scripts, except ones that are on an allowlist.  See also
// https://crbug.com/846346 and DoContentScriptsDependOnRelaxedCorb function in
// extensions/browser/url_loader_factory_manager.cc.
const base::Feature kBypassCorbOnlyForExtensionsAllowlist{
    "BypassCorbOnlyForExtensionsAllowlist", base::FEATURE_ENABLED_BY_DEFAULT};
const char kBypassCorbAllowlistParamName[] = "BypassCorbExtensionsAllowlist";

// Enables the use of C++-based extension bindings (instead of JS generation).
const base::Feature kNativeCrxBindings{"NativeCrxBindings",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enables new extension updater service.
const base::Feature kNewExtensionUpdaterService{
    "NewExtensionUpdaterService", base::FEATURE_DISABLED_BY_DEFAULT};

// Forces extensions with <all_hosts> to use activeTab-style runtime host
// permissions.
const base::Feature kRuntimeHostPermissions{"RuntimeHostPermissions",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Forces requests to go through WebRequestProxyingURLLoaderFactory.
const base::Feature kForceWebRequestProxyForTest{
    "ForceWebRequestProxyForTest", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace extensions_features
