// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings.h"

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/ssl_status.h"
#include "device/base/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "ppapi/features/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::SSLStatus;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
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

class MockWebsiteSettingsUI : public WebsiteSettingsUI {
 public:
  virtual ~MockWebsiteSettingsUI() {}
  MOCK_METHOD1(SetCookieInfo, void(const CookieInfoList& cookie_info_list));
  MOCK_METHOD0(SetPermissionInfoStub, void());
  MOCK_METHOD1(SetIdentityInfo, void(const IdentityInfo& identity_info));

  void SetPermissionInfo(
      const PermissionInfoList& permission_info_list,
      ChosenObjectInfoList chosen_object_info_list) override {
    SetPermissionInfoStub();
    if (set_permission_info_callback_) {
      set_permission_info_callback_.Run(permission_info_list,
                                        std::move(chosen_object_info_list));
    }
  }

  base::Callback<void(const PermissionInfoList& permission_info_list,
                      ChosenObjectInfoList chosen_object_info_list)>
      set_permission_info_callback_;
};

class WebsiteSettingsTest : public ChromeRenderViewHostTestHarness {
 public:
  WebsiteSettingsTest() : url_("http://www.example.com") {}

  ~WebsiteSettingsTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Setup stub SecurityInfo.
    security_info_.security_level = security_state::NONE;

    // Create the certificate.
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(cert_);

    TabSpecificContentSettings::CreateForWebContents(web_contents());
    InfoBarService::CreateForWebContents(web_contents());

    // Setup mock ui.
    mock_ui_.reset(new MockWebsiteSettingsUI());
    // Use this rather than gmock's ON_CALL.WillByDefault(Invoke(... because
    // gmock doesn't handle move-only types well.
    mock_ui_->set_permission_info_callback_ = base::Bind(
        &WebsiteSettingsTest::SetPermissionInfo, base::Unretained(this));
  }

  void TearDown() override {
    ASSERT_TRUE(website_settings_.get())
        << "No WebsiteSettings instance created.";
    RenderViewHostTestHarness::TearDown();
    website_settings_.reset();
  }

  void SetDefaultUIExpectations(MockWebsiteSettingsUI* mock_ui) {
    // During creation |WebsiteSettings| makes the following calls to the ui.
    EXPECT_CALL(*mock_ui, SetPermissionInfoStub());
    EXPECT_CALL(*mock_ui, SetIdentityInfo(_));
    EXPECT_CALL(*mock_ui, SetCookieInfo(_));
  }

  void SetURL(const std::string& url) { url_ = GURL(url); }

  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) {
    last_chosen_object_info_.clear();
    for (auto& chosen_object_info : chosen_object_info_list)
      last_chosen_object_info_.push_back(std::move(chosen_object_info));
  }

  void ResetMockUI() { mock_ui_.reset(new MockWebsiteSettingsUI()); }

  void ClearWebsiteSettings() { website_settings_.reset(nullptr); }

  const GURL& url() const { return url_; }
  scoped_refptr<net::X509Certificate> cert() { return cert_; }
  MockWebsiteSettingsUI* mock_ui() { return mock_ui_.get(); }
  const security_state::SecurityInfo& security_info() {
    return security_info_;
  }
  const std::vector<std::unique_ptr<WebsiteSettingsUI::ChosenObjectInfo>>&
  last_chosen_object_info() {
    return last_chosen_object_info_;
  }
  TabSpecificContentSettings* tab_specific_content_settings() {
    return TabSpecificContentSettings::FromWebContents(web_contents());
  }
  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }

  WebsiteSettings* website_settings() {
    if (!website_settings_.get()) {
      website_settings_.reset(new WebsiteSettings(
          mock_ui(), profile(), tab_specific_content_settings(), web_contents(),
          url(), security_info()));
    }
    return website_settings_.get();
  }

  device::MockUsbService& usb_service() {
    return *device_client_.usb_service();
  }

  security_state::SecurityInfo security_info_;

 private:
  device::MockDeviceClient device_client_;
  std::unique_ptr<WebsiteSettings> website_settings_;
  std::unique_ptr<MockWebsiteSettingsUI> mock_ui_;
  scoped_refptr<net::X509Certificate> cert_;
  GURL url_;
  std::vector<std::unique_ptr<WebsiteSettingsUI::ChosenObjectInfo>>
      last_chosen_object_info_;
};

}  // namespace

TEST_F(WebsiteSettingsTest, OnPermissionsChanged) {
  // Setup site permissions.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSetting setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_POPUPS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
#if BUILDFLAG(ENABLE_PLUGINS)
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);
#endif
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

  // SetPermissionInfo() is called once initially, and then again every time
  // OnSitePermissionChanged() is called.
