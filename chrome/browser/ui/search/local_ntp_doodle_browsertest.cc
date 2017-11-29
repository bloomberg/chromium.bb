// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/string_escape.h"
#include "base/memory/ref_counted_memory.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_provider_logos/logo_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using search_provider_logos::EncodedLogo;
using search_provider_logos::EncodedLogoCallback;
using search_provider_logos::LogoCallbacks;
using search_provider_logos::LogoCallbackReason;
using search_provider_logos::LogoObserver;
using search_provider_logos::LogoService;
using search_provider_logos::LogoType;
using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::IsEmpty;

namespace {

const char kCachedB64[] = "\161\247\041\171\337\276";  // b64decode("cached++")
const char kFreshB64[] = "\176\267\254\207\357\276";   // b64decode("fresh+++")

scoped_refptr<base::RefCountedString> MakeRefPtr(std::string content) {
  return base::RefCountedString::TakeString(&content);
}

class MockLogoService : public LogoService {
 public:
  MOCK_METHOD1(GetLogoPtr, void(LogoCallbacks* callbacks));

  void GetLogo(LogoCallbacks callbacks) override { GetLogoPtr(&callbacks); }
  void GetLogo(LogoObserver* observer) override { NOTREACHED(); }
};

ACTION_P2(ReturnCachedLogo, reason, logo) {
  if (arg0->on_cached_encoded_logo_available) {
    std::move(arg0->on_cached_encoded_logo_available).Run(reason, logo);
  }
}

ACTION_P2(ReturnFreshLogo, reason, logo) {
  if (arg0->on_fresh_encoded_logo_available) {
    std::move(arg0->on_fresh_encoded_logo_available).Run(reason, logo);
  }
}

}  // namespace

class LocalNTPDoodleTest : public InProcessBrowserTest {
 protected:
  LocalNTPDoodleTest() {}

  MockLogoService* logo_service() {
    return static_cast<MockLogoService*>(
        LogoServiceFactory::GetForProfile(browser()->profile()));
  }

  base::Optional<std::string> GetComputedStyle(content::WebContents* tab,
                                               const std::string& id,
                                               const std::string& css_name) {
    std::string css_value;
    if (instant_test_utils::GetStringFromJS(
            tab,
            base::StringPrintf(
                "getComputedStyle(document.getElementById(%s))[%s]",
                base::GetQuotedJSONString(id).c_str(),
                base::GetQuotedJSONString(css_name).c_str()),
            &css_value)) {
      return css_value;
    }
    return base::nullopt;
  }

  base::Optional<double> GetComputedOpacity(content::WebContents* tab,
                                            const std::string& id) {
    auto css_value = GetComputedStyle(tab, id, "opacity");
    double double_value;
    if ((css_value != base::nullopt) &&
        base::StringToDouble(*css_value, &double_value)) {
      return double_value;
    }
    return base::nullopt;
  }

  base::Optional<std::string> GetComputedDisplay(content::WebContents* tab,
                                                 const std::string& id) {
    return GetComputedStyle(tab, id, "display");
  }

  // Gets $(id)[property]. Coerces to string.
  base::Optional<std::string> GetElementProperty(content::WebContents* tab,
                                                 const std::string& id,
                                                 const std::string& property) {
    std::string value;
    if (instant_test_utils::GetStringFromJS(
            tab,
            base::StringPrintf("document.getElementById(%s)[%s] + ''",
                               base::GetQuotedJSONString(id).c_str(),
                               base::GetQuotedJSONString(property).c_str()),
            &value)) {
      return value;
    }
    return base::nullopt;
  }

  void WaitForFadeIn(content::WebContents* tab, const std::string& id) {
    content::ConsoleObserverDelegate console_observer(tab, "WaitForFadeIn");
    tab->SetDelegate(&console_observer);

    bool result = false;
    if (!instant_test_utils::GetBoolFromJS(
            tab,
            base::StringPrintf(
                R"js(
                  (function(id, message) {
                    var element = document.getElementById(id);
                    var fn = function() {
                      if (element.classList.contains('show-logo') &&
                          (window.getComputedStyle(element).opacity == 1.0)) {
                        console.log(message);
                      } else {
                        element.addEventListener('transitionend', fn);
                      }
                    };
                    fn();
                    return true;
                  })(%s, 'WaitForFadeIn')
                )js",
                base::GetQuotedJSONString(id).c_str()),
            &result) &&
        result) {
      ADD_FAILURE() << "failed to wait for fade-in";
      return;
    }

