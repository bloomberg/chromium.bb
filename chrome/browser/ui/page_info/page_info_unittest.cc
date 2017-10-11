// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/page_info.h"

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/infobar.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/content_switches.h"
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

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#endif

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

class MockPageInfoUI : public PageInfoUI {
 public:
  virtual ~MockPageInfoUI() {}
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

class PageInfoTest : public ChromeRenderViewHostTestHarness {
 public:
  PageInfoTest() : url_("http://www.example.com") {}

  ~PageInfoTest() override {}

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
    ResetMockUI();
  }

  void TearDown() override {
    ASSERT_TRUE(page_info_.get()) << "No PageInfo instance created.";
    RenderViewHostTestHarness::TearDown();
    page_info_.reset();
  }

  void SetDefaultUIExpectations(MockPageInfoUI* mock_ui) {
    // During creation |PageInfo| makes the following calls to the ui.
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
    last_permission_info_list_ = permission_info_list;
  }

  void ResetMockUI() {
    mock_ui_.reset(new MockPageInfoUI());
    // Use this rather than gmock's ON_CALL.WillByDefault(Invoke(... because
    // gmock doesn't handle move-only types well.
    mock_ui_->set_permission_info_callback_ =
        base::Bind(&PageInfoTest::SetPermissionInfo, base::Unretained(this));
  }


  void ClearPageInfo() { page_info_.reset(nullptr); }

  const GURL& url() const { return url_; }
  scoped_refptr<net::X509Certificate> cert() { return cert_; }
  MockPageInfoUI* mock_ui() { return mock_ui_.get(); }
  const security_state::SecurityInfo& security_info() { return security_info_; }
  const std::vector<std::unique_ptr<PageInfoUI::ChosenObjectInfo>>&
  last_chosen_object_info() {
    return last_chosen_object_info_;
  }
  const PermissionInfoList& last_permission_info_list() {
    return last_permission_info_list_;
  }
  TabSpecificContentSettings* tab_specific_content_settings() {
    return TabSpecificContentSettings::FromWebContents(web_contents());
  }
  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }

  PageInfo* page_info() {
    if (!page_info_.get()) {
      page_info_.reset(new PageInfo(mock_ui(), profile(),
                                    tab_specific_content_settings(),
                                    web_contents(), url(), security_info()));
    }
    return page_info_.get();
  }

  device::MockUsbService& usb_service() {
    return *device_client_.usb_service();
  }

  security_state::SecurityInfo security_info_;

 private:
  device::MockDeviceClient device_client_;
  std::unique_ptr<PageInfo> page_info_;
  std::unique_ptr<MockPageInfoUI> mock_ui_;
  scoped_refptr<net::X509Certificate> cert_;
  GURL url_;
  std::vector<std::unique_ptr<PageInfoUI::ChosenObjectInfo>>
      last_chosen_object_info_;
  PermissionInfoList last_permission_info_list_;
};

bool PermissionInfoListContainsPermission(const PermissionInfoList& permissions,
                                          ContentSettingsType content_type) {
  for (const auto& permission : permissions) {
    if (permission.type == content_type)
      return true;
  }
  return false;
}

}  // namespace

TEST_F(PageInfoTest, NonFactoryDefaultPermissionsShown) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSiteDetails);

  page_info()->PresentSitePermissions();
// By default, the number of permissions shown should be 0, except on Android,
// where Geolocation needs to be checked for DSE settings.
#if defined(OS_ANDROID)
  EXPECT_EQ(1uL, last_permission_info_list().size());
#else
  EXPECT_EQ(0uL, last_permission_info_list().size());
#endif

  std::vector<ContentSettingsType> non_default_permissions = {
      CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
  };
  // Change some default-ask settings away from the default.
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                       CONTENT_SETTING_ALLOW);
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                       CONTENT_SETTING_ALLOW);
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                                       CONTENT_SETTING_ALLOW);
  EXPECT_EQ(non_default_permissions.size(), last_permission_info_list().size());

  non_default_permissions.push_back(CONTENT_SETTINGS_TYPE_POPUPS);
  // Change a default-block setting to a user-preference block instead.
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_POPUPS,
                                       CONTENT_SETTING_BLOCK);
  EXPECT_EQ(non_default_permissions.size(), last_permission_info_list().size());

  non_default_permissions.push_back(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
  // Change a default-allow setting away from the default.
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                       CONTENT_SETTING_BLOCK);
  EXPECT_EQ(non_default_permissions.size(), last_permission_info_list().size());

  // Make sure setting a default setting to the default doesn't do anything.
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(non_default_permissions.size(), last_permission_info_list().size());

  non_default_permissions.pop_back();
  // Clear the Javascript setting.
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                       CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(non_default_permissions.size(), last_permission_info_list().size());

  non_default_permissions.push_back(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
  // Change the default setting for Javascript away from the factory default.
  page_info()->content_settings_->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  page_info()->PresentSitePermissions();
  EXPECT_EQ(non_default_permissions.size(), last_permission_info_list().size());

  // Change it back to ALLOW, which is its factory default, but has a source
  // from the user preference (i.e. it counts as non-factory default).
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                       CONTENT_SETTING_ALLOW);
  EXPECT_EQ(non_default_permissions.size(), last_permission_info_list().size());

  // Sanity check the correct permissions are being shown.
  for (ContentSettingsType type : non_default_permissions) {
    EXPECT_TRUE(PermissionInfoListContainsPermission(
        last_permission_info_list(), type));
  }
}

