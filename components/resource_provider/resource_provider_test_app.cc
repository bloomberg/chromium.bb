// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/resource_provider/public/cpp/resource_loader.h"
#include "components/resource_provider/public/interfaces/resource_provider.mojom.h"
#include "components/resource_provider/test.mojom.h"
#include "mojo/platform_handle/platform_handle_functions.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/shell_client.h"

namespace resource_provider {
namespace test {

std::string ReadFile(base::File* file) {
  const size_t kBufferSize = 1 << 16;
  std::unique_ptr<char[]> buffer(new char[kBufferSize]);
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

class Test : public mojom::Test {
 public:
  explicit Test(shell::Connector* connector) : connector_(connector) {}
  ~Test() override {}

  // mojom::Test:
  void GetResource1(const GetResource1Callback& callback) override {
    printf("GetResource1\n");
    ResourceContentsMap results(GetResources(SetWithString("sample")));
    printf("GetResource1 result %s\n", results["sample"].c_str());
    callback.Run(results["sample"]);
  }
  void GetBothResources(const GetBothResourcesCallback& callback) override {
    ResourceContentsMap results(
        GetResources(SetWithStrings("sample", "dir/sample2")));
    callback.Run(results["sample"], results["dir/sample2"]);
  }

 private:
  using ResourceContentsMap = std::map<std::string, std::string>;

  ResourceContentsMap GetResources(const std::set<std::string>& paths) {
    ResourceLoader loader(connector_, paths);
    loader.BlockUntilLoaded();

    // Load the contents of each of the handles.
    ResourceContentsMap results;
    for (auto& path : paths) {
      base::File file(loader.ReleaseFile(path));
      results[path] = ReadFile(&file);
    }
    return results;
  }

  shell::Connector* connector_;

  DISALLOW_COPY_AND_ASSIGN(Test);
};

class TestApp : public shell::ShellClient,
                public shell::InterfaceFactory<mojom::Test> {
 public:
  TestApp() {}
  ~TestApp() override {}

  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override {
    test_.reset(new Test(connector));
  }
  bool AcceptConnection(shell::Connection* connection) override {
    connection->AddInterface<mojom::Test>(this);
    return true;
  }

  // InterfaceFactory<mojom::Test>:
  void Create(shell::Connection* connection,
              mojom::TestRequest request) override {
    printf("test app create\n");
    bindings_.AddBinding(test_.get(), std::move(request));
  }

 private:
  std::unique_ptr<Test> test_;
  mojo::BindingSet<test::mojom::Test> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestApp);
};

}  // namespace test
}  // namespace resource_provider

MojoResult MojoMain(MojoHandle shell_handle) {
  return shell::ApplicationRunner(new resource_provider::test::TestApp)
      .Run(shell_handle);
}
