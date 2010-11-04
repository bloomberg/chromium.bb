// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_EXTENSION_PROVIDER_H_
#pragma once

#include "chrome/browser/extensions/stateful_external_extension_provider.h"

class ListValue;
class MockExternalPolicyExtensionProviderVisitor;
class PrefService;

// A specialization of the ExternalExtensionProvider that uses
// prefs::kExtensionInstallForceList to look up which external extensions are
// registered.
class ExternalPolicyExtensionProvider
    : public StatefulExternalExtensionProvider {
 public:
  explicit ExternalPolicyExtensionProvider();
  virtual ~ExternalPolicyExtensionProvider();

  // Set the internal list of extensions based on
  // prefs::kExtensionInstallForceList.
  void SetPreferences(PrefService* prefs);

 private:
  friend class MockExternalPolicyExtensionProviderVisitor;

  // Set the internal list of extensions based on |forcelist|.
  void SetPreferences(const ListValue* forcelist);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_EXTENSION_PROVIDER_H_