TEST_F(PageInfoTest, OnPermissionsChanged) {
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
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_POPUPS,
                                       CONTENT_SETTING_ALLOW);
#if BUILDFLAG(ENABLE_PLUGINS)
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_PLUGINS,
                                       CONTENT_SETTING_BLOCK);
#endif
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                       CONTENT_SETTING_ALLOW);
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                       CONTENT_SETTING_ALLOW);
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                                       CONTENT_SETTING_ALLOW);
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                                       CONTENT_SETTING_ALLOW);

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

TEST_F(PageInfoTest, OnSiteDataAccessed) {
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub());
  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_)).Times(2);

  page_info()->OnSiteDataAccessed();
}

TEST_F(PageInfoTest, OnChosenObjectDeleted) {
  scoped_refptr<device::UsbDevice> device =
      new device::MockUsbDevice(0, 0, "Google", "Gizmo", "1234567890");
  usb_service().AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());
  store->GrantDevicePermission(url(), url(), device->guid());

  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_));

  // Access PageInfo so that SetPermissionInfo is called once to populate
  // |last_chosen_object_info_|. It will be called again by
  // OnSiteChosenObjectDeleted.
  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(2);
  page_info();

  ASSERT_EQ(1u, last_chosen_object_info().size());
  const PageInfoUI::ChosenObjectInfo* info = last_chosen_object_info()[0].get();
  page_info()->OnSiteChosenObjectDeleted(info->ui_info, *info->object);

  EXPECT_FALSE(store->HasDevicePermission(url(), url(), device));
  EXPECT_EQ(0u, last_chosen_object_info().size());
}

TEST_F(PageInfoTest, Malware) {
  security_info_.security_level = security_state::DANGEROUS;
  security_info_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MALWARE;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_MALWARE,
            page_info()->site_identity_status());
}

TEST_F(PageInfoTest, SocialEngineering) {
  security_info_.security_level = security_state::DANGEROUS;
  security_info_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_SOCIAL_ENGINEERING,
            page_info()->site_identity_status());
}

TEST_F(PageInfoTest, UnwantedSoftware) {
  security_info_.security_level = security_state::DANGEROUS;
  security_info_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_UNWANTED_SOFTWARE;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_UNWANTED_SOFTWARE,
            page_info()->site_identity_status());
}

#if defined(SAFE_BROWSING_DB_LOCAL)
TEST_F(PageInfoTest, PasswordReuse) {
  security_info_.security_level = security_state::DANGEROUS;
  security_info_.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_PASSWORD_REUSE;
  SetDefaultUIExpectations(mock_ui());

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_PASSWORD_REUSE,
            page_info()->site_identity_status());
}
#endif

TEST_F(PageInfoTest, HTTPConnection) {
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_NO_CERT,
            page_info()->site_identity_status());
  EXPECT_EQ(base::string16(), page_info()->organization_name());
}

TEST_F(PageInfoTest, HTTPSConnection) {
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

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_CERT,
            page_info()->site_identity_status());
  EXPECT_EQ(base::string16(), page_info()->organization_name());
}

// Define some dummy constants for Android-only resources.
#if !defined(OS_ANDROID)
#define IDR_PAGEINFO_WARNING_MINOR 0
#define IDR_PAGEINFO_BAD 0
#define IDR_PAGEINFO_GOOD 0
#endif

