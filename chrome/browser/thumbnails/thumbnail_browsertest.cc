// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace thumbnails {

namespace {

using testing::DoAll;
using testing::Field;
using testing::NiceMock;
using testing::Return;
using testing::_;

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MockThumbnailService : public ThumbnailService {
 public:
  MOCK_METHOD2(SetPageThumbnail,
               bool(const ThumbnailingContext& context,
                    const gfx::Image& thumbnail));

  MOCK_METHOD3(GetPageThumbnail,
               bool(const GURL& url,
                    bool prefix_match,
                    scoped_refptr<base::RefCountedMemory>* bytes));

  MOCK_METHOD1(AddForcedURL, void(const GURL& url));

  MOCK_METHOD2(ShouldAcquirePageThumbnail,
               bool(const GURL& url, ui::PageTransition transition));

  // Implementation of RefcountedKeyedService.
  void ShutdownOnUIThread() override {}

 protected:
  ~MockThumbnailService() override = default;
};

class ThumbnailTest : public InProcessBrowserTest {
 public:
  ThumbnailTest() {}

  MockThumbnailService* thumbnail_service() {
    return static_cast<MockThumbnailService*>(
        ThumbnailServiceFactory::GetForProfile(browser()->profile()).get());
  }

 private:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeature(
        features::kCaptureThumbnailOnNavigatingAway);

    ASSERT_TRUE(embedded_test_server()->Start());

    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(&ThumbnailTest::OnWillCreateBrowserContextServices,
                           base::Unretained(this)));
  }

  static scoped_refptr<RefcountedKeyedService> CreateThumbnailService(
      content::BrowserContext* context) {
    return scoped_refptr<RefcountedKeyedService>(
        new NiceMock<MockThumbnailService>());
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ThumbnailServiceFactory::GetInstance()->SetTestingFactory(
        context, &ThumbnailTest::CreateThumbnailService);
  }

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(ThumbnailTest, ShouldCaptureOnNavigatingAway) {
#if BUILDFLAG(ENABLE_PACKAGE_MASH_SERVICES)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kMus)) {
    LOG(ERROR) << "This test fails in MUS mode, aborting.";
    return;
  }
#endif

  const GURL about_blank_url("about:blank");
  const GURL simple_url = embedded_test_server()->GetURL("/simple.html");

  // Normally, ShouldAcquirePageThumbnail depends on many things, e.g. whether
  // the given URL is in TopSites. For the purposes of this test, bypass all
  // that and always take thumbnails for |simple_url|.
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(about_blank_url, _))
      .WillByDefault(Return(false));
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(simple_url, _))
      .WillByDefault(Return(true));

  // The test framework opens an about:blank tab by default.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(about_blank_url, active_tab->GetLastCommittedURL());

  EXPECT_CALL(
      *thumbnail_service(),
      SetPageThumbnail(Field(&ThumbnailingContext::url, about_blank_url), _))
      .Times(0);

  // Navigate to some page.
  ui_test_utils::NavigateToURL(browser(), simple_url);

  // Before navigating away, we should take a thumbnail of the page.
  base::RunLoop run_loop;
  EXPECT_CALL(*thumbnail_service(),
              SetPageThumbnail(Field(&ThumbnailingContext::url, simple_url), _))
      .WillOnce(DoAll(RunClosure(run_loop.QuitClosure()), Return(true)));
  // Navigate to about:blank again.
  ui_test_utils::NavigateToURL(browser(), about_blank_url);
  // Wait for the thumbnailing process to finish.
  run_loop.Run();
}

}  // namespace

}  // namespace thumbnails