#if !BUILDFLAG(ENABLE_PLUGINS)
  // SetPermissionInfo for plugins didn't get called.
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(6);
#else
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(7);
#endif

  // Execute code under tests.
  website_settings()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_POPUPS,
                                              CONTENT_SETTING_ALLOW);
#if BUILDFLAG(ENABLE_PLUGINS)
  website_settings()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_PLUGINS,
                                              CONTENT_SETTING_BLOCK);
#endif
  website_settings()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                              CONTENT_SETTING_ALLOW);
  website_settings()->OnSitePermissionChanged(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  website_settings()->OnSitePermissionChanged(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, CONTENT_SETTING_ALLOW);
  website_settings()->OnSitePermissionChanged(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  // Verify that the site permissions were changed correctly.
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_POPUPS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
#if BUILDFLAG(ENABLE_PLUGINS)
  setting = content_settings->GetContentSetting(
      url(), url(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string());
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
#endif
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
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub());
  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_)).Times(2);

  website_settings()->OnSiteDataAccessed();
}

TEST_F(WebsiteSettingsTest, OnChosenObjectDeleted) {
  scoped_refptr<device::UsbDevice> device =
      new device::MockUsbDevice(0, 0, "Google", "Gizmo", "1234567890");
  usb_service().AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());
  store->GrantDevicePermission(url(), url(), device->guid());

  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_));

  // Access WebsiteSettings so that SetPermissionInfo is called once to populate
  // |last_chosen_object_info_|. It will be called again by
  // OnSiteChosenObjectDeleted.
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(2);
  website_settings();

  ASSERT_EQ(1u, last_chosen_object_info().size());
  const WebsiteSettingsUI::ChosenObjectInfo* info =
      last_chosen_object_info()[0].get();
  website_settings()->OnSiteChosenObjectDeleted(info->ui_info, *info->object);

  EXPECT_FALSE(store->HasDevicePermission(url(), url(), device));
  EXPECT_EQ(0u, last_chosen_object_info().size());
}

TEST_F(WebsiteSettingsTest, Malware) {
  security_info_.security_level = security_state::DANGEROUS;
  security_info_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MALWARE;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_UNENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_MALWARE,
            website_settings()->site_identity_status());
}

TEST_F(WebsiteSettingsTest, SocialEngineering) {
  security_info_.security_level = security_state::DANGEROUS;
  security_info_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_UNENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_SOCIAL_ENGINEERING,
            website_settings()->site_identity_status());
}

TEST_F(WebsiteSettingsTest, UnwantedSoftware) {
  security_info_.security_level = security_state::DANGEROUS;
  security_info_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_UNWANTED_SOFTWARE;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_UNENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_UNWANTED_SOFTWARE,
            website_settings()->site_identity_status());
}