TEST_F(PageInfoTest, InsecureContent) {
  struct TestCase {
    security_state::SecurityLevel security_level;
    net::CertStatus cert_status;
    security_state::ContentStatus mixed_content_status;
    bool contained_mixed_form;
    security_state::ContentStatus content_with_cert_errors_status;
    PageInfo::SiteConnectionStatus expected_site_connection_status;
    PageInfo::SiteIdentityStatus expected_site_identity_status;
    int expected_connection_icon_id;
  };

  const TestCase kTestCases[] = {
      // Passive mixed content.
      {security_state::NONE, 0, security_state::CONTENT_STATUS_DISPLAYED, false,
       security_state::CONTENT_STATUS_NONE,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content with a nonsecure form. The nonsecure form is the
      // more severe problem.
      {security_state::NONE, 0, security_state::CONTENT_STATUS_DISPLAYED, true,
       security_state::CONTENT_STATUS_NONE,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Only nonsecure form.
      {security_state::NONE, 0, security_state::CONTENT_STATUS_NONE, true,
       security_state::CONTENT_STATUS_NONE,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content with a cert error on the main resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_DISPLAYED, false,
       security_state::CONTENT_STATUS_NONE,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_WARNING_MINOR},
      // Active and passive mixed content.
      {security_state::DANGEROUS, 0,
       security_state::CONTENT_STATUS_DISPLAYED_AND_RAN, false,
       security_state::CONTENT_STATUS_NONE,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active mixed content and nonsecure form.
      {security_state::DANGEROUS, 0,
       security_state::CONTENT_STATUS_DISPLAYED_AND_RAN, true,
       security_state::CONTENT_STATUS_NONE,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active and passive mixed content with a cert error on the main
      // resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_DISPLAYED_AND_RAN, false,
       security_state::CONTENT_STATUS_NONE,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_BAD},
      // Active mixed content.
      {security_state::DANGEROUS, 0, security_state::CONTENT_STATUS_RAN, false,
       security_state::CONTENT_STATUS_NONE,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active mixed content with a cert error on the main resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_RAN, false,
       security_state::CONTENT_STATUS_NONE,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_BAD},

      // Passive subresources with cert errors.
      {security_state::NONE, 0, security_state::CONTENT_STATUS_NONE, false,
       security_state::CONTENT_STATUS_DISPLAYED,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive subresources with cert errors, with a cert error on the
      // main resource also. In this case, the subresources with
      // certificate errors are ignored: if the main resource had a cert
      // error, it's not that useful to warn about subresources with cert
      // errors as well.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_NONE, false,
       security_state::CONTENT_STATUS_DISPLAYED,
       PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_GOOD},
      // Passive and active subresources with cert errors.
      {security_state::DANGEROUS, 0, security_state::CONTENT_STATUS_NONE, false,
       security_state::CONTENT_STATUS_DISPLAYED_AND_RAN,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive and active subresources with cert errors, with a cert
      // error on the main resource also.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_NONE, false,
       security_state::CONTENT_STATUS_DISPLAYED_AND_RAN,
       PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_GOOD},
      // Active subresources with cert errors.
      {security_state::DANGEROUS, 0, security_state::CONTENT_STATUS_NONE, false,
       security_state::CONTENT_STATUS_RAN,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active subresources with cert errors, with a cert error on the main
      // resource also.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_NONE, false,
       security_state::CONTENT_STATUS_RAN,
       PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_GOOD},

      // Passive mixed content and subresources with cert errors.
      {security_state::NONE, 0, security_state::CONTENT_STATUS_DISPLAYED, false,
       security_state::CONTENT_STATUS_DISPLAYED,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content and subresources with cert errors.
      {security_state::NONE, 0, security_state::CONTENT_STATUS_DISPLAYED, false,
       security_state::CONTENT_STATUS_DISPLAYED,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content, a nonsecure form, and subresources with cert
      // errors.
      {security_state::NONE, 0, security_state::CONTENT_STATUS_DISPLAYED, true,
       security_state::CONTENT_STATUS_DISPLAYED,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_WARNING_MINOR},
      // Passive mixed content and active subresources with cert errors.
      {security_state::DANGEROUS, 0, security_state::CONTENT_STATUS_DISPLAYED,
       false, security_state::CONTENT_STATUS_RAN,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Active mixed content and passive subresources with cert errors.
      {security_state::DANGEROUS, 0, security_state::CONTENT_STATUS_RAN, false,
       security_state::CONTENT_STATUS_DISPLAYED,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_ACTIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_CERT, IDR_PAGEINFO_BAD},
      // Passive mixed content, active subresources with cert errors, and a cert
      // error on the main resource.
      {security_state::DANGEROUS, net::CERT_STATUS_DATE_INVALID,
       security_state::CONTENT_STATUS_DISPLAYED, false,
       security_state::CONTENT_STATUS_RAN,
       PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
       PageInfo::SITE_IDENTITY_STATUS_ERROR, IDR_PAGEINFO_WARNING_MINOR},
  };

  for (const auto& test : kTestCases) {
    ResetMockUI();
    ClearPageInfo();
    security_info_ = security_state::SecurityInfo();
    security_info_.security_level = test.security_level;
    security_info_.scheme_is_cryptographic = true;
    security_info_.certificate = cert();
    security_info_.cert_status = test.cert_status;
    security_info_.security_bits = 81;  // No error if > 80.
    security_info_.mixed_content_status = test.mixed_content_status;
    security_info_.contained_mixed_form = test.contained_mixed_form;
    security_info_.content_with_cert_errors_status =
        test.content_with_cert_errors_status;
    int status = 0;
    status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
    status = SetSSLCipherSuite(status, CR_TLS_RSA_WITH_AES_256_CBC_SHA256);
    security_info_.connection_status = status;

    SetDefaultUIExpectations(mock_ui());

    EXPECT_EQ(test.expected_site_connection_status,
              page_info()->site_connection_status());
    EXPECT_EQ(test.expected_site_identity_status,
              page_info()->site_identity_status());
#if defined(OS_ANDROID)
    EXPECT_EQ(
        test.expected_connection_icon_id,
        PageInfoUI::GetConnectionIconID(page_info()->site_connection_status()));
#endif
    EXPECT_EQ(base::string16(), page_info()->organization_name());
  }
}

TEST_F(PageInfoTest, HTTPSEVCert) {
  scoped_refptr<net::X509Certificate> ev_cert =
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der));
  ASSERT_TRUE(ev_cert);

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

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_EV_CERT,
            page_info()->site_identity_status());
  EXPECT_EQ(base::UTF8ToUTF16("Google Inc"), page_info()->organization_name());
}

TEST_F(PageInfoTest, HTTPSRevocationError) {
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

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN,
            page_info()->site_identity_status());
  EXPECT_EQ(base::string16(), page_info()->organization_name());
}

TEST_F(PageInfoTest, HTTPSConnectionError) {
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

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_CERT,
            page_info()->site_identity_status());
  EXPECT_EQ(base::string16(), page_info()->organization_name());
}

TEST_F(PageInfoTest, HTTPSPolicyCertConnection) {
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

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT,
            page_info()->site_identity_status());
  EXPECT_EQ(base::string16(), page_info()->organization_name());
}

