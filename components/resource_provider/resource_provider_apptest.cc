// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/files/file.h"
#include "base/run_loop.h"
#include "components/resource_provider/public/interfaces/resource_provider.mojom.h"
#include "mojo/application/application_test_base_chromium.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/platform_handle/platform_handle_functions.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_delegate.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/application/service_provider_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/system/macros.h"

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

std::vector<std::string> VectorWithString(const std::string& contents) {
  std::vector<std::string> result;
  result.push_back(contents);
  return result;
}

std::vector<std::string> VectorWithStrings(const std::string& contents1,
                                           const std::string& contents2) {
  std::vector<std::string> result;
  result.push_back(contents1);
  result.push_back(contents2);
  return result;
}

// ResourceFetcher fetches resources from a ResourceProvider, storing the
// results as well as running
// a message loop until the resource has been obtained.
class ResourceFetcher {
 public:
  using ResourceMap = std::map<std::string, std::string>;

  explicit ResourceFetcher(ResourceProvider* provider)
      : provider_(provider), waiting_count_(0u) {}
  ~ResourceFetcher() {}

  ResourceMap* resources() { return &resources_; }

  void WaitForResults() {
    ASSERT_NE(0u, waiting_count_);
    run_loop_.Run();
  }

  void GetResources(const std::vector<std::string>& paths) {
    waiting_count_++;
    provider_->GetResources(mojo::Array<mojo::String>::From(paths),
                            base::Bind(&ResourceFetcher::OnGotResources,
                                       base::Unretained(this), paths));
  }

 private:
  // Callback when a resource has been fetched.
  void OnGotResources(const std::vector<std::string>& paths,
                      mojo::Array<mojo::ScopedHandle> resources) {
    ASSERT_FALSE(resources.is_null());
    ASSERT_EQ(paths.size(), resources.size());
    for (size_t i = 0; i < paths.size(); ++i) {
      std::string contents;
      if (resources[i].is_valid()) {
        MojoPlatformHandle platform_handle;
        ASSERT_EQ(MOJO_RESULT_OK,
                  MojoExtractPlatformHandle(resources[i].release().value(),
                                            &platform_handle));
        base::File file(platform_handle);
        contents = ReadFile(&file);
      }
      resources_[paths[i]] = contents;
    }

    if (waiting_count_ > 0) {
      waiting_count_--;
      if (waiting_count_ == 0)
        run_loop_.Quit();
    }
  }

  ResourceProvider* provider_;
  ResourceMap resources_;
  // Number of resources we're waiting on.
  size_t waiting_count_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetcher);
};

class ResourceProviderApplicationTest : public mojo::test::ApplicationTestBase {
 public:
  ResourceProviderApplicationTest() {}
  ~ResourceProviderApplicationTest() override {}

 protected:
  // ApplicationTestBase:
  void SetUp() override {
    ApplicationTestBase::SetUp();
    application_impl()->ConnectToService("mojo:resource_provider",
                                         &resource_provider_);
  }

  ResourceProviderPtr resource_provider_;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(ResourceProviderApplicationTest);
};

TEST_F(ResourceProviderApplicationTest, FetchOneResource) {
  ResourceFetcher fetcher(resource_provider_.get());
  fetcher.GetResources(VectorWithString("sample"));
  fetcher.WaitForResults();
  ASSERT_TRUE(fetcher.resources()->count("sample") > 0u);
  EXPECT_EQ("test data\n", (*fetcher.resources())["sample"]);
}

TEST_F(ResourceProviderApplicationTest, FetchTwoResources) {
  ResourceFetcher fetcher(resource_provider_.get());
  fetcher.GetResources(VectorWithStrings("sample", "dir/sample2"));
  fetcher.WaitForResults();
  ASSERT_TRUE(fetcher.resources()->count("sample") > 0u);
  EXPECT_EQ("test data\n", (*fetcher.resources())["sample"]);

  ASSERT_TRUE(fetcher.resources()->count("dir/sample2") > 0u);
  EXPECT_EQ("xxyy\n", (*fetcher.resources())["dir/sample2"]);
}

}  // namespace
}  // namespace resource_provider