TEST_F(WebsiteSettingsTest, HTTPConnection) {
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_UNENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_NO_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSConnection) {
  security_info_.security_level = security_state::SECURE;
  security_info_.scheme_is_cryptographic = true;
  security_info_.certificate = cert();
  security_info_.cert_status = 0;
  security_info_.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  security_info_.connection_status = status;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, InsecureContent) {
  struct TestCase {
    security_state::SecurityLevel security_level;
    net::CertStatus cert_status;
    security_state::ContentStatus mixed_content_status;
    security_state::ContentStatus content_with_cert_errors_status;
    WebsiteSettings::SiteConnectionStatus expected_site_connection_status;
    WebsiteSettings::SiteIdentityStatus expected_site_identity_status;
    int expected_connection_icon_id;
  };

  const TestCase kTestCases[] = {
      // Passive mixed content.
      {security_state::NONE, 0, security_state::CONTENT_STATUS_DISPLAYED,
       security_state::CONTENT_STATUS_NONE,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content with a cert error on the main resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_DISPLAYED,
       security_state::CONTENT_STATUS_NONE,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_WARNING_MINOR},
      // Active and passive mixed content.
      {security_state::DANGEROUS, 0,
       security_state::CONTENT_STATUS_DISPLAYED_AND_RAN,
       security_state::CONTENT_STATUS_NONE,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active and passive mixed content with a cert error on the main
      // resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_DISPLAYED_AND_RAN,
       security_state::CONTENT_STATUS_NONE,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_BAD},
      // Active mixed content.
      {security_state::DANGEROUS, 0, security_state::CONTENT_STATUS_RAN,
       security_state::CONTENT_STATUS_NONE,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active mixed content with a cert error on the main resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_RAN, security_state::CONTENT_STATUS_NONE,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_BAD},

      // Passive subresources with cert errors.
      {security_state::NONE, 0, security_state::CONTENT_STATUS_NONE,
       security_state::CONTENT_STATUS_DISPLAYED,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive subresources with cert errors, with a cert error on the
      // main resource also. In this case, the subresources with
      // certificate errors are ignored: if the main resource had a cert
      // error, it's not that useful to warn about subresources with cert
      // errors as well.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_NONE,
       security_state::CONTENT_STATUS_DISPLAYED,
       WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
       WebsiteSettings::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_GOOD},
      // Passive and active subresources with cert errors.
      {security_state::DANGEROUS, 0, security_state::CONTENT_STATUS_NONE,
       security_state::CONTENT_STATUS_DISPLAYED_AND_RAN,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive and active subresources with cert errors, with a cert
      // error on the main resource also.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_NONE,
       security_state::CONTENT_STATUS_DISPLAYED_AND_RAN,
       WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
       WebsiteSettings::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_GOOD},
      // Active subresources with cert errors.
      {security_state::DANGEROUS, 0, security_state::CONTENT_STATUS_NONE,
       security_state::CONTENT_STATUS_RAN,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active subresources with cert errors, with a cert error on the main
      // resource also.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_NONE, security_state::CONTENT_STATUS_RAN,
       WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
       WebsiteSettings::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_GOOD},

      // Passive mixed content and subresources with cert errors.
      {security_state::NONE, 0, security_state::CONTENT_STATUS_DISPLAYED,
       security_state::CONTENT_STATUS_DISPLAYED,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content and active subresources with cert errors.
      {security_state::DANGEROUS, 0, security_state::CONTENT_STATUS_DISPLAYED,
       security_state::CONTENT_STATUS_RAN,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active mixed content and passive subresources with cert errors.
      {security_state::DANGEROUS, 0, security_state::CONTENT_STATUS_RAN,
       security_state::CONTENT_STATUS_DISPLAYED,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive mixed content, active subresources with cert errors, and a cert
      // error on the main resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_DISPLAYED,
       security_state::CONTENT_STATUS_RAN,
       WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       WebsiteSettings::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_WARNING_MINOR},
  };

  for (const auto& test : kTestCases) {
    ResetMockUI();
    ClearWebsiteSettings();
    security_info_ = security_state::SecurityInfo();
    security_info_.security_level = test.security_level;
    security_info_.scheme_is_cryptographic = true;
    security_info_.certificate = cert();
    security_info_.cert_status = test.cert_status;
    security_info_.security_bits = 81;  // No error if > 80.
    security_info_.mixed_content_status = test.mixed_content_status;
    security_info_.content_with_cert_errors_status =
        test.content_with_cert_errors_status;
    int status = 0;
    status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
    status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
    security_info_.connection_status = status;

    SetDefaultUIExpectations(mock_ui());

    EXPECT_EQ(test.expected_site_connection_status,
              website_settings()->site_connection_status());
    EXPECT_EQ(test.expected_site_identity_status,
              website_settings()->site_identity_status());
    EXPECT_EQ(test.expected_connection_icon_id,
              WebsiteSettingsUI::GetConnectionIconID(
                  website_settings()->site_connection_status()));
    EXPECT_EQ(base::string16(), website_settings()->organization_name());
  }
}

TEST_F(WebsiteSettingsTest, HTTPSEVCert) {
  scoped_refptr<net::X509Certificate> ev_cert =
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der),
          sizeof(google_der));

  security_info_.security_level = security_state::NONE;
  security_info_.scheme_is_cryptographic = true;
  security_info_.certificate = ev_cert;
  security_info_.cert_status = net::CERT_STATUS_IS_EV;
  security_info_.security_bits = 81;  // No error if > 80.
  security_info_.mixed_content_status =
      security_state::CONTENT_STATUS_DISPLAYED;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  security_info_.connection_status = status;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(
      WebsiteSettings::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
      website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::UTF8ToUTF16("Google Inc"),
            website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSRevocationError) {
  security_info_.security_level = security_state::SECURE;
  security_info_.scheme_is_cryptographic = true;
  security_info_.certificate = cert();
  security_info_.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info_.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  security_info_.connection_status = status;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSConnectionError) {
  security_info_.security_level = security_state::SECURE;
  security_info_.scheme_is_cryptographic = true;
  security_info_.certificate = cert();
  security_info_.cert_status = 0;
  security_info_.security_bits = -1;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  security_info_.connection_status = status;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSPolicyCertConnection) {
  security_info_.security_level =
      security_state::SECURE_WITH_POLICY_INSTALLED_CERT;
  security_info_.scheme_is_cryptographic = true;
  security_info_.certificate = cert();
  security_info_.cert_status = 0;
  security_info_.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  security_info_.connection_status = status;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

TEST_F(WebsiteSettingsTest, HTTPSSHA1) {
  security_info_.security_level = security_state::NONE;
  security_info_.scheme_is_cryptographic = true;
  security_info_.certificate = cert();
  security_info_.cert_status = 0;
  security_info_.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
  security_info_.connection_status = status;
  security_info_.sha1_in_chain = true;

  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(
      WebsiteSettings::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM,
      website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
  EXPECT_EQ(IDR_PAGEINFO_WARNING_MINOR,
            WebsiteSettingsUI::GetIdentityIconID(
                website_settings()->site_identity_status()));
}

#if !defined(OS_ANDROID)
TEST_F(WebsiteSettingsTest, NoInfoBar) {
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(0u, infobar_service()->infobar_count());
  website_settings()->OnUIClosing();
  EXPECT_EQ(0u, infobar_service()->infobar_count());
}

TEST_F(WebsiteSettingsTest, ShowInfoBar) {
  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_));

  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(2);

  EXPECT_EQ(0u, infobar_service()->infobar_count());
  website_settings()->OnSitePermissionChanged(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);
  website_settings()->OnUIClosing();
  ASSERT_EQ(1u, infobar_service()->infobar_count());

  infobar_service()->RemoveInfoBar(infobar_service()->infobar_at(0));
}
#endif

TEST_F(WebsiteSettingsTest, AboutBlankPage) {
  SetURL("about:blank");
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_UNENCRYPTED,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_NO_CERT,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}

// On desktop, internal URLs aren't handled by WebsiteSettings class. Instead, a
// custom and simpler popup is shown, so no need to test.
#if defined(OS_ANDROID) || defined(OS_IOS)
TEST_F(WebsiteSettingsTest, InternalPage) {
  SetURL("chrome://bookmarks");
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(WebsiteSettings::SITE_CONNECTION_STATUS_INTERNAL_PAGE,
            website_settings()->site_connection_status());
  EXPECT_EQ(WebsiteSettings::SITE_IDENTITY_STATUS_INTERNAL_PAGE,
            website_settings()->site_identity_status());
  EXPECT_EQ(base::string16(), website_settings()->organization_name());
}
#endif

// Tests that metrics are recorded on a WebsiteSettings for pages with
// various security levels.
TEST_F(WebsiteSettingsTest, SecurityLevelMetrics) {
  struct TestCase {
    const std::string url;
    const security_state::SecurityLevel security_level;
    const std::string histogram_name;
  };
  const char kGenericHistogram[] = "WebsiteSettings.Action";

  const TestCase kTestCases[] = {
      {"https://example.test", security_state::SECURE,
       "Security.PageInfo.Action.HttpsUrl.Valid"},
      {"https://example.test", security_state::EV_SECURE,
       "Security.PageInfo.Action.HttpsUrl.Valid"},
      {"https://example2.test", security_state::NONE,
       "Security.PageInfo.Action.HttpsUrl.Downgraded"},
      {"https://example.test", security_state::DANGEROUS,
       "Security.PageInfo.Action.HttpsUrl.Dangerous"},
      {"http://example.test", security_state::HTTP_SHOW_WARNING,
       "Security.PageInfo.Action.HttpUrl.Warning"},
      {"http://example.test", security_state::DANGEROUS,
       "Security.PageInfo.Action.HttpUrl.Dangerous"},
      {"http://example.test", security_state::NONE,
       "Security.PageInfo.Action.HttpUrl.Neutral"},
  };

  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    SetURL(test.url);
    security_info_.security_level = test.security_level;
    ResetMockUI();
    ClearWebsiteSettings();
    SetDefaultUIExpectations(mock_ui());

    histograms.ExpectTotalCount(kGenericHistogram, 0);
    histograms.ExpectTotalCount(test.histogram_name, 0);

    website_settings()->RecordWebsiteSettingsAction(
        WebsiteSettings::WebsiteSettingsAction::
            WEBSITE_SETTINGS_OPENED);

    // RecordWebsiteSettingsAction() is called during WebsiteSettings
    // creation in addition to the explicit RecordWebsiteSettingsAction()
    // call, so it is called twice in total.
    histograms.ExpectTotalCount(kGenericHistogram, 2);
    histograms.ExpectBucketCount(
        kGenericHistogram,
        WebsiteSettings::WebsiteSettingsAction::WEBSITE_SETTINGS_OPENED, 2);

    histograms.ExpectTotalCount(test.histogram_name, 2);
    histograms.ExpectBucketCount(
        test.histogram_name,
        WebsiteSettings::WebsiteSettingsAction::WEBSITE_SETTINGS_OPENED, 2);
  }
}
