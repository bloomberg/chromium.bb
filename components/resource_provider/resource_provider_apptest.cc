// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/resource_provider/public/cpp/resource_loader.h"
#include "components/resource_provider/public/interfaces/resource_provider.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/platform_handle/platform_handle_functions.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/cpp/service_provider_impl.h"

namespace resource_provider {
namespace {

std::string ReadFile(base::File* file) {
  const size_t kBufferSize = 1 << 16;
  scoped_ptr<char[]> buffer(new char[kBufferSize]);
  const int read = file->ReadAtCurrentPos(buffer.get(), kBufferSize);
  if (read == -1)
    return std::string();
  return std::string(buffer.get(), read);
}

std::set<std::string> SetWithString(const std::string& contents) {
  std::set<std::string> result;
  result.insert(contents);
  return result;
}

std::set<std::string> SetWithStrings(const std::string& contents1,
                                     const std::string& contents2) {
  std::set<std::string> result;
  result.insert(contents1);
  result.insert(contents2);
  return result;
}

class ResourceProviderApplicationTest : public mojo::test::ApplicationTestBase {
 public:
  ResourceProviderApplicationTest() {}
  ~ResourceProviderApplicationTest() override {}

 protected:
  using ResourceContentsMap = std::map<std::string, std::string>;

  // Queries ResourceProvider for the specified resources, blocking until the
  // resources are returned. The return map maps from the path to the contents
  // of the file at the specified path.
  ResourceContentsMap GetResources(const std::set<std::string>& paths) {
    ResourceLoader loader(shell(), paths);
    loader.BlockUntilLoaded();

    // Load the contents of each of the handles.
    ResourceContentsMap results;
    for (auto& path : paths) {
      base::File file(loader.ReleaseFile(path));
      results[path] = ReadFile(&file);
    }
    return results;
  }

  // ApplicationTestBase:
  void SetUp() override {
    ApplicationTestBase::SetUp();
  }

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(ResourceProviderApplicationTest);
};

TEST_F(ResourceProviderApplicationTest, FetchOneResource) {
  ResourceContentsMap results(GetResources(SetWithString("sample")));
  ASSERT_TRUE(results.count("sample") > 0u);
  EXPECT_EQ("test data\n", results["sample"]);
}

TEST_F(ResourceProviderApplicationTest, FetchTwoResources) {
  ResourceContentsMap results(
      GetResources(SetWithStrings("sample", "dir/sample2")));
  ASSERT_TRUE(results.count("sample") > 0u);
  EXPECT_EQ("test data\n", results["sample"]);

  ASSERT_TRUE(results.count("dir/sample2") > 0u);
  EXPECT_EQ("xxyy\n", results["dir/sample2"]);
}

}  // namespace
}  // namespace resource_provider
