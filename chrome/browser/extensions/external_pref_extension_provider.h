// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_PROVIDER_H_
#pragma once

#include "chrome/browser/extensions/stateful_external_extension_provider.h"

// A specialization of the ExternalExtensionProvider that uses a json file to
// look up which external extensions are registered.
class ExternalPrefExtensionProvider : public StatefulExternalExtensionProvider {
 public:
  explicit ExternalPrefExtensionProvider();
  virtual ~ExternalPrefExtensionProvider();

  // Used only during testing to not use the json file for external extensions,
  // but instead parse a json file specified by the test.
  void SetPreferencesForTesting(const std::string& json_data_for_testing);

 private:
  void SetPreferences(ValueSerializer* serializer);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_PROVIDER_H_
