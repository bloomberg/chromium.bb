// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_service.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/features.h"
#include "components/search_provider_logos/fixed_logo_api.h"
#include "components/search_provider_logos/google_logo_api.h"
#include "components/search_provider_logos/logo_cache.h"
#include "components/search_provider_logos/logo_tracker.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

namespace search_provider_logos {

namespace {

bool AreImagesSameSize(const SkBitmap& bitmap1, const SkBitmap& bitmap2) {
  return bitmap1.width() == bitmap2.width() &&
         bitmap1.height() == bitmap2.height();
}

scoped_refptr<base::RefCountedString> EncodeBitmapAsPNG(
    const SkBitmap& bitmap) {
  scoped_refptr<base::RefCountedMemory> png_bytes =
      gfx::Image::CreateFrom1xBitmap(bitmap).As1xPNGBytes();
  scoped_refptr<base::RefCountedString> str = new base::RefCountedString();
  str->data().assign(png_bytes->front_as<char>(), png_bytes->size());
  return str;
}

std::string EncodeBitmapAsPNGBase64(const SkBitmap& bitmap) {
  scoped_refptr<base::RefCountedString> png_bytes = EncodeBitmapAsPNG(bitmap);
  std::string encoded_image_base64;
  base::Base64Encode(png_bytes->data(), &encoded_image_base64);
  return encoded_image_base64;
}

SkBitmap MakeBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorBLUE);
  return bitmap;
}

EncodedLogo EncodeLogo(const Logo& logo) {
  EncodedLogo encoded_logo;
  encoded_logo.encoded_image = EncodeBitmapAsPNG(logo.image);
  encoded_logo.metadata = logo.metadata;
  return encoded_logo;
}

Logo DecodeLogo(const EncodedLogo& encoded_logo) {
  Logo logo;
  logo.image =
      gfx::Image::CreateFrom1xPNGBytes(encoded_logo.encoded_image->front(),
                                       encoded_logo.encoded_image->size())
          .AsBitmap();
  logo.metadata = encoded_logo.metadata;
  return logo;
}

Logo GetSampleLogo(const GURL& logo_url, base::Time response_time) {
  Logo logo;
  logo.image = MakeBitmap(2, 5);
  logo.metadata.can_show_after_expiration = false;
  logo.metadata.expiration_time =
      response_time + base::TimeDelta::FromHours(19);
  logo.metadata.fingerprint = "8bc33a80";
  logo.metadata.source_url = logo_url;
  logo.metadata.on_click_url = GURL("http://www.google.com/search?q=potato");
  logo.metadata.alt_text = "A logo about potatoes";
  logo.metadata.animated_url = GURL("http://www.google.com/logos/doodle.png");
  logo.metadata.mime_type = "image/png";
  return logo;
}

Logo GetSampleLogo2(const GURL& logo_url, base::Time response_time) {
  Logo logo;
  logo.image = MakeBitmap(4, 3);
  logo.metadata.can_show_after_expiration = true;
  logo.metadata.expiration_time = base::Time();
  logo.metadata.fingerprint = "71082741021409127";
  logo.metadata.source_url = logo_url;
  logo.metadata.on_click_url = GURL("http://example.com/page25");
  logo.metadata.alt_text = "The logo for example.com";
  logo.metadata.mime_type = "image/jpeg";
  return logo;
}

std::string MakeServerResponse(const SkBitmap& image,
                               const std::string& on_click_url,
                               const std::string& alt_text,
                               const std::string& animated_url,
                               const std::string& mime_type,
                               const std::string& fingerprint,
                               base::TimeDelta time_to_live) {
  base::DictionaryValue dict;

  std::string data_uri = "data:";
  data_uri += mime_type;
  data_uri += ";base64,";
  data_uri += EncodeBitmapAsPNGBase64(image);

  dict.SetString("ddljson.target_url", on_click_url);
  dict.SetString("ddljson.alt_text", alt_text);
  if (animated_url.empty()) {
    dict.SetString("ddljson.doodle_type", "SIMPLE");
    if (!image.isNull())
      dict.SetString("ddljson.data_uri", data_uri);
  } else {
    dict.SetString("ddljson.doodle_type", "ANIMATED");
    dict.SetBoolean("ddljson.large_image.is_animated_gif", true);
    dict.SetString("ddljson.large_image.url", animated_url);
    if (!image.isNull())
      dict.SetString("ddljson.cta_data_uri", data_uri);
  }
  dict.SetString("ddljson.fingerprint", fingerprint);
  if (time_to_live != base::TimeDelta())
    dict.SetInteger("ddljson.time_to_live_ms",
                    static_cast<int>(time_to_live.InMilliseconds()));

  std::string output;
  base::JSONWriter::Write(dict, &output);
  return output;
}

