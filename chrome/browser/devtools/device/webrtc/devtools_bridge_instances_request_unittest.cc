// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/webrtc/devtools_bridge_instances_request.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockDelegate : public DevToolsBridgeInstancesRequest::Delegate {
 public:
  DevToolsBridgeInstancesRequest::InstanceList instances;

  void OnDevToolsBridgeInstancesRequestSucceeded(
      const DevToolsBridgeInstancesRequest::InstanceList& result) override {
    instances = result;
  }

  void OnDevToolsBridgeInstancesRequestFailed() override {}
};

base::FilePath GetTestFilePath(const std::string& file_name) {
  base::FilePath path;
  if (!PathService::Get(chrome::DIR_TEST_DATA, &path))
    return base::FilePath();
  return path.AppendASCII("devtools")
      .AppendASCII("webrtc_device_provider")
      .AppendASCII(file_name);
}

}  // namespace

TEST(DevToolsBridgeInstancesRequestTest, ParseResponse) {
  std::string input;
  ASSERT_TRUE(base::ReadFileToString(
      GetTestFilePath("devtools_bridge_instances_response.json"), &input));
  base::JSONReader reader;
  scoped_ptr<base::Value> root(reader.ReadToValue(input));
  ASSERT_TRUE(root.get()) << reader.GetErrorMessage();
  EXPECT_TRUE(root->IsType(base::Value::TYPE_DICTIONARY));

  const base::DictionaryValue* dictionary = NULL;
  ASSERT_TRUE(root->GetAsDictionary(&dictionary));

  MockDelegate delegate;

  delegate.instances.resize(10);

  DevToolsBridgeInstancesRequest(&delegate).OnGCDAPIFlowComplete(*dictionary);

  ASSERT_TRUE(delegate.instances.size() == 1);
  ASSERT_EQ("ab911465-83c7-e335-ea64-cb656868cbe0", delegate.instances[0].id);
}
