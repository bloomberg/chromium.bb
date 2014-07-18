// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/viewer.h"

#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_test_util.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

const char kTestScheme[] = "myscheme";

class FakeViewRequestDelegate : public ViewRequestDelegate {
 public:
  virtual ~FakeViewRequestDelegate() {}
  MOCK_METHOD1(OnArticleReady, void(const DistilledArticleProto* proto));
  MOCK_METHOD1(OnArticleUpdated,
               void(ArticleDistillationUpdate article_update));
};

class TestDomDistillerService : public DomDistillerServiceInterface {
 public:
  TestDomDistillerService() {}
  virtual ~TestDomDistillerService() {}

  MOCK_CONST_METHOD0(GetSyncableService, syncer::SyncableService*());
  MOCK_METHOD3(AddToList,
               const std::string(const GURL&,
                                 DistillerPage*,
                                 const ArticleAvailableCallback&));
  virtual const std::string AddToList(
      const GURL& url,
      scoped_ptr<DistillerPage> distiller_page,
      const ArticleAvailableCallback& article_cb) {
    return AddToList(url, distiller_page.get(), article_cb);
  }
  MOCK_CONST_METHOD0(GetEntries, std::vector<ArticleEntry>());
  MOCK_METHOD1(AddObserver, void(DomDistillerObserver*));
  MOCK_METHOD1(RemoveObserver, void(DomDistillerObserver*));
  MOCK_METHOD0(ViewUrlImpl, ViewerHandle*());
  virtual scoped_ptr<ViewerHandle> ViewUrl(
      ViewRequestDelegate*,
      scoped_ptr<DistillerPage> distiller_page,
      const GURL&) {
    return scoped_ptr<ViewerHandle>(ViewUrlImpl());
  }
  MOCK_METHOD0(ViewEntryImpl, ViewerHandle*());
  virtual scoped_ptr<ViewerHandle> ViewEntry(
      ViewRequestDelegate*,
      scoped_ptr<DistillerPage> distiller_page,
      const std::string&) {
    return scoped_ptr<ViewerHandle>(ViewEntryImpl());
  }
  MOCK_METHOD0(RemoveEntryImpl, ArticleEntry*());
  virtual scoped_ptr<ArticleEntry> RemoveEntry(const std::string&) {
    return scoped_ptr<ArticleEntry>(RemoveEntryImpl());
  }
  virtual scoped_ptr<DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) {
    return scoped_ptr<DistillerPage>();
  }
  virtual scoped_ptr<DistillerPage> CreateDefaultDistillerPageWithHandle(
      scoped_ptr<SourcePageHandle> handle) {
    return scoped_ptr<DistillerPage>();
  }
  virtual DistilledPagePrefs* GetDistilledPagePrefs() OVERRIDE;
};

class DomDistillerViewerTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    service_.reset(new TestDomDistillerService());
  }

 protected:
  scoped_ptr<ViewerHandle> CreateViewRequest(
      const std::string& path,
      ViewRequestDelegate* view_request_delegate) {
    return viewer::CreateViewRequest(
        service_.get(), path, view_request_delegate, gfx::Size());
  }

  scoped_ptr<TestDomDistillerService> service_;
};

TEST_F(DomDistillerViewerTest, TestCreatingViewUrlRequest) {
  scoped_ptr<FakeViewRequestDelegate> view_request_delegate(
      new FakeViewRequestDelegate());
  ViewerHandle* viewer_handle(new ViewerHandle(ViewerHandle::CancelCallback()));
  EXPECT_CALL(*service_.get(), ViewUrlImpl())
      .WillOnce(testing::Return(viewer_handle));
  EXPECT_CALL(*service_.get(), ViewEntryImpl()).Times(0);
  CreateViewRequest(
      std::string("?") + kUrlKey + "=http%3A%2F%2Fwww.example.com%2F",
      view_request_delegate.get());
}

TEST_F(DomDistillerViewerTest, TestCreatingViewEntryRequest) {
  scoped_ptr<FakeViewRequestDelegate> view_request_delegate(
      new FakeViewRequestDelegate());
  ViewerHandle* viewer_handle(new ViewerHandle(ViewerHandle::CancelCallback()));
  EXPECT_CALL(*service_.get(), ViewEntryImpl())
      .WillOnce(testing::Return(viewer_handle));
  EXPECT_CALL(*service_.get(), ViewUrlImpl()).Times(0);
  CreateViewRequest(std::string("?") + kEntryIdKey + "=abc-def",
                    view_request_delegate.get());
}

TEST_F(DomDistillerViewerTest, TestCreatingInvalidViewRequest) {
  scoped_ptr<FakeViewRequestDelegate> view_request_delegate(
      new FakeViewRequestDelegate());
  EXPECT_CALL(*service_.get(), ViewEntryImpl()).Times(0);
  EXPECT_CALL(*service_.get(), ViewUrlImpl()).Times(0);
  // Specify none of the required query parameters.
  CreateViewRequest("?foo=bar", view_request_delegate.get());
  // Specify both of the required query parameters.
  CreateViewRequest("?" + std::string(kUrlKey) +
                        "=http%3A%2F%2Fwww.example.com%2F&" +
                        std::string(kEntryIdKey) + "=abc-def",
                    view_request_delegate.get());
  // Specify an internal Chrome page.
  CreateViewRequest("?" + std::string(kUrlKey) + "=chrome%3A%2F%2Fsettings%2F",
                    view_request_delegate.get());
  // Specify a recursive URL.
  CreateViewRequest("?" + std::string(kUrlKey) + "=" +
                        std::string(kTestScheme) + "%3A%2F%2Fabc-def%2F",
                    view_request_delegate.get());
}

DistilledPagePrefs* TestDomDistillerService::GetDistilledPagePrefs() {
  return NULL;
}

TEST_F(DomDistillerViewerTest, TestGetDistilledPageThemeJsOutput) {
  std::string kDarkJs = "useTheme('dark');";
  std::string kSepiaJs = "useTheme('sepia');";
  std::string kLightJs = "useTheme('light');";
  EXPECT_EQ(kDarkJs.compare(viewer::GetDistilledPageThemeJs(
                DistilledPagePrefs::DARK)),
            0);
  EXPECT_EQ(kLightJs.compare(viewer::GetDistilledPageThemeJs(
                DistilledPagePrefs::LIGHT)),
            0);
  EXPECT_EQ(kSepiaJs.compare(viewer::GetDistilledPageThemeJs(
                DistilledPagePrefs::SEPIA)),
            0);
}

}  // namespace dom_distiller