std::string MakeServerResponse(const Logo& logo, base::TimeDelta time_to_live) {
  return MakeServerResponse(
      logo.image, logo.metadata.on_click_url.spec(), logo.metadata.alt_text,
      logo.metadata.animated_url.spec(), logo.metadata.mime_type,
      logo.metadata.fingerprint, time_to_live);
}

void ExpectLogosEqual(const Logo* expected_logo, const Logo* actual_logo) {
  if (!expected_logo) {
    ASSERT_FALSE(actual_logo);
    return;
  }
  ASSERT_TRUE(actual_logo);
  EXPECT_TRUE(AreImagesSameSize(expected_logo->image, actual_logo->image));
  EXPECT_EQ(expected_logo->metadata.on_click_url,
            actual_logo->metadata.on_click_url);
  EXPECT_EQ(expected_logo->metadata.source_url,
            actual_logo->metadata.source_url);
  EXPECT_EQ(expected_logo->metadata.animated_url,
            actual_logo->metadata.animated_url);
  EXPECT_EQ(expected_logo->metadata.alt_text, actual_logo->metadata.alt_text);
  EXPECT_EQ(expected_logo->metadata.mime_type, actual_logo->metadata.mime_type);
  EXPECT_EQ(expected_logo->metadata.fingerprint,
            actual_logo->metadata.fingerprint);
  EXPECT_EQ(expected_logo->metadata.can_show_after_expiration,
            actual_logo->metadata.can_show_after_expiration);
}

void ExpectLogosEqual(const Logo* expected_logo,
                      const EncodedLogo* actual_encoded_logo) {
  Logo actual_logo;
  if (actual_encoded_logo)
    actual_logo = DecodeLogo(*actual_encoded_logo);
  ExpectLogosEqual(expected_logo, actual_encoded_logo ? &actual_logo : nullptr);
}

ACTION_P(ExpectLogosEqualAction, expected_logo) {
  ExpectLogosEqual(expected_logo, arg0);
}

class MockLogoCache : public LogoCache {
 public:
  MockLogoCache() : LogoCache(base::FilePath()) {
    // Delegate actions to the *Internal() methods by default.
    ON_CALL(*this, UpdateCachedLogoMetadata(_))
        .WillByDefault(
            Invoke(this, &MockLogoCache::UpdateCachedLogoMetadataInternal));
    ON_CALL(*this, GetCachedLogoMetadata())
        .WillByDefault(
            Invoke(this, &MockLogoCache::GetCachedLogoMetadataInternal));
    ON_CALL(*this, SetCachedLogo(_))
        .WillByDefault(Invoke(this, &MockLogoCache::SetCachedLogoInternal));
  }

  MOCK_METHOD1(UpdateCachedLogoMetadata, void(const LogoMetadata& metadata));
  MOCK_METHOD0(GetCachedLogoMetadata, const LogoMetadata*());
  MOCK_METHOD1(SetCachedLogo, void(const EncodedLogo* logo));
  // GetCachedLogo() can't be mocked since it returns a scoped_ptr, which is
  // non-copyable. Instead create a method that's pinged when GetCachedLogo() is
  // called.
  MOCK_METHOD0(OnGetCachedLogo, void());

  void EncodeAndSetCachedLogo(const Logo& logo) {
    EncodedLogo encoded_logo = EncodeLogo(logo);
    SetCachedLogo(&encoded_logo);
  }

