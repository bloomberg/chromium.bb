// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"
#include "components/version_info/version_info.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/action_handlers_handler.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace app_runtime = api::app_runtime;

namespace {

class ActionHandlersManifestTest : public ManifestTest {
 protected:
  ManifestData CreateManifest(const std::string& action_handlers) {
    std::unique_ptr<base::DictionaryValue> manifest =
        base::DictionaryValue::From(base::test::ParseJson(R"json({
                                    "name": "test",
                                    "version": "1",
                                    "app": {
                                      "background": {
                                        "scripts": ["background.js"]
                                      }
                                    },
                                    "manifest_version": 2,
                                    "action_handlers": )json" +
                                                          action_handlers +
                                                          "}"));
    EXPECT_TRUE(manifest);
    return ManifestData(std::move(manifest), "test");
  }

  // Returns all action handlers associated with |extension|.
  std::set<app_runtime::ActionType> GetActionHandlers(
      const Extension* extension) {
    ActionHandlersInfo* info = static_cast<ActionHandlersInfo*>(
        extension->GetManifestData(manifest_keys::kActionHandlers));
    return info ? info->action_handlers : std::set<app_runtime::ActionType>();
  }
};

}  // namespace

TEST_F(ActionHandlersManifestTest, InvalidType) {
  LoadAndExpectError(CreateManifest("32"),
                     manifest_errors::kInvalidActionHandlersType);
  LoadAndExpectError(CreateManifest("[true]"),
                     manifest_errors::kInvalidActionHandlersType);
  LoadAndExpectError(CreateManifest("[\"invalid_handler\"]"),
                     manifest_errors::kInvalidActionHandlersActionType);
}

TEST_F(ActionHandlersManifestTest, VerifyParse) {
  scoped_refptr<Extension> none = LoadAndExpectSuccess(CreateManifest("[]"));
  EXPECT_EQ(std::set<app_runtime::ActionType>{}, GetActionHandlers(none.get()));

  scoped_refptr<Extension> new_note =
      LoadAndExpectSuccess(CreateManifest("[\"new_note\"]"));
  EXPECT_EQ(
      std::set<app_runtime::ActionType>{app_runtime::ACTION_TYPE_NEW_NOTE},
      GetActionHandlers(new_note.get()));
}

}  // namespace extensions
