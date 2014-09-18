// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings.h"

#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/cert_store.h"
#include "content/public/common/ssl_status.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/test_certificate_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::SSLStatus;
using testing::_;
using testing::AnyNumber;
using testing::Return;
using testing::SetArgPointee;

namespace {

// SSL cipher suite like specified in RFC5246 Appendix A.5. "The Cipher Suite".
// Without the CR_ prefix, this clashes with the OS X 10.8 headers.
int CR_TLS_RSA_WITH_AES_256_CBC_SHA256 = 0x3D;

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
  MOCK_METHOD1(SetCookieInfo, void(const CookieInfoList& cookie_info_list));
  MOCK_METHOD1(SetPermissionInfo,
               void(const PermissionInfoList& permission_info_list));
  MOCK_METHOD1(SetIdentityInfo, void(const IdentityInfo& identity_info));
  MOCK_METHOD1(SetFirstVisit, void(const base::string16& first_visit));
  MOCK_METHOD1(SetSelectedTab, void(TabId tab_id));
};

class WebsiteSettingsTest : public ChromeRenderViewHostTestHarness {
 public:
  WebsiteSettingsTest() : cert_id_(0), url_("http://www.example.com") {}

  virtual ~WebsiteSettingsTest() {
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();
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

    TabSpecificContentSettings::CreateForWebContents(web_contents());
    InfoBarService::CreateForWebContents(web_contents());

    // Setup the mock cert store.
    EXPECT_CALL(cert_store_, RetrieveCert(cert_id_, _) )
        .Times(AnyNumber())
        .WillRepeatedly(DoAll(SetArgPointee<1>(cert_), Return(true)));

    // Setup mock ui.
    mock_ui_.reset(new MockWebsiteSettingsUI());
  }

  virtual void TearDown() {
    ASSERT_TRUE(website_settings_.get())
        << "No WebsiteSettings instance created.";
    RenderViewHostTestHarness::TearDown();
    website_settings_.reset();
  }

  void SetDefaultUIExpectations(MockWebsiteSettingsUI* mock_ui) {
    // During creation |WebsiteSettings| makes the following calls to the ui.
    EXPECT_CALL(*mock_ui, SetPermissionInfo(_));
    EXPECT_CALL(*mock_ui, SetIdentityInfo(_));
    EXPECT_CALL(*mock_ui, SetCookieInfo(_));
    EXPECT_CALL(*mock_ui, SetFirstVisit(base::string16()));
  }

  const GURL& url() const { return url_; }
  MockCertStore* cert_store() { return &cert_store_; }
  int cert_id() { return cert_id_; }
  MockWebsiteSettingsUI* mock_ui() { return mock_ui_.get(); }
  const SSLStatus& ssl() { return ssl_; }
  TabSpecificContentSettings* tab_specific_content_settings() {
    return TabSpecificContentSettings::FromWebContents(web_contents());
  }
  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }

  WebsiteSettings* website_settings() {
    if (!website_settings_.get()) {
      website_settings_.reset(new WebsiteSettings(
          mock_ui(), profile(), tab_specific_content_settings(),
          infobar_service(), url(), ssl(), cert_store()));
    }
    return website_settings_.get();
  }

  SSLStatus ssl_;

 private:
  scoped_ptr<WebsiteSettings> website_settings_;
  scoped_ptr<MockWebsiteSettingsUI> mock_ui_;
  int cert_id_;
  scoped_refptr<net::X509Certificate> cert_;
  MockCertStore cert_store_;
  GURL url_;
};

}  // namespace

TEST_F(WebsiteSettingsTest, OnPermissionsChanged) {
  // Setup site permissions.
  HostContentSettingsMap* content_settings =
      profile()->GetHostContentSettingsMap();
  ContentSetting setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_POPUPS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ASK);

  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_));
  EXPECT_CALL(*mock_ui(), SetFirstVisit(base::string16()));

  // SetPermissionInfo() is called once initially, and then again every time
  // OnSitePermissionChanged() is called.
// TODO(markusheintz): This is a temporary hack to fix issue: http://crbug.com/144203.
#if defined(OS_MACOSX)
  EXPECT_CALL(*mock_ui(), SetPermissionInfo(_)).Times(6);
#else
  EXPECT_CALL(*mock_ui(), SetPermissionInfo(_)).Times(1);
#endif
  EXPECT_CALL(*mock_ui(), SetSelectedTab(
      WebsiteSettingsUI::TAB_ID_PERMISSIONS));

  // Execute code under tests.
  website_settings()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_POPUPS,
                                              CONTENT_SETTING_ALLOW);
  website_settings()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_PLUGINS,
                                              CONTENT_SETTING_BLOCK);
  website_settings()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              CONTENT_SETTING_ALLOW);
  website_settings()->OnSitePermissionChanged(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  website_settings()->OnSitePermissionChanged(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM, CONTENT_SETTING_ALLOW);

  // Verify that the site permissions were changed correctly.
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_POPUPS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
}