  void ExpectSetCachedLogo(const Logo* expected_logo) {
    Mock::VerifyAndClearExpectations(this);
    EXPECT_CALL(*this, SetCachedLogo(_))
        .WillOnce(ExpectLogosEqualAction(expected_logo));
  }

  void UpdateCachedLogoMetadataInternal(const LogoMetadata& metadata) {
    ASSERT_TRUE(logo_.get());
    ASSERT_TRUE(metadata_.get());
    EXPECT_EQ(metadata_->fingerprint, metadata.fingerprint);
    metadata_.reset(new LogoMetadata(metadata));
    logo_->metadata = metadata;
  }

  const LogoMetadata* GetCachedLogoMetadataInternal() {
    return metadata_.get();
  }

  void SetCachedLogoInternal(const EncodedLogo* logo) {
    logo_ = logo ? base::MakeUnique<EncodedLogo>(*logo) : nullptr;
    metadata_ = logo ? base::MakeUnique<LogoMetadata>(logo->metadata) : nullptr;
  }

  std::unique_ptr<EncodedLogo> GetCachedLogo() override {
    OnGetCachedLogo();
    return logo_ ? base::MakeUnique<EncodedLogo>(*logo_) : nullptr;
  }

 private:
  std::unique_ptr<LogoMetadata> metadata_;
  std::unique_ptr<EncodedLogo> logo_;
};

class MockLogoObserver : public LogoObserver {
 public:
  virtual ~MockLogoObserver() {}

  void ExpectNoLogo() {
    Mock::VerifyAndClearExpectations(this);
    EXPECT_CALL(*this, OnLogoAvailable(_, _)).Times(0);
    EXPECT_CALL(*this, OnObserverRemoved()).Times(1);
  }

  void ExpectCachedLogo(const Logo* expected_cached_logo) {
    Mock::VerifyAndClearExpectations(this);
    EXPECT_CALL(*this, OnLogoAvailable(_, true))
        .WillOnce(ExpectLogosEqualAction(expected_cached_logo));
    EXPECT_CALL(*this, OnLogoAvailable(_, false)).Times(0);
    EXPECT_CALL(*this, OnObserverRemoved()).Times(1);
  }

  void ExpectFreshLogo(const Logo* expected_fresh_logo) {
    Mock::VerifyAndClearExpectations(this);
    EXPECT_CALL(*this, OnLogoAvailable(_, true)).Times(0);
    EXPECT_CALL(*this, OnLogoAvailable(nullptr, true));
    EXPECT_CALL(*this, OnLogoAvailable(_, false))
        .WillOnce(ExpectLogosEqualAction(expected_fresh_logo));
    EXPECT_CALL(*this, OnObserverRemoved()).Times(1);
  }

  void ExpectCachedAndFreshLogos(const Logo* expected_cached_logo,
                                 const Logo* expected_fresh_logo) {
    Mock::VerifyAndClearExpectations(this);
    InSequence dummy;
    EXPECT_CALL(*this, OnLogoAvailable(_, true))
        .WillOnce(ExpectLogosEqualAction(expected_cached_logo));
    EXPECT_CALL(*this, OnLogoAvailable(_, false))
        .WillOnce(ExpectLogosEqualAction(expected_fresh_logo));
    EXPECT_CALL(*this, OnObserverRemoved()).Times(1);
  }

  MOCK_METHOD2(OnLogoAvailable, void(const Logo*, bool));
  MOCK_METHOD0(OnObserverRemoved, void());
};

class FakeImageDecoder : public image_fetcher::ImageDecoder {
 public:
  void DecodeImage(
      const std::string& image_data,
      const gfx::Size& desired_image_frame_size,
      const image_fetcher::ImageDecodedCallback& callback) override {
    gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
        reinterpret_cast<const uint8_t*>(image_data.data()), image_data.size());
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, image));
  }
};

