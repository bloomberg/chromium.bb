// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_test_base.h"

#include "base/macros.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

// A test launcher for tests for the mojoPrivate API defined in
// extensions/test/data/mojo_private_unittest.js.

namespace extensions {

class MojoPrivateApiTest : public ApiTestBase {
 public:
  MojoPrivateApiTest() = default;

  scoped_refptr<const Extension> CreateExtension() override {
    std::unique_ptr<base::DictionaryValue> manifest =
        DictionaryBuilder()
            .Set("name", "test")
            .Set("version", "1.0")
            .Set("manifest_version", 2)
            .Build();
    // Return an extension whitelisted for the mojoPrivate API.
    return ExtensionBuilder()
        .SetManifest(std::move(manifest))
        .SetID("pkedcjkdefgpdelpbcmbmeomcjbeemfm")
        .Build();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoPrivateApiTest);
};

TEST_F(MojoPrivateApiTest, RequireAsync) {
  env()->RegisterModule("add",
                        "define('add', [], function() {"
                        "  return { Add: function(x, y) { return x + y; } };"
                        "});");
  ASSERT_NO_FATAL_FAILURE(
      RunTest("mojo_private_unittest.js", "testRequireAsync"));
}

}  // namespace extensions
