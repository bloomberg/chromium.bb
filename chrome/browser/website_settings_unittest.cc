// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/website_settings.h"

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/ui/website_settings_ui.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/cert_store.h"
#include "content/public/common/ssl_status.h"
#include "content/test/test_browser_thread.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/test_certificate_data.h"
#include "net/base/x509_certificate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::SSLStatus;
using namespace testing;

namespace {

// SSL cipher suite like specified in RFC5246 Appendix A.5. "The Cipher Suite".
static int TLS_RSA_WITH_AES_256_CBC_SHA256 = 0x3D;

int SetSSLVersion(int connection_status, int version) {
  // Clear SSL version bits (Bits 20, 21 and 22).
  connection_status &=
      ~(net::SSL_CONNECTION_VERSION_MASK << net::SSL_CONNECTION_VERSION_SHIFT);
  int bitmask = version << net::SSL_CONNECTION_VERSION_SHIFT;
  return bitmask | connection_status;
}

int SetSSLCipherSuite(int connection_status, int cipher_suite) {
  // Clear cipher suite bits (the 16 lowest bits).
  connection_status &= ~net::SSL_CONNECTION_CIPHERSUITE_MASK;
  return cipher_suite | connection_status;
}

class MockCertStore : public content::CertStore {
 public:
  virtual ~MockCertStore() {}
  MOCK_METHOD2(StoreCert, int(net::X509Certificate*, int));
  MOCK_METHOD2(RetrieveCert, bool(int, scoped_refptr<net::X509Certificate>*));
};

class MockWebsiteSettingsUI : public WebsiteSettingsUI {
 public:
  virtual ~MockWebsiteSettingsUI() {}
  MOCK_METHOD1(SetPresenter, void(WebsiteSettings* presenter));
  MOCK_METHOD1(SetSiteInfo, void(const std::string& site_info));
  MOCK_METHOD1(SetCookieInfo, void(const CookieInfoList& cookie_info_list));
  MOCK_METHOD1(SetPermissionInfo,
               void(const PermissionInfoList& permission_info_list));
};

class WebsiteSettingsTest : public testing::Test {
 public:
  WebsiteSettingsTest()
      : testing::Test(),
        message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        profile_(new TestingProfile()),
        cert_id_(0),
        url_("http://www.example.com"),
        website_settings_(NULL) {
  }

  virtual ~WebsiteSettingsTest() {
  }

  virtual void SetUp() {
    // Setup stub SSLStatus.
    ssl_.security_style = content::SECURITY_STYLE_UNAUTHENTICATED;

    // Create the certificate.
    cert_id_ = 1;
    base::Time start_date = base::Time::Now();
    base::Time expiration_date = base::Time::FromInternalValue(
        start_date.ToInternalValue() + base::Time::kMicrosecondsPerWeek);
    cert_ = new net::X509Certificate("subject",
                                     "issuer",
                                     start_date,
                                     expiration_date);

    // Setup the mock cert store.
    EXPECT_CALL(cert_store_, RetrieveCert(cert_id_, _) )
        .Times(AnyNumber())
        .WillRepeatedly(DoAll(SetArgPointee<1>(cert_), Return(true)));
  }

  virtual void TearDown() {
    ASSERT_TRUE(website_settings_) << "No WebsiteSettings instance created.";
    // Call OnUIClosing to destroy the |website_settings| object.
    website_settings_->OnUIClosing();
  }

  WebsiteSettingsUI* CreateMockUI() {
    MockWebsiteSettingsUI* ui = new MockWebsiteSettingsUI();
    EXPECT_CALL(*ui, SetPermissionInfo(_)).Times(AnyNumber());
    EXPECT_CALL(*ui, SetSiteInfo(_)).Times(AnyNumber());
    EXPECT_CALL(*ui, SetPresenter(_)).Times(AnyNumber());
    EXPECT_CALL(*ui, SetCookieInfo(_)).Times(AnyNumber());
    return ui;
  }

  Profile* profile() const { return profile_.get(); }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<Profile> profile_;
  int cert_id_;
  scoped_refptr<net::X509Certificate> cert_;
  MockCertStore cert_store_;
  GURL url_;
  SSLStatus ssl_;
  WebsiteSettings* website_settings_;
};

}  // namespace