class LogoServiceTest : public ::testing::Test {
 protected:
  LogoServiceTest()
      : template_url_service_(nullptr, 0),
        test_clock_(new base::SimpleTestClock()),
        logo_cache_(new NiceMock<MockLogoCache>()),
        fake_url_fetcher_factory_(nullptr) {
    feature_list_.InitAndEnableFeature(features::kThirdPartyDoodles);

    // Default search engine with logo. All 3P doodle_urls use ddljson API.
    AddSearchEngine("ex", "Logo Example",
                    "https://example.com/?q={searchTerms}",
                    GURL("https://example.com/logo.json"),
                    /*make_default=*/true);

    test_clock_->SetNow(base::Time::FromJsTime(INT64_C(1388686828000)));
    logo_service_ =
        base::MakeUnique<LogoService>(base::FilePath(), &template_url_service_,
                                      base::MakeUnique<FakeImageDecoder>(),
                                      new net::TestURLRequestContextGetter(
                                          base::ThreadTaskRunnerHandle::Get()),
                                      /*use_gray_background=*/false);
    logo_service_->SetClockForTests(base::WrapUnique(test_clock_));
    logo_service_->SetLogoCacheForTests(base::WrapUnique(logo_cache_));
  }

  void TearDown() override {
    // |logo_service_|'s LogoTracker owns |logo_cache_|, which gets destroyed on
    // a background sequence after the LogoTracker's destruction. Ensure that
    // |logo_cache_| is actually destroyed before the test ends to make gmock
    // happy.
    logo_service_.reset();
    task_environment_.RunUntilIdle();
  }

  // Returns the response that the server would send for the given logo.
  std::string ServerResponse(const Logo& logo) const;

  // Sets the response to be returned when the LogoTracker fetches the logo.
  void SetServerResponse(const std::string& response,
                         net::URLRequestStatus::Status request_status =
                             net::URLRequestStatus::SUCCESS,
                         net::HttpStatusCode response_code = net::HTTP_OK);

  // Sets the response to be returned when the LogoTracker fetches the logo and
  // provides the given fingerprint.
  void SetServerResponseWhenFingerprint(
      const std::string& fingerprint,
      const std::string& response_when_fingerprint,
      net::URLRequestStatus::Status request_status =
          net::URLRequestStatus::SUCCESS,
      net::HttpStatusCode response_code = net::HTTP_OK);

  const GURL& DoodleURL() const;

  // Calls logo_service_->GetLogo() with |observer_| and waits for the
  // asynchronous response(s).
  void GetLogo();

  void AddSearchEngine(base::StringPiece keyword,
                       base::StringPiece short_name,
                       const std::string& url,
                       GURL doodle_url,
                       bool make_default);

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedTaskEnvironment task_environment_;
  TemplateURLService template_url_service_;
  base::SimpleTestClock* test_clock_;
  NiceMock<MockLogoCache>* logo_cache_;
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  std::unique_ptr<LogoService> logo_service_;
  NiceMock<MockLogoObserver> observer_;
};

std::string LogoServiceTest::ServerResponse(const Logo& logo) const {
  base::TimeDelta time_to_live;
  if (!logo.metadata.expiration_time.is_null())
    time_to_live = logo.metadata.expiration_time - test_clock_->Now();
  return MakeServerResponse(logo, time_to_live);
}

void LogoServiceTest::SetServerResponse(
    const std::string& response,
    net::URLRequestStatus::Status request_status,
    net::HttpStatusCode response_code) {
  SetServerResponseWhenFingerprint(std::string(), response, request_status,
                                   response_code);
}

void LogoServiceTest::SetServerResponseWhenFingerprint(
    const std::string& fingerprint,
    const std::string& response_when_fingerprint,
    net::URLRequestStatus::Status request_status,
    net::HttpStatusCode response_code) {
  GURL url_with_fp =
      GoogleNewAppendQueryparamsToLogoURL(false, DoodleURL(), fingerprint);
  fake_url_fetcher_factory_.SetFakeResponse(
      url_with_fp, response_when_fingerprint, response_code, request_status);
}

const GURL& LogoServiceTest::DoodleURL() const {
  return template_url_service_.GetDefaultSearchProvider()->doodle_url();
}

void LogoServiceTest::GetLogo() {
  logo_service_->GetLogo(&observer_);
  task_environment_.RunUntilIdle();
}

