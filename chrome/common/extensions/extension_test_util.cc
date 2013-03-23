// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace extension_test_util {

scoped_refptr<Extension> CreateExtensionWithID(std::string id) {
  DictionaryValue values;
  values.SetString(extension_manifest_keys::kName, "test");
  values.SetString(extension_manifest_keys::kVersion, "0.1");
  std::string error;
  return Extension::Create(base::FilePath(), extensions::Manifest::INTERNAL,
                           values, Extension::NO_FLAGS, id, &error);
}

}  // namespace extension_test_util