TEST_F(WebsiteSettingsTest, OnSiteDataAccessed) {
  EXPECT_CALL(*mock_ui(), SetPermissionInfo(_));
  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetFirstVisit(base::string16()));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_)).Times(2);
  EXPECT_CALL(*mock_ui(), SetSelectedTab(
      WebsiteSettingsUI::TAB_ID_PERMISSIONS));

  website_settings()->OnSiteDataAccessed();
}

TEST_F(WebsiteSettingsTest, HTTPConnection) {
  SetDefaultUIExpectations(mock_ui());
  EXPECT_CALL(*mock_ui(), SetSelectedTab(
      WebsiteSettingsUI::TAB_ID_PERMISSIONS));
  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_UNENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_NO_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSConnection) {
  ssl_.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl_.cert_id = cert_id();
  ssl_.cert_status = 0;
  ssl_.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl_.connection_status = status;

  SetDefaultUIExpectations(mock_ui());
  EXPECT_CALL(*mock_ui(), SetSelectedTab(
      WebsiteSettingsUI::TAB_ID_PERMISSIONS));

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSMixedContent) {
  ssl_.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl_.cert_id = cert_id();
  ssl_.cert_status = 0;
  ssl_.security_bits = 81;  // No error if > 80.
  ssl_.content_status = SSLStatus::DISPLAYED_INSECURE_CONTENT;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl_.connection_status = status;

  SetDefaultUIExpectations(mock_ui());
  EXPECT_CALL(*mock_ui(), SetSelectedTab(WebsiteSettingsUI::TAB_ID_CONNECTION));

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_MIXED_CONTENT,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSEVCert) {
  scoped_refptr<net::X509Certificate> ev_cert =
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der),
          sizeof(google_der));
  int ev_cert_id = 1;
  EXPECT_CALL(*cert_store(), RetrieveCert(ev_cert_id, _)).WillRepeatedly(
      DoAll(SetArgPointee<1>(ev_cert), Return(true)));

  ssl_.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl_.cert_id = ev_cert_id;
  ssl_.cert_status = net::CERT_STATUS_IS_EV;
  ssl_.security_bits = 81;  // No error if > 80.
  ssl_.content_status = SSLStatus::DISPLAYED_INSECURE_CONTENT;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl_.connection_status = status;

  SetDefaultUIExpectations(mock_ui());
  EXPECT_CALL(*mock_ui(), SetSelectedTab(WebsiteSettingsUI::TAB_ID_CONNECTION));

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_MIXED_CONTENT,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::UTF8ToUTF16("Google Inc"),
            website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSRevocationError) {
  ssl_.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl_.cert_id = cert_id();
  ssl_.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  ssl_.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl_.connection_status = status;

  SetDefaultUIExpectations(mock_ui());
  EXPECT_CALL(*mock_ui(), SetSelectedTab(WebsiteSettingsUI::TAB_ID_CONNECTION));

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSConnectionError) {
  ssl_.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl_.cert_id = cert_id();
  ssl_.cert_status = 0;
  ssl_.security_bits = 1;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl_.connection_status = status;

  SetDefaultUIExpectations(mock_ui());
  EXPECT_CALL(*mock_ui(), SetSelectedTab(WebsiteSettingsUI::TAB_ID_CONNECTION));

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, NoInfoBar) {
  SetDefaultUIExpectations(mock_ui());
  EXPECT_CALL(*mock_ui(), SetSelectedTab(
      WebsiteSettingsUI::TAB_ID_PERMISSIONS));
  EXPECT_EQ(0u, infobar_service()->infobar_count());
  website_settings()->OnUIClosing();
  EXPECT_EQ(0u, infobar_service()->infobar_count());
}

TEST_F(WebsiteSettingsTest, ShowInfoBar) {
  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_));
  EXPECT_CALL(*mock_ui(), SetFirstVisit(base::string16()));

  // SetPermissionInfo() is called once initially, and then again every time
  // OnSitePermissionChanged() is called.
  // TODO(markusheintz): This is a temporary hack to fix issue:
  // http://crbug.com/144203.
#if defined(OS_MACOSX)
  EXPECT_CALL(*mock_ui(), SetPermissionInfo(_)).Times(2);
#else
  EXPECT_CALL(*mock_ui(), SetPermissionInfo(_)).Times(1);
#endif

  EXPECT_CALL(*mock_ui(), SetSelectedTab(
      WebsiteSettingsUI::TAB_ID_PERMISSIONS));
  EXPECT_EQ(0u, infobar_service()->infobar_count());
  website_settings()->OnSitePermissionChanged(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);
  website_settings()->OnUIClosing();
  ASSERT_EQ(1u, infobar_service()->infobar_count());

  infobar_service()->RemoveInfoBar(infobar_service()->infobar_at(0));
}
