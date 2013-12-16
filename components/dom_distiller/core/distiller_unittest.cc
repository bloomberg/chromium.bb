// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "ui/base/android/ui_base_jni_registrar.h"
#include "ui/gfx/android/gfx_jni_registrar.h"
#endif

using::testing::Invoke;
using::testing::Return;
using::testing::_;

namespace {
  const char kTitle[] = "Title";
  const char kContent[] = "Content";
  const char kURL[] = "http://a.com/";
  const char kId0[] = "0";
  const char kId1[] = "1";
  const char kImageURL0[] = "http://a.com/img1.jpg";
  const char kImageURL1[] = "http://a.com/img2.jpg";
  const char kImageData0[] = { 'a', 'b', 'c', 'd', 'e', 0 };
  const char kImageData1[] = { '1', '2', '3', '4', '5', 0 };
}  // namespace

namespace dom_distiller {

class TestDistillerURLFetcher : public DistillerURLFetcher {
 public:
  TestDistillerURLFetcher() : DistillerURLFetcher(NULL) {
    responses_[kImageURL0] = std::string(kImageData0);
    responses_[kImageURL1] = std::string(kImageData1);
  }

  void CallCallback(std::string url, const URLFetcherCallback& callback) {
    callback.Run(responses_[url]);
  }

  virtual void FetchURL(const std::string& url,
                        const URLFetcherCallback& callback) OVERRIDE {
    ASSERT_TRUE(base::MessageLoop::current());
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TestDistillerURLFetcher::CallCallback,
                   base::Unretained(this), url, callback));
  }

  std::map<std::string, std::string> responses_;
};


class TestDistillerURLFetcherFactory : public DistillerURLFetcherFactory {
 public:
  TestDistillerURLFetcherFactory() : DistillerURLFetcherFactory(NULL) {}
  virtual ~TestDistillerURLFetcherFactory() {}
  virtual DistillerURLFetcher* CreateDistillerURLFetcher() const OVERRIDE {
    return new TestDistillerURLFetcher();
  }
};


class MockDistillerPage : public DistillerPage {
 public:
  MOCK_METHOD0(InitImpl, void());
  MOCK_METHOD1(LoadURLImpl, void(const GURL& gurl));
  MOCK_METHOD1(ExecuteJavaScriptImpl, void(const std::string& script));

  explicit MockDistillerPage(DistillerPage::Delegate* delegate)
      : DistillerPage(delegate) {}
};


class MockDistillerPageFactory : public DistillerPageFactory {
 public:
  MOCK_CONST_METHOD1(
      CreateDistillerPageMock,
      DistillerPage*(DistillerPage::Delegate* delegate));

  virtual scoped_ptr<DistillerPage> CreateDistillerPage(
      DistillerPage::Delegate* delegate) const OVERRIDE {
    return scoped_ptr<DistillerPage>(CreateDistillerPageMock(delegate));
  }
};


class DistillerTest : public testing::Test {
 public:
  virtual ~DistillerTest() {}
  void OnDistillPageDone(scoped_ptr<DistilledPageProto> proto) {
    proto_ = proto.Pass();
  }

 protected:
  scoped_ptr<DistillerImpl> distiller_;
  scoped_ptr<DistilledPageProto> proto_;
  MockDistillerPageFactory page_factory_;
  TestDistillerURLFetcherFactory url_fetcher_factory_;
};

ACTION_P2(DistillerPageOnExecuteJavaScriptDone, distiller_page, list) {
  distiller_page->OnExecuteJavaScriptDone(list);
}

ACTION_P2(CreateMockDistillerPage, list, kurl) {
  DistillerPage::Delegate* delegate = arg0;
  MockDistillerPage* distiller_page = new MockDistillerPage(delegate);
  EXPECT_CALL(*distiller_page, InitImpl());
  EXPECT_CALL(*distiller_page, LoadURLImpl(kurl))
      .WillOnce(testing::InvokeWithoutArgs(distiller_page,
                                           &DistillerPage::OnLoadURLDone));
  EXPECT_CALL(*distiller_page, ExecuteJavaScriptImpl(_))
      .WillOnce(DistillerPageOnExecuteJavaScriptDone(distiller_page, list));
  return distiller_page;
}

TEST_F(DistillerTest, DistillPage) {
#if defined(OS_ANDROID)
  // Register JNI bindings for android.
  JNIEnv* env = base::android::AttachCurrentThread();
  gfx::android::RegisterJni(env);
  ui::android::RegisterJni(env);
#endif

  ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  scoped_ptr<base::ListValue> list(new base::ListValue());
  list->AppendString(kTitle);
  list->AppendString(kContent);
  list->AppendString(kImageURL0);
  list->AppendString(kImageURL1);
  EXPECT_CALL(page_factory_,
              CreateDistillerPageMock(_)).WillOnce(
                  CreateMockDistillerPage(list.get(), GURL(kURL)));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  distiller_->DistillPage(
      GURL(kURL),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, proto_->title());
  EXPECT_EQ(kContent, proto_->html());
  EXPECT_EQ(kURL, proto_->url());
  EXPECT_EQ(2, proto_->image_size());
  EXPECT_EQ(kImageData0, proto_->image(0).data());
  EXPECT_EQ(kId0, proto_->image(0).name());
  EXPECT_EQ(kImageData1, proto_->image(1).data());
  EXPECT_EQ(kId1, proto_->image(1).name());
}

}  // namespace dom_distiller
