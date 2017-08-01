// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/ios_payment_instrument_finder.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ios/chrome/browser/payments/ios_payment_instrument.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class TestIOSPaymentInstrumentFinder : public IOSPaymentInstrumentFinder {
 public:
  TestIOSPaymentInstrumentFinder(
      net::TestURLRequestContextGetter* context_getter)
      : IOSPaymentInstrumentFinder(context_getter, nil) {}

  std::vector<GURL> FilterUnsupportedURLPaymentMethods(
      const std::vector<GURL>& queried_url_payment_method_identifiers)
      override {
    return queried_url_payment_method_identifiers;
  }

  DISALLOW_COPY_AND_ASSIGN(TestIOSPaymentInstrumentFinder);
};

class IOSPaymentInstrumentFinderTest : public testing::Test {
 public:
  IOSPaymentInstrumentFinderTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        context_getter_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        ios_payment_instrument_finder_(
            base::MakeUnique<TestIOSPaymentInstrumentFinder>(
                context_getter_.get())) {}

  ~IOSPaymentInstrumentFinderTest() override {}

  size_t num_payment_methods_remaining() {
    return ios_payment_instrument_finder_->num_payment_methods_remaining_;
  }

  const std::vector<std::unique_ptr<IOSPaymentInstrument>>& result() {
    return result_;
  }

  void ExpectUnableToParsePaymentMethodManifest(const std::string& input) {
    GURL actual_web_app_url;

    bool success =
        ios_payment_instrument_finder_->GetWebAppManifestURLFromPaymentManifest(
            input, &actual_web_app_url);

    EXPECT_FALSE(success);
    EXPECT_TRUE(actual_web_app_url.is_empty());
  }

  void ExpectParsedPaymentMethodManifest(const std::string& input,
                                         const GURL& expected_web_app_url) {
    GURL actual_web_app_url;

    bool success =
        ios_payment_instrument_finder_->GetWebAppManifestURLFromPaymentManifest(
            input, &actual_web_app_url);

    EXPECT_TRUE(success);
    EXPECT_EQ(expected_web_app_url, actual_web_app_url);
  }

  void ExpectUnableToParseWebAppManifest(const std::string& input) {
    std::string actual_app_name;
    GURL actual_app_icon;
    GURL actual_universal_link;

    bool success =
        ios_payment_instrument_finder_->GetPaymentAppDetailsFromWebAppManifest(
            input, GURL("https://bobpay.xyz/"), &actual_app_name,
            &actual_app_icon, &actual_universal_link);

    EXPECT_FALSE(success);
    EXPECT_TRUE(actual_app_name.empty() || actual_app_icon.is_empty() ||
                actual_universal_link.is_empty());
  }

  void ExpectParsedWebAppManifest(const std::string& input,
                                  const std::string& expected_app_name,
                                  const GURL& expected_app_icon,
                                  const GURL& expected_universal_link) {
    std::string actual_app_name;
    GURL actual_app_icon;
    GURL actual_universal_link;

    bool success =
        ios_payment_instrument_finder_->GetPaymentAppDetailsFromWebAppManifest(
            input, GURL("https://bobpay.xyz/"), &actual_app_name,
            &actual_app_icon, &actual_universal_link);

    EXPECT_TRUE(success);
    EXPECT_EQ(expected_app_name, actual_app_name);
    EXPECT_EQ(expected_app_icon, actual_app_icon);
    EXPECT_EQ(expected_universal_link, actual_universal_link);
  }

  // A callback method for testing if and when the iOS payment instrument
  // finder finishes searching for native third party payment apps.
  void InstrumentsFoundCallback(
      std::vector<std::unique_ptr<IOSPaymentInstrument>> result) {
    result_ = std::move(result);
    if (run_loop_)
      run_loop_->Quit();
  }

  void FindInstrumentsWithMethods(std::vector<GURL>& url_methods) {
    ios_payment_instrument_finder_->CreateIOSPaymentInstrumentsForMethods(
        url_methods,
        base::BindOnce(
            &IOSPaymentInstrumentFinderTest::InstrumentsFoundCallback,
            base::Unretained(this)));
  }

  void FindInstrumentsWithWebAppManifest(const GURL& method,
                                         const std::string& content) {
    ios_payment_instrument_finder_->callback_ = base::BindOnce(
        &IOSPaymentInstrumentFinderTest::InstrumentsFoundCallback,
        base::Unretained(this));
    ios_payment_instrument_finder_->num_payment_methods_remaining_ = 1;
    ios_payment_instrument_finder_->OnWebAppManifestDownloaded(
        method, GURL("https://bobpay.xyz/"), content);
  }

  void RunLoop() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  scoped_refptr<net::TestURLRequestContextGetter> context_getter_;
  std::unique_ptr<TestIOSPaymentInstrumentFinder>
      ios_payment_instrument_finder_;

  std::unique_ptr<base::RunLoop> run_loop_;
  std::vector<std::unique_ptr<IOSPaymentInstrument>> result_;

  DISALLOW_COPY_AND_ASSIGN(IOSPaymentInstrumentFinderTest);
};

// Payment method manifest parsing:

TEST_F(IOSPaymentInstrumentFinderTest, NullPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest(std::string());
}

TEST_F(IOSPaymentInstrumentFinderTest,
       NonJsonPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("this is not json");
}

TEST_F(IOSPaymentInstrumentFinderTest, StringPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("\"this is a string\"");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       EmptyDictionaryPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{}");
}

TEST_F(IOSPaymentInstrumentFinderTest, NullDefaultApplicationIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": null}");
}

