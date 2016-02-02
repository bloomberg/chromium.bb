// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/connect_util.h"
#include "mojo/shell/fetcher.h"
#include "mojo/shell/package_manager/package_manager_impl.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/interfaces/content_handler.mojom.h"
#include "mojo/shell/public/interfaces/service_provider.mojom.h"
#include "mojo/shell/test_package_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace test {
namespace {

const char kTestMimeType[] = "test/mime-type";

class TestFetcher : public Fetcher {
 public:
  TestFetcher(const FetchCallback& fetch_callback,
              const GURL& url,
              const std::string& mime_type)
      : Fetcher(fetch_callback), url_(url), mime_type_(mime_type) {
    loader_callback_.Run(make_scoped_ptr(this));
  }
  ~TestFetcher() override {}

  // Fetcher:
  const GURL& GetURL() const override { return url_; }
  GURL GetRedirectURL() const override { return GURL("yyy"); }
  GURL GetRedirectReferer() const override { return GURL(); }
  URLResponsePtr AsURLResponse(base::TaskRunner* task_runner,
                               uint32_t skip) override {
    return URLResponse::New();
  }
  void AsPath(
      base::TaskRunner* task_runner,
      base::Callback<void(const base::FilePath&, bool)> callback) override {}
  std::string MimeType() override { return mime_type_; }
  bool HasMojoMagic() override { return false; }
  bool PeekFirstLine(std::string* line) override { return false; }

 private:
  const GURL url_;
  const std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(TestFetcher);
};

void QuitClosure(bool* value) {
  *value = true;
  base::MessageLoop::current()->QuitWhenIdle();
}

class TestContentHandler : public ContentHandler, public ApplicationDelegate {
 public:
  TestContentHandler(ApplicationConnection* connection,
                     InterfaceRequest<ContentHandler> request)
      : binding_(this, std::move(request)) {}

  // ContentHandler:
  void StartApplication(
      InterfaceRequest<Application> application_request,
      URLResponsePtr response,
      const Callback<void()>& destruct_callback) override {
    apps_.push_back(new ApplicationImpl(this, std::move(application_request)));
    destruct_callback.Run();
  }

 private:
  StrongBinding<ContentHandler> binding_;
  ScopedVector<ApplicationImpl> apps_;

  DISALLOW_COPY_AND_ASSIGN(TestContentHandler);
};

class TestApplicationLoader : public ApplicationLoader,
                              public ApplicationDelegate,
                              public InterfaceFactory<ContentHandler> {
 public:
  TestApplicationLoader() : num_loads_(0) {}
  ~TestApplicationLoader() override {}

  int num_loads() const { return num_loads_; }
  const GURL& last_requestor_url() const { return last_requestor_url_; }

 private:
  // ApplicationLoader implementation.
  void Load(const GURL& url,
            InterfaceRequest<Application> application_request) override {
    ++num_loads_;
    test_app_.reset(new ApplicationImpl(this, std::move(application_request)));
  }

  // ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService<ContentHandler>(this);
    last_requestor_url_ = GURL(connection->GetRemoteApplicationURL());
    return true;
  }
  // InterfaceFactory<ContentHandler> implementation.
  void Create(ApplicationConnection* connection,
              InterfaceRequest<ContentHandler> request) override {
    new TestContentHandler(connection, std::move(request));
  }

  scoped_ptr<ApplicationImpl> test_app_;
  int num_loads_;
  GURL last_requestor_url_;

  DISALLOW_COPY_AND_ASSIGN(TestApplicationLoader);
};

class TestPackageManagerImpl : public PackageManagerImpl {
 public:
  explicit TestPackageManagerImpl(const base::FilePath& package_path)
      : PackageManagerImpl(package_path, nullptr, nullptr),
        mime_type_(kTestMimeType) {}
  ~TestPackageManagerImpl() override {}

  void set_mime_type(const std::string& mime_type) {
    mime_type_ = mime_type;
  }

  // PackageManagerImpl:
  void FetchRequest(
      URLRequestPtr request,
      const Fetcher::FetchCallback& loader_callback) override {
    new TestFetcher(loader_callback, GURL(request->url.get()), mime_type_);
  }

 private:
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(TestPackageManagerImpl);
};

}  // namespace

class ContentHandlerTest : public testing::Test {
 public:
  ContentHandlerTest()
      : content_handler_url_("http://test.content.handler"),
        requestor_url_("http://requestor.url") {}
  ~ContentHandlerTest() override {}

  void SetUp() override {
    base::FilePath shell_dir;
    PathService::Get(base::DIR_MODULE, &shell_dir);
    test_package_manager_ = new TestPackageManagerImpl(shell_dir);
    test_package_manager_->RegisterContentHandler(kTestMimeType,
                                                  content_handler_url_);
    application_manager_.reset(new ApplicationManager(
        make_scoped_ptr(test_package_manager_)));
  }

  void TearDown() override {
    test_package_manager_ = nullptr;
    application_manager_.reset();
  }

 protected:
  const GURL content_handler_url_;
  const GURL requestor_url_;

