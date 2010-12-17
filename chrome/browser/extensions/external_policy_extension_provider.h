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
// registered. The value of this preference is received via the constructor and
// via |SetPreferences| in case of run-time updates.
// Instances of this class are expected to be created and destroyed on the UI
// thread and they are expecting public method calls from the FILE thread.
class ExternalPolicyExtensionProvider
    : public StatefulExternalExtensionProvider {
 public:
  explicit ExternalPolicyExtensionProvider(const ListValue* forcelist);
  virtual ~ExternalPolicyExtensionProvider();

  // Set the internal list of extensions based on |forcelist|.
  // Does not take ownership of |forcelist|.
  void SetPreferences(const ListValue* forcelist);

 private:
  friend class MockExternalPolicyExtensionProviderVisitor;

  // Set the internal list of extensions based on |forcelist|.
  // Does not take ownership of |forcelist|.
  void ProcessPreferences(const ListValue* forcelist);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_EXTENSION_PROVIDER_H_