TEST_F(IOSPaymentInstrumentFinderTest, NumberDefaultApplicationIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": 0}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       ListOfNumbersDefaultApplicationIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": [0]}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       EmptyListOfDefaultApplicationsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": []}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       ListOfEmptyDefaultApplicationsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": [\"\"]}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       DefaultApplicationsShouldNotHaveNulCharacters) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": [\"https://bobpay.com/app\0json\"]}");
}

TEST_F(IOSPaymentInstrumentFinderTest, DefaultApplicationKeyShouldBeLowercase) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"Default_Applications\": [\"https://bobpay.com/app.json\"]}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       DefaultApplicationsShouldHaveAbsoluteUrl) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": ["
      "\"app.json\"]}");
}

TEST_F(IOSPaymentInstrumentFinderTest, DefaultApplicationsShouldBeHttps) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": ["
      "\"http://bobpay.com/app.json\","
      "\"http://alicepay.com/app.json\"]}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       WellFormedPaymentMethodManifestWithApps) {
  ExpectParsedPaymentMethodManifest(
      "{\"default_applications\": ["
      "\"https://bobpay.com/app.json\","
      "\"https://alicepay.com/app.json\"]}",
      GURL("https://bobpay.com/app.json"));
}

// Web app manifest parsing:

TEST_F(IOSPaymentInstrumentFinderTest, NullContentIsMalformed) {
  ExpectUnableToParseWebAppManifest(std::string());
}

TEST_F(IOSPaymentInstrumentFinderTest, NonJsonContentIsMalformed) {
  ExpectUnableToParseWebAppManifest("this is not json");
}

TEST_F(IOSPaymentInstrumentFinderTest, StringContentIsMalformed) {
  ExpectUnableToParseWebAppManifest("\"this is a string\"");
}

TEST_F(IOSPaymentInstrumentFinderTest, EmptyDictionaryIsMalformed) {
  ExpectUnableToParseWebAppManifest("{}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       NullRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": null");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       NumberRelatedApplicationSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": 0"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       ListOfNumbersRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [0]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       EmptyListRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": []"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       ListOfEmptyDictionariesRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{}]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest, NoItunesPlatformIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest, NoUniversalLinkIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\""
      "  }]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest, NoShortNameIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\""
      "  }]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest, PlatformShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"it\0unes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest,
       UniversalLinkShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobp\0ay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest, IconSourceShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/to\0uch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest, ShortNameShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bob\0pay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest, KeysShouldBeLowerCase) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"Short_name\": \"Bobpay\", "
      "  \"Icons\": [{"
      "    \"Src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"Related_applications\": [{"
      "    \"Platform\": \"itunes\", "
      "    \"Url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest, UniversalLinkShouldHaveAbsoluteUrl) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"pay.html\""
      "  }]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest, UniversalLinkShouldBeHttps) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"http://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(IOSPaymentInstrumentFinderTest, WellFormed) {
  ExpectParsedWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}",
      "Bobpay", GURL("https://bobpay.xyz/images/touch/homescreen48.png"),
      GURL("https://bobpay.xyz/pay"));
}

TEST_F(IOSPaymentInstrumentFinderTest, RelativeIconPathWellFormed) {
  ExpectParsedWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}",
      "Bobpay", GURL("https://bobpay.xyz/images/touch/homescreen48.png"),
      GURL("https://bobpay.xyz/pay"));
}

TEST_F(IOSPaymentInstrumentFinderTest,
       TwoRelatedApplicationsSecondIsWellFormed) {
  ExpectParsedWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }, {"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}",
      "Bobpay", GURL("https://bobpay.xyz/images/touch/homescreen48.png"),
      GURL("https://bobpay.xyz/pay"));
}

// Tests that supplying no methods to the IOSPaymentInstrumentFinder class still
// invokes the caller's callback function and that the list of returned
// instruments is empty.
TEST_F(IOSPaymentInstrumentFinderTest, NoMethodsSuppliedNoInstruments) {
  std::vector<GURL> url_methods;

  FindInstrumentsWithMethods(url_methods);

  EXPECT_EQ(0u, num_payment_methods_remaining());
  EXPECT_EQ(0u, result().size());
}

// Tests that supplying many invalid methods to the IOSPaymentInstrumentFinder
// class still invokes the caller's callback function and that the list of
// returned instruments is empty.
TEST_F(IOSPaymentInstrumentFinderTest,
       ManyInvalidMethodsSuppliedNoInstruments) {
  std::vector<GURL> url_methods;
  url_methods.push_back(GURL("https://fake-host-name/bobpay"));
  url_methods.push_back(GURL("https://fake-host-name/alicepay"));
  url_methods.push_back(GURL("https://fake-host-name/sampay"));

  FindInstrumentsWithMethods(url_methods);
  RunLoop();

  EXPECT_EQ(0u, num_payment_methods_remaining());
  EXPECT_EQ(0u, result().size());
}

// Tests that supplying one valid method with a corresponding complete web
// app manifest will result in one created IOSPaymentInstrument that is returned
// to the caller.
TEST_F(IOSPaymentInstrumentFinderTest, OneValidMethodSuppliedOneInstrument) {
  FindInstrumentsWithWebAppManifest(
      GURL("https://emerald-eon.appspot.com/bobpay"),
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/images/touch/homescreen48.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }, {"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
  RunLoop();

  EXPECT_EQ(0u, num_payment_methods_remaining());
  EXPECT_EQ(1u, result().size());
  EXPECT_EQ("Bobpay", base::UTF16ToASCII(result()[0]->GetLabel()));
  EXPECT_EQ("emerald-eon.appspot.com",
            base::UTF16ToASCII(result()[0]->GetSublabel()));
}

}  // namespace payments