void LogoServiceTest::AddSearchEngine(base::StringPiece keyword,
                                      base::StringPiece short_name,
                                      const std::string& url,
                                      GURL doodle_url,
                                      bool make_default) {
  TemplateURLData search_url;
  search_url.SetKeyword(base::ASCIIToUTF16(keyword));
  search_url.SetShortName(base::ASCIIToUTF16(short_name));
  search_url.SetURL(url);
  search_url.doodle_url = doodle_url;

  TemplateURL* template_url =
      template_url_service_.Add(base::MakeUnique<TemplateURL>(search_url));
  if (make_default) {
    template_url_service_.SetUserSelectedDefaultSearchProvider(template_url);
  }
}

// Tests -----------------------------------------------------------------------

TEST_F(LogoServiceTest, CTAURLHasComma) {
  GURL url_with_fp = GoogleLegacyAppendQueryparamsToLogoURL(
      false, GURL("http://logourl.com/path"), "abc123");
  EXPECT_EQ("http://logourl.com/path?async=es_dfp:abc123,cta:1",
            url_with_fp.spec());

  url_with_fp = GoogleLegacyAppendQueryparamsToLogoURL(
      false, GURL("http://logourl.com/?a=b"), "");
  EXPECT_EQ("http://logourl.com/?a=b&async=cta:1", url_with_fp.spec());
}

TEST_F(LogoServiceTest, CTAGrayBackgroundHasCommas) {
  GURL url_with_fp = GoogleLegacyAppendQueryparamsToLogoURL(
      true, GURL("http://logourl.com/path"), "abc123");
  EXPECT_EQ(
      "http://logourl.com/path?async=es_dfp:abc123,cta:1,transp:1,graybg:1",
      url_with_fp.spec());

  url_with_fp = GoogleLegacyAppendQueryparamsToLogoURL(
      true, GURL("http://logourl.com/?a=b"), "");
  EXPECT_EQ("http://logourl.com/?a=b&async=cta:1,transp:1,graybg:1",
            url_with_fp.spec());
}

TEST_F(LogoServiceTest, DownloadAndCacheLogo) {
  Logo logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  SetServerResponse(ServerResponse(logo));
  logo_cache_->ExpectSetCachedLogo(&logo);
  observer_.ExpectFreshLogo(&logo);
  GetLogo();
}

TEST_F(LogoServiceTest, EmptyCacheAndFailedDownload) {
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(nullptr)).Times(AnyNumber());

  SetServerResponse("server is borked");
  observer_.ExpectCachedLogo(nullptr);
  GetLogo();

  SetServerResponse("", net::URLRequestStatus::FAILED, net::HTTP_OK);
  observer_.ExpectCachedLogo(nullptr);
  GetLogo();

  SetServerResponse("", net::URLRequestStatus::SUCCESS, net::HTTP_BAD_GATEWAY);
  observer_.ExpectCachedLogo(nullptr);
  GetLogo();
}

TEST_F(LogoServiceTest, AcceptMinimalLogoResponse) {
  Logo logo;
  logo.image = MakeBitmap(1, 2);
  logo.metadata.source_url = DoodleURL();
  logo.metadata.can_show_after_expiration = true;
  logo.metadata.mime_type = "image/png";

  std::string response =
      ")]}' {\"ddljson\":{\"data_uri\":\"data:image/png;base64," +
      EncodeBitmapAsPNGBase64(logo.image) + "\"}}";

  SetServerResponse(response);
  observer_.ExpectFreshLogo(&logo);
  GetLogo();
}

TEST_F(LogoServiceTest, ReturnCachedLogo) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint, "",
                                   net::URLRequestStatus::FAILED, net::HTTP_OK);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(&cached_logo);
  GetLogo();
}

TEST_F(LogoServiceTest, ValidateCachedLogo) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  // During revalidation, the image data and mime_type are absent.
  Logo fresh_logo = cached_logo;
  fresh_logo.image.reset();
  fresh_logo.metadata.mime_type.clear();
  fresh_logo.metadata.expiration_time =
      test_clock_->Now() + base::TimeDelta::FromDays(8);
  SetServerResponseWhenFingerprint(fresh_logo.metadata.fingerprint,
                                   ServerResponse(fresh_logo));

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(1);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(&cached_logo);
  GetLogo();

  ASSERT_TRUE(logo_cache_->GetCachedLogoMetadata());
  EXPECT_EQ(fresh_logo.metadata.expiration_time,
            logo_cache_->GetCachedLogoMetadata()->expiration_time);

  // Ensure that cached logo is still returned correctly on subsequent requests.
  // In particular, the metadata should stay valid. http://crbug.com/480090
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(1);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(&cached_logo);
  GetLogo();
}

