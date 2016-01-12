// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/path_service.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/shell/capability_filter_test.h"
#include "mojo/shell/fetcher.h"
#include "mojo/shell/package_manager/package_manager_impl.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/interfaces/content_handler.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace test {
namespace {

const char kTestMimeType[] = "test/mime-type";

// A custom Fetcher used to trigger a content handler for kTestMimeType for a
// specific test.
class TestFetcher : public Fetcher {
 public:
  TestFetcher(const GURL& url, const FetchCallback& callback)
      : Fetcher(callback),
        url_(url) {
    loader_callback_.Run(make_scoped_ptr(this));
  }
  ~TestFetcher() override {}

 private:
  // Overridden from Fetcher:
  const GURL& GetURL() const override { return url_; }
  GURL GetRedirectURL() const override { return GURL(); }
  GURL GetRedirectReferer() const override { return GURL(); }
  URLResponsePtr AsURLResponse(base::TaskRunner* task_runner,
                               uint32_t skip) override {
    URLResponsePtr response(URLResponse::New());
    response->url = url_.spec();
    return response;
  }
  void AsPath(
      base::TaskRunner* task_runner,
      base::Callback<void(const base::FilePath&, bool)> callback) override {}
  std::string MimeType() override { return kTestMimeType; }
  bool HasMojoMagic() override { return false; }
  bool PeekFirstLine(std::string* line) override { return false; }

  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(TestFetcher);
};

class TestPackageManager : public PackageManagerImpl {
 public:
  TestPackageManager(const base::FilePath& package_path)
      : PackageManagerImpl(package_path, nullptr) {}
  ~TestPackageManager() override {}

 private:
  // Overridden from PackageManagerImpl:
  void FetchRequest(
      URLRequestPtr request,
      const Fetcher::FetchCallback& loader_callback) override {
    new TestFetcher(GURL(request->url), loader_callback);
  }

  DISALLOW_COPY_AND_ASSIGN(TestPackageManager);
};

class TestContentHandler : public ApplicationDelegate,
                           public InterfaceFactory<ContentHandler>,
                           public ContentHandler {
 public:
  TestContentHandler() : app_(nullptr) {}
  ~TestContentHandler() override {}

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {
    app_ = app;
  }
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService<ContentHandler>(this);
    return true;
  }

  // Overridden from InterfaceFactory<ContentHandler>:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<ContentHandler> request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // Overridden from ContentHandler:
  void StartApplication(
      InterfaceRequest<Application> application,
      URLResponsePtr response,
      const Callback<void()>& destruct_callback) override {
    scoped_ptr<ApplicationDelegate> delegate(new test::TestApplication);
    embedded_apps_.push_back(
        new ApplicationImpl(delegate.get(), std::move(application)));
    embedded_app_delegates_.push_back(std::move(delegate));
    destruct_callback.Run();
  }

  ApplicationImpl* app_;
  WeakBindingSet<ContentHandler> bindings_;
  ScopedVector<ApplicationDelegate> embedded_app_delegates_;
  ScopedVector<ApplicationImpl> embedded_apps_;

  DISALLOW_COPY_AND_ASSIGN(TestContentHandler);
};

}  // namespace

class CapabilityFilterContentHandlerTest : public test::CapabilityFilterTest {
 public:
  CapabilityFilterContentHandlerTest()
      : package_manager_(nullptr) {
    base::FilePath shell_dir;
    PathService::Get(base::DIR_MODULE, &shell_dir);
    package_manager_ = new TestPackageManager(shell_dir);
  }
  ~CapabilityFilterContentHandlerTest() override {}

 private:
  // Overridden from CapabilityFilterTest:
  PackageManager* CreatePackageManager() override {
    return package_manager_;
  }
  void SetUp() override {
    test::CapabilityFilterTest::SetUp();

    GURL content_handler_url("test:content_handler");
    package_manager_->RegisterContentHandler(kTestMimeType,
                                             content_handler_url);
    CreateLoader<TestContentHandler>(content_handler_url.spec());
  }

  // Owned by ApplicationManager in base class.
  PackageManagerImpl* package_manager_;

  DISALLOW_COPY_AND_ASSIGN(CapabilityFilterContentHandlerTest);
};

TEST_F(CapabilityFilterContentHandlerTest, Blocking) {
  RunBlockingTest();
}

TEST_F(CapabilityFilterContentHandlerTest, Wildcards) {
  RunWildcardTest();
}

}  // namespace test
}  // namespace shell
}  // namespace mojo
