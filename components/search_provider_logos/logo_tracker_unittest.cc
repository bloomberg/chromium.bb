// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_tracker.h"

#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/search_provider_logos/google_logo_api.h"
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
  logo.image = gfx::Image::CreateFrom1xPNGBytes(
      encoded_logo.encoded_image->front(),
      encoded_logo.encoded_image->size()).AsBitmap();
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
  logo.metadata.source_url = logo_url.spec();
  logo.metadata.on_click_url = "http://www.google.com/search?q=potato";
  logo.metadata.alt_text = "A logo about potatoes";
  logo.metadata.mime_type = "image/png";
  return logo;
}

Logo GetSampleLogo2(const GURL& logo_url, base::Time response_time) {
  Logo logo;
  logo.image = MakeBitmap(4, 3);
  logo.metadata.can_show_after_expiration = true;
  logo.metadata.expiration_time = base::Time();
  logo.metadata.fingerprint = "71082741021409127";
  logo.metadata.source_url = logo_url.spec();
  logo.metadata.on_click_url = "http://example.com/page25";
  logo.metadata.alt_text = "The logo for example.com";
  logo.metadata.mime_type = "image/png";
  return logo;
}

std::string MakeServerResponse(
    const SkBitmap& image,
    const std::string& on_click_url,
    const std::string& alt_text,
    const std::string& mime_type,
    const std::string& fingerprint,
    base::TimeDelta time_to_live) {
  base::DictionaryValue dict;
  if (!image.isNull()) {
    dict.SetString("update.logo.data", EncodeBitmapAsPNGBase64(image));
  }

  dict.SetString("update.logo.target", on_click_url);
  dict.SetString("update.logo.alt", alt_text);
  dict.SetString("update.logo.mime_type", mime_type);
  dict.SetString("update.logo.fingerprint", fingerprint);
  if (time_to_live.ToInternalValue() != 0)
    dict.SetInteger("update.logo.time_to_live",
                    static_cast<int>(time_to_live.InMilliseconds()));

  std::string output;
  base::JSONWriter::Write(&dict, &output);
  return output;
}

std::string MakeServerResponse(const Logo& logo, base::TimeDelta time_to_live) {
  return MakeServerResponse(logo.image,
                            logo.metadata.on_click_url,
                            logo.metadata.alt_text,
                            logo.metadata.mime_type,
                            logo.metadata.fingerprint,
                            time_to_live);
}

void ExpectLogosEqual(const Logo* expected_logo,
                      const Logo* actual_logo) {
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
  ExpectLogosEqual(expected_logo, actual_encoded_logo ? &actual_logo : NULL);
}

ACTION_P(ExpectLogosEqualAction, expected_logo) {
  ExpectLogosEqual(expected_logo, arg0);
}

class MockLogoCache : public LogoCache {
 public:
  MockLogoCache() : LogoCache(base::FilePath()) {
    // Delegate actions to the *Internal() methods by default.
    ON_CALL(*this, UpdateCachedLogoMetadata(_)).WillByDefault(
        Invoke(this, &MockLogoCache::UpdateCachedLogoMetadataInternal));
    ON_CALL(*this, GetCachedLogoMetadata()).WillByDefault(
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
    metadata_.reset(new LogoMetadata(metadata));
  }

  virtual const LogoMetadata* GetCachedLogoMetadataInternal() {
    return metadata_.get();
  }

  virtual void SetCachedLogoInternal(const EncodedLogo* logo) {
    logo_.reset(logo ? new EncodedLogo(*logo) : NULL);
    metadata_.reset(logo ? new LogoMetadata(logo->metadata) : NULL);
  }

  virtual scoped_ptr<EncodedLogo> GetCachedLogo() OVERRIDE {
    OnGetCachedLogo();
    return make_scoped_ptr(logo_ ? new EncodedLogo(*logo_) : NULL);
  }

 private:
  scoped_ptr<LogoMetadata> metadata_;
  scoped_ptr<EncodedLogo> logo_;
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
    EXPECT_CALL(*this, OnLogoAvailable(NULL, true));
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

class TestLogoDelegate : public LogoDelegate {
 public:
  TestLogoDelegate() {}
  virtual ~TestLogoDelegate() {}

  virtual void DecodeUntrustedImage(
      const scoped_refptr<base::RefCountedString>& encoded_image,
      base::Callback<void(const SkBitmap&)> image_decoded_callback) OVERRIDE {
    SkBitmap bitmap =
        gfx::Image::CreateFrom1xPNGBytes(encoded_image->front(),
                                         encoded_image->size()).AsBitmap();
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(image_decoded_callback, bitmap));
  }
};

class LogoTrackerTest : public ::testing::Test {
 protected:
  LogoTrackerTest()
      : message_loop_(new base::MessageLoop()),
        logo_url_("https://google.com/doodleoftheday?size=hp"),
        test_clock_(new base::SimpleTestClock()),
        logo_cache_(new NiceMock<MockLogoCache>()),
        fake_url_fetcher_factory_(NULL) {
    test_clock_->SetNow(base::Time::FromJsTime(GG_INT64_C(1388686828000)));
    logo_tracker_ = new LogoTracker(
        base::FilePath(),
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        new net::TestURLRequestContextGetter(base::MessageLoopProxy::current()),
        scoped_ptr<LogoDelegate>(new TestLogoDelegate()));
    logo_tracker_->SetServerAPI(logo_url_,
                                base::Bind(&GoogleParseLogoResponse),
                                base::Bind(&GoogleAppendFingerprintToLogoURL));
    logo_tracker_->SetClockForTests(scoped_ptr<base::Clock>(test_clock_));
    logo_tracker_->SetLogoCacheForTests(scoped_ptr<LogoCache>(logo_cache_));
  }

