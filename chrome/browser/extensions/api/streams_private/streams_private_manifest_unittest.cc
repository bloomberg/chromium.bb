// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/extensions/mime_types_handler.h"
#include "chrome/common/extensions/value_builder.h"
#include "extensions/common/error_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::ListBuilder;

namespace {

class StreamsPrivateManifestTest : public ExtensionManifestTest {
};

TEST_F(StreamsPrivateManifestTest, ValidMimeTypesHandlerMIMETypes) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetID(extension_misc::kQuickOfficeExtensionId)
      .SetManifest(DictionaryBuilder()
                   .Set("name", "MIME type handler test")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("mime_types", ListBuilder()
                       .Append("plain/text")))
      .Build();

  ASSERT_TRUE(extension.get());
  MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  ASSERT_TRUE(handler != NULL);

  EXPECT_FALSE(handler->CanHandleMIMEType("plain/html"));
  EXPECT_TRUE(handler->CanHandleMIMEType("plain/text"));
}

TEST_F(StreamsPrivateManifestTest,
       MimeTypesHandlerMIMETypesNotWhitelisted) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "MIME types test")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("mime_types", ListBuilder()
                        .Append("plain/text")))
      .Build();

  ASSERT_TRUE(extension.get());

  MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  ASSERT_TRUE(handler == NULL);
}

}  // namespace