TEST_F(WebsiteSettingsTest, OnPermissionsChanged) {
  MockWebsiteSettingsUI* ui = new MockWebsiteSettingsUI();
  EXPECT_CALL(*ui, SetPermissionInfo(_));
  EXPECT_CALL(*ui, SetSiteInfo(_));
  EXPECT_CALL(*ui, SetPresenter(_)).Times(2);

  HostContentSettingsMap* content_settings =
      profile()->GetHostContentSettingsMap();
  ContentSetting setting = content_settings->GetContentSetting(
      url_, url_, CONTENT_SETTINGS_TYPE_POPUPS, "");
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
  setting = content_settings->GetContentSetting(
      url_, url_, CONTENT_SETTINGS_TYPE_PLUGINS, "");
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url_, url_, CONTENT_SETTINGS_TYPE_GEOLOCATION, "");
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      url_, url_, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "");
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);

  website_settings_ = new WebsiteSettings(ui, profile(), url_, ssl_,
                                          &cert_store_);
  website_settings_->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_POPUPS,
                                             CONTENT_SETTING_ALLOW);
  website_settings_->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_PLUGINS,
                                             CONTENT_SETTING_BLOCK);
  website_settings_->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                             CONTENT_SETTING_ALLOW);
  website_settings_->OnSitePermissionChanged(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);

  setting = content_settings->GetContentSetting(
      url_, url_, CONTENT_SETTINGS_TYPE_POPUPS, "");
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url_, url_, CONTENT_SETTINGS_TYPE_PLUGINS, "");
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
  setting = content_settings->GetContentSetting(
      url_, url_, CONTENT_SETTINGS_TYPE_GEOLOCATION, "");
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url_, url_, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "");
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
}

TEST_F(WebsiteSettingsTest, HTTPConnection) {
  website_settings_ = new WebsiteSettings(CreateMockUI(), profile(), url_, ssl_,
                                          &cert_store_);
  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_UNENCRYPTED,
            website_settings_->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_NO_CERT,
            website_settings_->site_identity_status());
  EXPECT_EQ(string16(), website_settings_->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSConnection) {
  ssl_.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl_.cert_id = cert_id_;
  ssl_.cert_status = 0;
  ssl_.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl_.connection_status = status;

  website_settings_ = new WebsiteSettings(CreateMockUI(), profile(), url_, ssl_,
                                          &cert_store_);
  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
            website_settings_->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT,
            website_settings_->site_identity_status());
  EXPECT_EQ(string16(), website_settings_->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSMixedContent) {
  ssl_.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl_.cert_id = cert_id_;
  ssl_.cert_status = 0;
  ssl_.security_bits = 81;  // No error if > 80.
  ssl_.content_status = SSLStatus::DISPLAYED_INSECURE_CONTENT;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl_.connection_status = status;

  website_settings_ = new WebsiteSettings(CreateMockUI(), profile(), url_, ssl_,
                                          &cert_store_);
  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_MIXED_CONTENT,
            website_settings_->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT,
            website_settings_->site_identity_status());
  EXPECT_EQ(string16(), website_settings_->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSEVCert) {
  scoped_refptr<net::X509Certificate> ev_cert =
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der),
          sizeof(google_der));
  int ev_cert_id = 1;
  EXPECT_CALL(cert_store_, RetrieveCert(ev_cert_id, _)).WillRepeatedly(
      DoAll(SetArgPointee<1>(ev_cert), Return(true)));

  ssl_.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl_.cert_id = ev_cert_id;
  ssl_.cert_status = net::CERT_STATUS_IS_EV;
  ssl_.security_bits = 81;  // No error if > 80.
  ssl_.content_status = SSLStatus::DISPLAYED_INSECURE_CONTENT;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl_.connection_status = status;

  website_settings_ = new WebsiteSettings(CreateMockUI(), profile(), url_, ssl_,
                          &cert_store_);
  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_MIXED_CONTENT,
            website_settings_->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT,
            website_settings_->site_identity_status());
  EXPECT_EQ(UTF8ToUTF16("Google Inc"), website_settings_->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSRevocationError) {
  ssl_.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl_.cert_id = cert_id_;
  ssl_.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  ssl_.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl_.connection_status = status;

  website_settings_ = new WebsiteSettings(CreateMockUI(), profile(), url_, ssl_,
                                          &cert_store_);
  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
            website_settings_->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN,
            website_settings_->site_identity_status());
  EXPECT_EQ(string16(), website_settings_->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSConnectionError) {
  ssl_.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl_.cert_id = cert_id_;
  ssl_.cert_status = 0;
  ssl_.security_bits = 1;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl_.connection_status = status;

  website_settings_ = new WebsiteSettings(CreateMockUI(), profile(), url_, ssl_,
                                          &cert_store_);
  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR,
            website_settings_->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT,
            website_settings_->site_identity_status());
  EXPECT_EQ(string16(), website_settings_->organization_name());
}