    console_observer.Wait();
  }

  // See enum LogoImpressionType in ntp_user_data_logger.cc.
  static const int kLogoImpressionStatic = 0;
  static const int kLogoImpressionCta = 1;

  // See enum LogoClickType in ntp_user_data_logger.cc.
  static const int kLogoClickCta = 1;

 private:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kUseGoogleLocalNtp, features::kDoodlesOnLocalNtp}, {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(
                    &LocalNTPDoodleTest::OnWillCreateBrowserContextServices,
                    base::Unretained(this)));
  }

  static std::unique_ptr<KeyedService> CreateLogoService(
      content::BrowserContext* context) {
    return base::MakeUnique<MockLogoService>();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    LogoServiceFactory::GetInstance()->SetTestingFactory(
        context, &LocalNTPDoodleTest::CreateLogoService);
  }

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldBeUnchangedOnLogoFetchCancelled) {
  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(
          DoAll(ReturnCachedLogo(LogoCallbackReason::CANCELED, base::nullopt),
                ReturnFreshLogo(LogoCallbackReason::CANCELED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(1.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(0.0));
  EXPECT_THAT(console_observer.message(), IsEmpty());

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 0);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldBeUnchangedWhenNoCachedOrFreshDoodle) {
  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(1.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(0.0));
  EXPECT_THAT(console_observer.message(), IsEmpty());

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 0);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldShowDoodleWhenCached) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-button"),
              Eq<std::string>("block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("Chromium"));
  // TODO(sfiera): check href by clicking on button.
  EXPECT_THAT(console_observer.message(), IsEmpty());

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldShowInteractiveLogo) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(std::string());
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.type = LogoType::INTERACTIVE;
  cached_logo.metadata.full_page_url =
      GURL("https://www.chromium.org/interactive");
  cached_logo.metadata.alt_text = "alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-button"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("block"));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-iframe", "src"),
              Eq<std::string>("https://www.chromium.org/interactive"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-iframe", "title"),
              Eq<std::string>("alt text"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeSimpleDoodleToDefaultWhenFetched) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, base::nullopt)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-default");
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(1.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(0.0));

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeDefaultToSimpleDoodleWhenFetched) {
  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = MakeRefPtr(kFreshB64);
  fresh_logo.metadata.mime_type = "image/png";
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  fresh_logo.metadata.alt_text = "Chromium";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-doodle");
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-button"),
              Eq<std::string>("block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("Chromium"));
  // TODO(sfiera): check href by clicking on button.

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.Fresh",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeDefaultToInteractiveDoodleWhenFetched) {
  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = MakeRefPtr(std::string());
  fresh_logo.metadata.mime_type = "image/png";
  fresh_logo.metadata.type = LogoType::INTERACTIVE;
  fresh_logo.metadata.full_page_url =
      GURL("https://www.chromium.org/interactive");
  fresh_logo.metadata.alt_text = "alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-doodle");
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-button"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("block"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-iframe", "src"),
              Eq<std::string>("https://www.chromium.org/interactive"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldNotFadeFromInteractiveDoodle) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(std::string());
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.type = LogoType::INTERACTIVE;
  cached_logo.metadata.full_page_url =
      GURL("https://www.chromium.org/interactive");
  cached_logo.metadata.alt_text = "alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, base::nullopt)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-button"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("block"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-iframe", "src"),
              Eq<std::string>("https://www.chromium.org/interactive"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeSimpleDoodleToSimpleDoodleWhenFetched) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/cached");
  cached_logo.metadata.alt_text = "cached alt text";

  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = MakeRefPtr(kFreshB64);
  fresh_logo.metadata.mime_type = "image/png";
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/fresh");
  fresh_logo.metadata.alt_text = "fresh alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-doodle");
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-button"),
              Eq<std::string>("block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq<std::string>("data:image/png;base64,fresh+++"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("fresh alt text"));
  // TODO(sfiera): check href by clicking on button.

  // LogoShown is recorded for both cached and fresh Doodle, but LogoShownTime2
  // is only recorded once per NTP.
  histograms.ExpectTotalCount("NewTabPage.LogoShown", 2);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               2);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.Fresh",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldUpdateMetadataWhenChanged) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/cached");
  cached_logo.metadata.alt_text = "cached alt text";

  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = cached_logo.encoded_image;
  fresh_logo.metadata.mime_type = cached_logo.metadata.mime_type;
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/fresh");
  fresh_logo.metadata.alt_text = "fresh alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-button"),
              Eq<std::string>("block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("fresh alt text"));
  // TODO(sfiera): check href by clicking on button.

  // Metadata update does not count as a new impression.
  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldAnimateLogoWhenClicked) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.type = LogoType::ANIMATED;
  cached_logo.metadata.animated_url = GURL("data:image/png;base64,cached++");
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-button"),
              Eq<std::string>("block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq<std::string>("data:image/png;base64,cached++"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("alt text"));

  // Click image, swapping out for animated URL.
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('logo-doodle-button').click();"));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq(cached_logo.metadata.animated_url.spec()));
  // TODO(sfiera): check href by clicking on button.

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionCta, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionCta, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
  histograms.ExpectTotalCount("NewTabPage.LogoClick", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoClick", kLogoClickCta, 1);
}