  virtual void TearDown() {
    // logo_tracker_ owns logo_cache_, which gets destructed on the file thread
    // after logo_tracker_'s destruction. Ensure that logo_cache_ is actually
    // destructed before the test ends to make gmock happy.
    delete logo_tracker_;
    message_loop_->RunUntilIdle();
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

  // Calls logo_tracker_->GetLogo() with listener_ and waits for the
  // asynchronous response(s).
  void GetLogo();

  scoped_ptr<base::MessageLoop> message_loop_;
  GURL logo_url_;
  base::SimpleTestClock* test_clock_;
  NiceMock<MockLogoCache>* logo_cache_;
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  LogoTracker* logo_tracker_;
  NiceMock<MockLogoObserver> observer_;
};

std::string LogoTrackerTest::ServerResponse(const Logo& logo) const {
  base::TimeDelta time_to_live;
  if (!logo.metadata.expiration_time.is_null())
    time_to_live = logo.metadata.expiration_time - test_clock_->Now();
  return MakeServerResponse(logo, time_to_live);
}

void LogoTrackerTest::SetServerResponse(
    const std::string& response,
    net::URLRequestStatus::Status request_status,
    net::HttpStatusCode response_code) {
  fake_url_fetcher_factory_.SetFakeResponse(
      logo_url_, response, response_code, request_status);
}

void LogoTrackerTest::SetServerResponseWhenFingerprint(
    const std::string& fingerprint,
    const std::string& response_when_fingerprint,
    net::URLRequestStatus::Status request_status,
    net::HttpStatusCode response_code) {
  GURL url_with_fp =
      net::AppendQueryParameter(logo_url_, "async", "es_dfp:" + fingerprint);
  fake_url_fetcher_factory_.SetFakeResponse(
      url_with_fp, response_when_fingerprint, response_code, request_status);
}

void LogoTrackerTest::GetLogo() {
  logo_tracker_->GetLogo(&observer_);
  base::RunLoop().RunUntilIdle();
}

// Tests -----------------------------------------------------------------------

TEST_F(LogoTrackerTest, DownloadAndCacheLogo) {
  Logo logo = GetSampleLogo(logo_url_, test_clock_->Now());
  SetServerResponse(ServerResponse(logo));
  logo_cache_->ExpectSetCachedLogo(&logo);
  observer_.ExpectFreshLogo(&logo);
  GetLogo();
}

TEST_F(LogoTrackerTest, EmptyCacheAndFailedDownload) {
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(NULL)).Times(AnyNumber());

  SetServerResponse("server is borked");
  observer_.ExpectCachedLogo(NULL);
  GetLogo();

  SetServerResponse("", net::URLRequestStatus::FAILED, net::HTTP_OK);
  observer_.ExpectCachedLogo(NULL);
  GetLogo();

