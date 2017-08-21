// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/test_util.h"

#include <utility>

#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

namespace extensions {
namespace test_util {

scoped_refptr<Extension> CreateEmptyExtension(const std::string& id) {
  return ExtensionBuilder()
      .SetManifest(
          DictionaryBuilder().Set("name", "test").Set("version", "0.1").Build())
      .SetID(id)
      .Build();
}

}  // namespace test_util
}  // namespace extensions