TEST_F(PageInfoTest, HTTPSSHA1) {
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

  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM,
            page_info()->site_identity_status());
  EXPECT_EQ(base::string16(), page_info()->organization_name());
#if defined(OS_ANDROID)
  EXPECT_EQ(IDR_PAGEINFO_WARNING_MINOR,
            PageInfoUI::GetIdentityIconID(page_info()->site_identity_status()));
#endif
}

#if !defined(OS_ANDROID)
TEST_F(PageInfoTest, NoInfoBar) {
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(0u, infobar_service()->infobar_count());
  page_info()->OnUIClosing();
  EXPECT_EQ(0u, infobar_service()->infobar_count());
}

TEST_F(PageInfoTest, ShowInfoBar) {
  EXPECT_CALL(*mock_ui(), SetIdentityInfo(_));
  EXPECT_CALL(*mock_ui(), SetCookieInfo(_));

  EXPECT_CALL(*mock_ui(), SetPermissionInfoStub()).Times(2);

  EXPECT_EQ(0u, infobar_service()->infobar_count());
  page_info()->OnSitePermissionChanged(CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                       CONTENT_SETTING_ALLOW);
  page_info()->OnUIClosing();
  ASSERT_EQ(1u, infobar_service()->infobar_count());

  infobar_service()->RemoveInfoBar(infobar_service()->infobar_at(0));
}
#endif

TEST_F(PageInfoTest, AboutBlankPage) {
  SetURL(url::kAboutBlankURL);
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_UNENCRYPTED,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_NO_CERT,
            page_info()->site_identity_status());
  EXPECT_EQ(base::string16(), page_info()->organization_name());
}

// On desktop, internal URLs aren't handled by PageInfo class. Instead, a
// custom and simpler bubble is shown, so no need to test.
#if defined(OS_ANDROID)
TEST_F(PageInfoTest, InternalPage) {
  SetURL("chrome://bookmarks");
  SetDefaultUIExpectations(mock_ui());
  EXPECT_EQ(PageInfo::SITE_CONNECTION_STATUS_INTERNAL_PAGE,
            page_info()->site_connection_status());
  EXPECT_EQ(PageInfo::SITE_IDENTITY_STATUS_INTERNAL_PAGE,
            page_info()->site_identity_status());
  EXPECT_EQ(base::string16(), page_info()->organization_name());
}
#endif