  SetServerResponse("", net::URLRequestStatus::SUCCESS, net::HTTP_BAD_GATEWAY);
  observer_.ExpectCachedLogo(NULL);
  GetLogo();
}

TEST_F(LogoTrackerTest, AcceptMinimalLogoResponse) {
  Logo logo;
  logo.image = MakeBitmap(1, 2);
  logo.metadata.source_url = logo_url_.spec();
  logo.metadata.can_show_after_expiration = true;

  std::string response = ")]}' {\"update\":{\"logo\":{\"data\":\"" +
                         EncodeBitmapAsPNGBase64(logo.image) +
                         "\",\"mime_type\":\"image/png\"}}}";

  SetServerResponse(response);
  observer_.ExpectFreshLogo(&logo);
  GetLogo();
}

TEST_F(LogoTrackerTest, ReturnCachedLogo) {
  Logo cached_logo = GetSampleLogo(logo_url_, test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint,
                                   "",
                                   net::URLRequestStatus::FAILED,
                                   net::HTTP_OK);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(&cached_logo);
  GetLogo();
}

TEST_F(LogoTrackerTest, ValidateCachedLogoFingerprint) {
  Logo cached_logo = GetSampleLogo(logo_url_, test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  Logo fresh_logo = cached_logo;
  fresh_logo.image.reset();
  fresh_logo.metadata.expiration_time =
      test_clock_->Now() + base::TimeDelta::FromDays(8);
  SetServerResponseWhenFingerprint(fresh_logo.metadata.fingerprint,
                                   ServerResponse(fresh_logo));

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(1);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(&cached_logo);

  GetLogo();

  EXPECT_TRUE(logo_cache_->GetCachedLogoMetadata() != NULL);
  EXPECT_EQ(logo_cache_->GetCachedLogoMetadata()->expiration_time,
            fresh_logo.metadata.expiration_time);
}

TEST_F(LogoTrackerTest, UpdateCachedLogo) {
  Logo cached_logo = GetSampleLogo(logo_url_, test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  Logo fresh_logo = GetSampleLogo2(logo_url_, test_clock_->Now());
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint,
                                   ServerResponse(fresh_logo));

  logo_cache_->ExpectSetCachedLogo(&fresh_logo);
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedAndFreshLogos(&cached_logo, &fresh_logo);

  GetLogo();
}

TEST_F(LogoTrackerTest, InvalidateCachedLogo) {
  Logo cached_logo = GetSampleLogo(logo_url_, test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  // This response means there's no current logo.
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint,
                                   ")]}' {\"update\":{}}");

  logo_cache_->ExpectSetCachedLogo(NULL);
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedAndFreshLogos(&cached_logo, NULL);

  GetLogo();
}

TEST_F(LogoTrackerTest, DeleteCachedLogoFromOldUrl) {
  SetServerResponse("", net::URLRequestStatus::FAILED, net::HTTP_OK);
  Logo cached_logo =
      GetSampleLogo(GURL("http://oldsearchprovider.com"), test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(NULL)).Times(AnyNumber());
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(NULL);
  GetLogo();
}

TEST_F(LogoTrackerTest, LogoWithTTLCannotBeShownAfterExpiration) {
  Logo logo = GetSampleLogo(logo_url_, test_clock_->Now());
  base::TimeDelta time_to_live = base::TimeDelta::FromDays(3);
  logo.metadata.expiration_time = test_clock_->Now() + time_to_live;
  SetServerResponse(ServerResponse(logo));
  GetLogo();

  const LogoMetadata* cached_metadata =
      logo_cache_->GetCachedLogoMetadata();
  EXPECT_TRUE(cached_metadata != NULL);
  EXPECT_FALSE(cached_metadata->can_show_after_expiration);
  EXPECT_EQ(test_clock_->Now() + time_to_live,
            cached_metadata->expiration_time);
}

TEST_F(LogoTrackerTest, LogoWithoutTTLCanBeShownAfterExpiration) {
  Logo logo = GetSampleLogo(logo_url_, test_clock_->Now());
  base::TimeDelta time_to_live = base::TimeDelta();
  SetServerResponse(MakeServerResponse(logo, time_to_live));
  GetLogo();

  const LogoMetadata* cached_metadata =
      logo_cache_->GetCachedLogoMetadata();
  EXPECT_TRUE(cached_metadata != NULL);
  EXPECT_TRUE(cached_metadata->can_show_after_expiration);
  EXPECT_EQ(test_clock_->Now() + base::TimeDelta::FromDays(30),
            cached_metadata->expiration_time);
}

TEST_F(LogoTrackerTest, UseSoftExpiredCachedLogo) {
  SetServerResponse("", net::URLRequestStatus::FAILED, net::HTTP_OK);
  Logo cached_logo = GetSampleLogo(logo_url_, test_clock_->Now());
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

TEST_F(LogoTrackerTest, RerequestSoftExpiredCachedLogo) {
  Logo cached_logo = GetSampleLogo(logo_url_, test_clock_->Now());
  cached_logo.metadata.expiration_time =
      test_clock_->Now() - base::TimeDelta::FromDays(5);
  cached_logo.metadata.can_show_after_expiration = true;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  Logo fresh_logo = GetSampleLogo2(logo_url_, test_clock_->Now());
  SetServerResponse(ServerResponse(fresh_logo));

  logo_cache_->ExpectSetCachedLogo(&fresh_logo);
  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedAndFreshLogos(&cached_logo, &fresh_logo);

  GetLogo();
}

TEST_F(LogoTrackerTest, DeleteAncientCachedLogo) {
  SetServerResponse("", net::URLRequestStatus::FAILED, net::HTTP_OK);
  Logo cached_logo = GetSampleLogo(logo_url_, test_clock_->Now());
  cached_logo.metadata.expiration_time =
      test_clock_->Now() - base::TimeDelta::FromDays(200);
  cached_logo.metadata.can_show_after_expiration = true;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(NULL)).Times(AnyNumber());
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(NULL);
  GetLogo();
}

TEST_F(LogoTrackerTest, DeleteExpiredCachedLogo) {
  SetServerResponse("", net::URLRequestStatus::FAILED, net::HTTP_OK);
  Logo cached_logo = GetSampleLogo(logo_url_, test_clock_->Now());
  cached_logo.metadata.expiration_time =
      test_clock_->Now() - base::TimeDelta::FromSeconds(1);
  cached_logo.metadata.can_show_after_expiration = false;
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);

  EXPECT_CALL(*logo_cache_, UpdateCachedLogoMetadata(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(0);
  EXPECT_CALL(*logo_cache_, SetCachedLogo(NULL)).Times(AnyNumber());
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(1));
  observer_.ExpectCachedLogo(NULL);
  GetLogo();
}

// Tests that deal with multiple listeners.

void EnqueueObservers(LogoTracker* logo_tracker,
                      const ScopedVector<MockLogoObserver>& observers,
                      size_t start_index) {
  if (start_index >= observers.size())
    return;

  logo_tracker->GetLogo(observers[start_index]);
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(&EnqueueObservers,
                                                    logo_tracker,
                                                    base::ConstRef(observers),
                                                    start_index + 1));
}

TEST_F(LogoTrackerTest, SupportOverlappingLogoRequests) {
  Logo cached_logo = GetSampleLogo(logo_url_, test_clock_->Now());
  logo_cache_->EncodeAndSetCachedLogo(cached_logo);
  ON_CALL(*logo_cache_, SetCachedLogo(_)).WillByDefault(Return());

  Logo fresh_logo = GetSampleLogo2(logo_url_, test_clock_->Now());
  std::string response = ServerResponse(fresh_logo);
  SetServerResponse(response);
  SetServerResponseWhenFingerprint(cached_logo.metadata.fingerprint, response);

  const int kNumListeners = 10;
  ScopedVector<MockLogoObserver> listeners;
  for (int i = 0; i < kNumListeners; ++i) {
    MockLogoObserver* listener = new MockLogoObserver();
    listener->ExpectCachedAndFreshLogos(&cached_logo, &fresh_logo);
    listeners.push_back(listener);
  }
  EnqueueObservers(logo_tracker_, listeners, 0);

  EXPECT_CALL(*logo_cache_, SetCachedLogo(_)).Times(AtMost(3));
  EXPECT_CALL(*logo_cache_, OnGetCachedLogo()).Times(AtMost(3));

  base::RunLoop().RunUntilIdle();
}

TEST_F(LogoTrackerTest, DeleteObserversWhenLogoURLChanged) {
  MockLogoObserver listener1;
  listener1.ExpectNoLogo();
  logo_tracker_->GetLogo(&listener1);

  logo_url_ = GURL("http://example.com/new-logo-url");
  logo_tracker_->SetServerAPI(logo_url_,
                              base::Bind(&GoogleParseLogoResponse),
                              base::Bind(&GoogleAppendFingerprintToLogoURL));
  Logo logo = GetSampleLogo(logo_url_, test_clock_->Now());
  SetServerResponse(ServerResponse(logo));

  MockLogoObserver listener2;
  listener2.ExpectFreshLogo(&logo);
  logo_tracker_->GetLogo(&listener2);

  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace search_provider_logos