TEST_F(LogoServiceTest, UpdateCachedLogoMetadata) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  Logo fresh_logo = cached_logo;
  fresh_logo.image.reset();
  fresh_logo.metadata.mime_type.clear();
  fresh_logo.metadata.on_click_url = GURL("http://new.onclick.url");
  fresh_logo.metadata.alt_text = "new alt text";
  fresh_logo.metadata.animated_url = GURL("http://new.animated.url");
  fresh_logo.metadata.expiration_time =
      test_clock_->Now() + base::TimeDelta::FromDays(8);
  SetServerResponseWhenFingerprint(fresh_logo.metadata.fingerprint,
                                   ServerResponse(fresh_logo));

  // On the first request, the cached logo should be used.
  observer_.ExpectCachedLogo(&cached_logo);
  GetLogo();

  // Subsequently, the cached image should be returned along with the updated
  // metadata.
  Logo expected_logo = fresh_logo;
  expected_logo.image = cached_logo.image;
  expected_logo.metadata.mime_type = cached_logo.metadata.mime_type;
  observer_.ExpectCachedLogo(&expected_logo);
  GetLogo();
}

TEST_F(LogoServiceTest, UpdateCachedLogo) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  Logo fresh_logo = GetSampleLogo2(DoodleURL(), test_clock_->Now());
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint,
                                   ServerResponse(fresh_logo));

  logo_cache_->ExpectSetCachedLogo(&fresh_logo);
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedAndFreshLogos(&cached_logo, &fresh_logo);

  GetLogo();
}

TEST_F(LogoServiceTest, InvalidateCachedLogo) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  // This response means there's no current logo.
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint,
                                   ")]}' {\"update\":{}}");

  logo_cache_->ExpectSetCachedLogo(nullptr);
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedAndFreshLogos(&cached_logo, nullptr);

  GetLogo();
}

TEST_F(LogoServiceTest, DeleteCachedLogoFromOldUrl) {
  SetServerResponse("", net::URLRequestStatus::FAILED, net::HTTP_OK);
  Logo cached_logo =
      GetSampleLogo(GURL("http://oldsearchprovider.com"), test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(nullptr)).Times(AnyNumber());
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(nullptr);
  GetLogo();
}

TEST_F(LogoServiceTest, LogoWithTTLCannotBeShownAfterExpiration) {
  Logo logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  base::TimeDelta time_to_live = base::TimeDelta::FromDays(3);
  logo.metadata.expiration_time = test_clock_->Now() + time_to_live;
  SetServerResponse(ServerResponse(logo));
  GetLogo();

  const LogoMetadata* cached_metadata = logo_cache_->GetCachedLogoMetadata();
  ASSERT_TRUE(cached_metadata);
  EXPECT_FALSE(cached_metadata->can_show_after_expiration);
  EXPECT_EQ(test_clock_->Now() + time_to_live,
            cached_metadata->expiration_time);
}

TEST_F(LogoServiceTest, LogoWithoutTTLCanBeShownAfterExpiration) {
  Logo logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  base::TimeDelta time_to_live = base::TimeDelta();
  SetServerResponse(MakeServerResponse(logo, time_to_live));
  GetLogo();

  const LogoMetadata* cached_metadata = logo_cache_->GetCachedLogoMetadata();
  ASSERT_TRUE(cached_metadata);
  EXPECT_TRUE(cached_metadata->can_show_after_expiration);
  EXPECT_EQ(test_clock_->Now() + base::TimeDelta::FromDays(30),
            cached_metadata->expiration_time);
}

