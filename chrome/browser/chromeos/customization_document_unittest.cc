// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_document.h"

#include "base/message_loop/message_loop.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/system/mock_statistics_provider.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::_;

namespace {

const char kGoodStartupManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"initial_locale\" : \"en-US\","
    "  \"initial_timezone\" : \"US/Pacific\","
    "  \"keyboard_layout\" : \"xkb:us::eng\","
    "  \"setup_content\" : {"
    "    \"en-US\" : {"
    "      \"eula_page\" : \"file:///opt/oem/eula/en-US/eula.html\","
    "    },"
    "    \"ru-RU\" : {"
    "      \"eula_page\" : \"file:///opt/oem/eula/ru-RU/eula.html\","
    "    },"
    "    \"default\" : {"
    "      \"eula_page\" : \"file:///opt/oem/eula/en/eula.html\","
    "    },"
    "  },"
    "  \"hwid_map\" : ["
    "    {"
    "      \"hwid_mask\": \"ZGA*34\","
    "      \"initial_locale\" : \"ja\","
    "      \"initial_timezone\" : \"Asia/Tokyo\","
    "      \"keyboard_layout\" : \"mozc-jp\","
    "    },"
    "    {"
    "      \"hwid_mask\": \"Mario 1?3*\","
    "      \"initial_locale\" : \"ru-RU\","
    "      \"initial_timezone\" : \"Europe/Moscow\","
    "      \"keyboard_layout\" : \"xkb:ru::rus\","
    "    },"
    "  ],"
    "}";

const char kBadManifest[] = "{\"version\": \"1\"}";

const char kGoodServicesManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"default_wallpaper\": \"http://somedomain.com/image.png\",\n"
    "  \"default_apps\": [\n"
    "    \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\n"
    "    \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"\n"
    "  ]\n"
    "}";

}  // anonymous namespace

namespace chromeos {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;

TEST(StartupCustomizationDocumentTest, Basic) {
  system::MockStatisticsProvider mock_statistics_provider;
  EXPECT_CALL(mock_statistics_provider, GetMachineStatistic(_, NotNull()))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_statistics_provider,
      GetMachineStatistic(std::string("hardware_class"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("Mario 12345")),
                          Return(true)));
  StartupCustomizationDocument customization(&mock_statistics_provider,
                                             kGoodStartupManifest);
  EXPECT_EQ("ru-RU", customization.initial_locale());
  EXPECT_EQ("Europe/Moscow", customization.initial_timezone());
  EXPECT_EQ("xkb:ru::rus", customization.keyboard_layout());

  EXPECT_EQ("file:///opt/oem/eula/en-US/eula.html",
            customization.GetEULAPage("en-US"));
  EXPECT_EQ("file:///opt/oem/eula/ru-RU/eula.html",
            customization.GetEULAPage("ru-RU"));
  EXPECT_EQ("file:///opt/oem/eula/en/eula.html",
            customization.GetEULAPage("ja"));
}

TEST(StartupCustomizationDocumentTest, VPD) {
  system::MockStatisticsProvider mock_statistics_provider;
  EXPECT_CALL(mock_statistics_provider,
      GetMachineStatistic(std::string("hardware_class"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("Mario 12345")),
                          Return(true)));
  EXPECT_CALL(mock_statistics_provider,
      GetMachineStatistic(std::string("initial_locale"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("ja")),
                          Return(true)));
  EXPECT_CALL(mock_statistics_provider,
      GetMachineStatistic(std::string("initial_timezone"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("Asia/Tokyo")),
                          Return(true)));
  EXPECT_CALL(mock_statistics_provider,
      GetMachineStatistic(std::string("keyboard_layout"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("mozc-jp")),
                          Return(true)));
  StartupCustomizationDocument customization(&mock_statistics_provider,
                                             kGoodStartupManifest);
  EXPECT_TRUE(customization.IsReady());
  EXPECT_EQ("ja", customization.initial_locale());
  EXPECT_EQ("Asia/Tokyo", customization.initial_timezone());
  EXPECT_EQ("mozc-jp", customization.keyboard_layout());
}

TEST(StartupCustomizationDocumentTest, BadManifest) {
  system::MockStatisticsProvider mock_statistics_provider;
  StartupCustomizationDocument customization(&mock_statistics_provider,
                                             kBadManifest);
  EXPECT_FALSE(customization.IsReady());
}

class TestURLFetcherCallback {
 public:
  scoped_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* d,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    scoped_ptr<net::FakeURLFetcher> fetcher(
        new net::FakeURLFetcher(url, d, response_data, response_code, status));
    OnRequestCreate(url, fetcher.get());
    return fetcher.Pass();
  }
  MOCK_METHOD2(OnRequestCreate,
               void(const GURL&, net::FakeURLFetcher*));
};

void AddMimeHeader(const GURL& url, net::FakeURLFetcher* fetcher) {
  scoped_refptr<net::HttpResponseHeaders> download_headers =
      new net::HttpResponseHeaders("");
  download_headers->AddHeader("Content-Type: application/json");
  fetcher->set_response_headers(download_headers);
}

class ServicesCustomizationDocumentTest : public testing::Test {
 protected:
  ServicesCustomizationDocumentTest()
    : factory_(NULL,
               base::Bind(&TestURLFetcherCallback::CreateURLFetcher,
               base::Unretained(&url_callback_))) {
  }

  // testing::Test:
  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(mock_statistics_provider_, GetMachineStatistic(_, NotNull()))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(mock_statistics_provider_,
        GetMachineStatistic(std::string("customization_id"), NotNull()))
            .WillOnce(DoAll(SetArgumentPointee<1>(std::string("test-dummy")),
                            Return(true)));
    chromeos::system::StatisticsProvider::SetTestProvider(
        &mock_statistics_provider_);

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    ServicesCustomizationDocument::RegisterPrefs(local_state_.registry());

    GURL url("http://localhost:1080/test-dummy.json");
    factory_.SetFakeResponse(url,
                             kGoodServicesManifest,
                             net::HTTP_OK,
                             net::URLRequestStatus::SUCCESS);
    EXPECT_CALL(url_callback_, OnRequestCreate(url, _))
      .Times(Exactly(1))
      .WillRepeatedly(Invoke(AddMimeHeader));
  }

  virtual void TearDown() OVERRIDE {
    TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
    chromeos::system::StatisticsProvider::SetTestProvider(NULL);
  }

  void RunUntilIdle() {
    message_loop_.RunUntilIdle();
  }

 private:
  system::MockStatisticsProvider mock_statistics_provider_;
  base::MessageLoop message_loop_;
  TestingPrefServiceSimple local_state_;
  TestURLFetcherCallback url_callback_;
  net::FakeURLFetcherFactory factory_;
};

TEST_F(ServicesCustomizationDocumentTest, Basic) {
  ServicesCustomizationDocument* doc =
      ServicesCustomizationDocument::GetInstance();
  EXPECT_FALSE(doc->IsReady());

  doc->StartFetching();
  RunUntilIdle();
  EXPECT_TRUE(doc->IsReady());

  EXPECT_EQ(doc->GetDefaultWallpaperUrl().spec(),
            "http://somedomain.com/image.png");

  std::vector<std::string> default_apps;
  EXPECT_TRUE(doc->GetDefaultApps(&default_apps));
  ASSERT_EQ(default_apps.size(), 2u);

  EXPECT_EQ(default_apps[0], "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  EXPECT_EQ(default_apps[1], "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
}

}  // namespace chromeos