// Tests that metrics for the "Re-Enable Warnings" button on PageInfo are being
// logged correctly.
TEST_F(PageInfoTest, ReEnableWarningsMetrics) {
  struct TestCase {
    const std::string url;
    const bool button_visible;
    const bool button_clicked;
  };

  const TestCase kTestCases[] = {
      {"https://example.test", false, false},
      {"https://example.test", true, false},
      {"https://example.test", true, true},
  };
  const char kGenericHistogram[] =
      "interstitial.ssl.did_user_revoke_decisions2";
  for (const auto& test : kTestCases) {
    base::HistogramTester histograms;
    ResetMockUI();
    SetURL(test.url);
    if (test.button_visible) {
      // In the case where the button should be visible, add an exception to
      // the profile settings for the site (since the exception is what
      // will make the button visible).
      HostContentSettingsMap* content_settings =
          HostContentSettingsMapFactory::GetForProfile(profile());
      std::unique_ptr<base::DictionaryValue> dict =
          std::unique_ptr<base::DictionaryValue>(new base::DictionaryValue());
      dict->SetKey(
          "testkey",
          base::Value(content::SSLHostStateDelegate::CertJudgment::ALLOWED));
      content_settings->SetWebsiteSettingDefaultScope(
          url(), GURL(), CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
          std::string(), std::move(dict));
      page_info();
      if (test.button_clicked) {
        page_info()->OnRevokeSSLErrorBypassButtonPressed();
        ClearPageInfo();
        histograms.ExpectTotalCount(kGenericHistogram, 1);
        histograms.ExpectBucketCount(
            kGenericHistogram,
            PageInfo::SSLCertificateDecisionsDidRevoke::
                USER_CERT_DECISIONS_REVOKED,
            1);
      } else {  // Case where button is visible but not clicked.
        ClearPageInfo();
        histograms.ExpectTotalCount(kGenericHistogram, 1);
        histograms.ExpectBucketCount(
            kGenericHistogram,
            PageInfo::SSLCertificateDecisionsDidRevoke::
                USER_CERT_DECISIONS_NOT_REVOKED,
            1);
      }
    } else {
      page_info();
      ClearPageInfo();
      // Button is not visible, so check histogram is empty after opening and
      // closing page info.
      histograms.ExpectTotalCount(kGenericHistogram, 0);
    }
  }
  // Test class expects PageInfo to exist during Teardown.
  page_info();
}

// Tests that metrics are recorded on a PageInfo for pages with
// various security levels.
TEST_F(PageInfoTest, SecurityLevelMetrics) {
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
    ClearPageInfo();
    SetDefaultUIExpectations(mock_ui());

    histograms.ExpectTotalCount(kGenericHistogram, 0);
    histograms.ExpectTotalCount(test.histogram_name, 0);

    page_info()->RecordPageInfoAction(
        PageInfo::PageInfoAction::PAGE_INFO_OPENED);

    // RecordPageInfoAction() is called during PageInfo
    // creation in addition to the explicit RecordPageInfoAction()
    // call, so it is called twice in total.
    histograms.ExpectTotalCount(kGenericHistogram, 2);
    histograms.ExpectBucketCount(kGenericHistogram,
                                 PageInfo::PageInfoAction::PAGE_INFO_OPENED, 2);

    histograms.ExpectTotalCount(test.histogram_name, 2);
    histograms.ExpectBucketCount(test.histogram_name,
                                 PageInfo::PageInfoAction::PAGE_INFO_OPENED, 2);
  }
}

// Tests that the SubresourceFilter setting is omitted correctly.
TEST_F(PageInfoTest, SubresourceFilterSetting_MatchesActivation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      subresource_filter::kSafeBrowsingSubresourceFilterExperimentalUI);
  auto showing_setting = [](const PermissionInfoList& permissions) {
    return PermissionInfoListContainsPermission(permissions,
                                                CONTENT_SETTINGS_TYPE_ADS);
  };

  // By default, the setting should not appear at all.
  SetURL("https://example.test/");
  SetDefaultUIExpectations(mock_ui());
  page_info();
  EXPECT_FALSE(showing_setting(last_permission_info_list()));

  // Reset state.
  ResetMockUI();
  ClearPageInfo();
  SetDefaultUIExpectations(mock_ui());

  // Now, simulate activation on that origin, which is encoded by the existence
  // of the website setting. The setting should then appear in page_info.
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings->SetWebsiteSettingDefaultScope(
      url(), GURL(), CONTENT_SETTINGS_TYPE_ADS_DATA, std::string(),
      base::MakeUnique<base::DictionaryValue>());
  page_info();
  EXPECT_TRUE(showing_setting(last_permission_info_list()));
}