  base::MessageLoop loop_;
  scoped_ptr<ApplicationManager> application_manager_;
  // Owned by ApplicationManager.
  TestPackageManagerImpl* test_package_manager_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerTest);
};

TEST_F(ContentHandlerTest, ContentHandlerConnectionGetsRequestorURL) {
  TestApplicationLoader* loader = new TestApplicationLoader;
  application_manager_->SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(loader),
      content_handler_url_);

  bool called = false;
  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->set_source(Identity(requestor_url_));
  params->SetTargetURL(GURL("test:test"));
  params->set_on_application_end(
      base::Bind(&QuitClosure, base::Unretained(&called)));
  application_manager_->ConnectToApplication(std::move(params));
  loop_.Run();
  EXPECT_TRUE(called);

  ASSERT_EQ(1, loader->num_loads());
  EXPECT_EQ(requestor_url_, loader->last_requestor_url());
}

TEST_F(ContentHandlerTest,
       MultipleConnectionsToContentHandlerGetSameContentHandlerId) {
  TestApplicationLoader* content_handler_loader = new TestApplicationLoader;
  application_manager_->SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(content_handler_loader),
      content_handler_url_);

  uint32_t content_handler_id;
  {
    base::RunLoop run_loop;
    scoped_ptr<ConnectToApplicationParams> params(
        new ConnectToApplicationParams);
    params->set_source(Identity(requestor_url_));
    params->SetTargetURL(GURL("test:test"));
    params->set_connect_callback(
        [&content_handler_id, &run_loop](uint32_t, uint32_t t) {
      content_handler_id = t;
      run_loop.Quit();
    });
    application_manager_->ConnectToApplication(std::move(params));
    run_loop.Run();
    EXPECT_NE(Shell::kInvalidApplicationID, content_handler_id);
  }

  uint32_t content_handler_id2;
  {
    base::RunLoop run_loop;
    scoped_ptr<ConnectToApplicationParams> params(
        new ConnectToApplicationParams);
    params->set_source(Identity(requestor_url_));
    params->SetTargetURL(GURL("test:test"));
    params->set_connect_callback(
        [&content_handler_id2, &run_loop](uint32_t, uint32_t t) {
      content_handler_id2 = t;
      run_loop.Quit();
    });
    application_manager_->ConnectToApplication(std::move(params));
    run_loop.Run();
    EXPECT_NE(Shell::kInvalidApplicationID, content_handler_id2);
  }
  EXPECT_EQ(content_handler_id, content_handler_id2);
}

TEST_F(ContentHandlerTest, DifferedContentHandlersGetDifferentIDs) {
  TestApplicationLoader* content_handler_loader = new TestApplicationLoader;
  application_manager_->SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(content_handler_loader),
      content_handler_url_);

  uint32_t content_handler_id;
  {
    base::RunLoop run_loop;
    scoped_ptr<ConnectToApplicationParams> params(
        new ConnectToApplicationParams);
    params->set_source(Identity(requestor_url_));
    params->SetTargetURL(GURL("test:test"));
    params->set_connect_callback(
        [&content_handler_id, &run_loop](uint32_t, uint32_t t) {
      content_handler_id = t;
      run_loop.Quit();
    });
    application_manager_->ConnectToApplication(std::move(params));
    run_loop.Run();
    EXPECT_NE(Shell::kInvalidApplicationID, content_handler_id);
  }

  const std::string mime_type2 = "test/mime-type2";
  const GURL content_handler_url2("http://test.content.handler2");
  test_package_manager_->set_mime_type(mime_type2);
  test_package_manager_->RegisterContentHandler(mime_type2,
                                                content_handler_url2);

  TestApplicationLoader* content_handler_loader2 = new TestApplicationLoader;
  application_manager_->SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(content_handler_loader2),
      content_handler_url2);

  uint32_t content_handler_id2;
  {
    base::RunLoop run_loop;
    scoped_ptr<ConnectToApplicationParams> params(
        new ConnectToApplicationParams);
    params->set_source(Identity(requestor_url_));
    params->SetTargetURL(GURL("test2:test2"));
    params->set_connect_callback(
        [&content_handler_id2, &run_loop](uint32_t, uint32_t t) {
      content_handler_id2 = t;
      run_loop.Quit();
    });
    application_manager_->ConnectToApplication(std::move(params));
    run_loop.Run();
    EXPECT_NE(Shell::kInvalidApplicationID, content_handler_id2);
  }
  EXPECT_NE(content_handler_id, content_handler_id2);
}

TEST_F(ContentHandlerTest,
       ConnectWithNoContentHandlerGetsInvalidContentHandlerId) {
  application_manager_->SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(new TestApplicationLoader),
      GURL("test:test"));

  uint32_t content_handler_id = 1u;
  scoped_ptr<ConnectToApplicationParams> params(
      new ConnectToApplicationParams);
  params->SetTargetURL(GURL("test:test"));
  params->set_connect_callback(
      [&content_handler_id](uint32_t, uint32_t t) { content_handler_id = t; });
  application_manager_->ConnectToApplication(std::move(params));
  EXPECT_EQ(0u, content_handler_id);
}

}  // namespace test
}  // namespace package_manager
}  // namespace mojo