TEST_F(LogoServiceTest, UseSoftExpiredCachedLogo) {
  SetServerResponse("", net::URLRequestStatus::FAILED, net::HTTP_OK);
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  cached_logo.metadata.expiration_time =
      test_clock_->Now() - base::TimeDelta::FromSeconds(1);
  cached_logo.metadata.can_show_after_expiration = true;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(&cached_logo);
  GetLogo();
}

TEST_F(LogoServiceTest, RerequestSoftExpiredCachedLogo) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  cached_logo.metadata.expiration_time =
      test_clock_->Now() - base::TimeDelta::FromDays(5);
  cached_logo.metadata.can_show_after_expiration = true;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  Logo fresh_logo = GetSampleLogo2(DoodleURL(), test_clock_->Now());
  SetServerResponse(ServerResponse(fresh_logo));

  logo_cache_->ExpectSetCachedLogo(&fresh_logo);
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedAndFreshLogos(&cached_logo, &fresh_logo);

  GetLogo();
}

TEST_F(LogoServiceTest, DeleteAncientCachedLogo) {
  SetServerResponse("", net::URLRequestStatus::FAILED, net::HTTP_OK);
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  cached_logo.metadata.expiration_time =
      test_clock_->Now() - base::TimeDelta::FromDays(200);
  cached_logo.metadata.can_show_after_expiration = true;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(nullptr)).Times(AnyNumber());
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(nullptr);
  GetLogo();
}

TEST_F(LogoServiceTest, DeleteExpiredCachedLogo) {
  SetServerResponse("", net::URLRequestStatus::FAILED, net::HTTP_OK);
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  cached_logo.metadata.expiration_time =
      test_clock_->Now() - base::TimeDelta::FromSeconds(1);
  cached_logo.metadata.can_show_after_expiration = false;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(nullptr)).Times(AnyNumber());
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(nullptr);
  GetLogo();
}

// Tests that deal with multiple listeners.

void EnqueueObservers(
    LogoService* logo_service,
    const std::vector<std::unique_ptr<MockLogoObserver>>& observers,
    size_t start_index) {
  if (start_index >= observers.size())
    return;

  logo_service->GetLogo(observers[start_index].get());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EnqueueObservers, logo_service,
                            base::ConstRef(observers), start_index + 1));
}

#if defined(THREAD_SANITIZER)
// Flakes on Linux TSan: http://crbug/754599 (data race).
#define MAYBE_SupportOverlappingLogoRequests \
  DISABLED_SupportOverlappingLogoRequests
#else
#define MAYBE_SupportOverlappingLogoRequests SupportOverlappingLogoRequests
#endif
TEST_F(LogoServiceTest, MAYBE_SupportOverlappingLogoRequests) {
  Logo cached_logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);
  ON_CALL(*logo_cache_, SetCachedLogo(_)).WillByDefault(Return());

  Logo fresh_logo = GetSampleLogo2(DoodleURL(), test_clock_->Now());
  std::string response = ServerResponse(fresh_logo);
  SetServerResponse(response);
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint, response);

  const int kNumListeners = 10;
  std::vector<std::unique_ptr<MockLogoObserver>> listeners;
  for (int i = 0; i < kNumListeners; ++i) {
    auto listener = base::MakeUnique<MockLogoObserver>();
    listener->ExpectCachedAndFreshLogos(&cached_logo, &fresh_logo);
    listeners.push_back(std::move(listener));
  }
  EnqueueObservers(logo_service_.get(), listeners, 0);

  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(AtMost(3));
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(3));

  task_environment_.RunUntilIdle();
}

TEST_F(LogoServiceTest, DeleteObserversWhenLogoURLChanged) {
  MockLogoObserver listener1;
  listener1.ExpectNoLogo();
  logo_service_->GetLogo(&listener1);

  // Change default search engine; new DSE has a doodle URL.
  AddSearchEngine("cr", "Chromium", "https://www.chromium.org/?q={searchTerms}",
                  GURL("https://chromium.org/logo.json"),
                  /*make_default=*/true);

  Logo logo = GetSampleLogo(DoodleURL(), test_clock_->Now());
  SetServerResponse(ServerResponse(logo));

  MockLogoObserver listener2;
  listener2.ExpectFreshLogo(&logo);
  logo_service_->GetLogo(&listener2);

  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace search_provider_logos
