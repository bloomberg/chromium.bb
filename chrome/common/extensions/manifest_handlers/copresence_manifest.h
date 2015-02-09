// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_COPRESENCE_MANIFEST_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_COPRESENCE_MANIFEST_H_

#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// Parses the "copresence" manifest key.
// TODO(ckehoe): Handle the copresence permission here.
class CopresenceManifestHandler final : public ManifestHandler {
 public:
  CopresenceManifestHandler();
  ~CopresenceManifestHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;

 private:
  const std::vector<std::string> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(CopresenceManifestHandler);
};

// The parsed data from the copresence manifest entry.
struct CopresenceManifestData final : public Extension::ManifestData {
 public:
  CopresenceManifestData();
  ~CopresenceManifestData() override;

  std::string api_key;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_COPRESENCE_MANIFEST_H_
