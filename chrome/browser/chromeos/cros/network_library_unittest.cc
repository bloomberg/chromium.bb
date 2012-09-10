// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cert.h>
#include <pk11pub.h>

#include <vector>
#include <string>

#include "base/at_exit.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/onc_network_parser.h"
#include "chrome/common/chrome_paths.h"
#include "crypto/nss_util.h"
#include "net/base/crypto_module.h"
#include "net/base/nss_cert_database.h"
#include "net/base/x509_certificate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace chromeos {

namespace {

// Have to do a stub here because MOCK can't handle closure arguments.
class StubEnrollmentDelegate : public EnrollmentDelegate {
 public:
  explicit StubEnrollmentDelegate(OncNetworkParser* parser)
    : did_enroll(false), correct_args(false), parser_(parser) {}

  void Enroll(const std::vector<std::string>& uri_list,
              const base::Closure& closure) {
    std::vector<std::string> expected_uri_list;
    expected_uri_list.push_back("http://youtu.be/dQw4w9WgXcQ");
    expected_uri_list.push_back("chrome-extension://abc/keygen-cert.html");
    if (uri_list == expected_uri_list)
      correct_args = true;

    scoped_refptr<net::X509Certificate> certificate0(
        parser_->ParseCertificate(0));
    scoped_refptr<net::X509Certificate> certificate1(
        parser_->ParseCertificate(1));

    if (certificate0.get() && certificate1.get()) {
      did_enroll = true;
      closure.Run();
    }
  }

  bool did_enroll;
  bool correct_args;
  OncNetworkParser* parser_;
};

void WifiNetworkConnectCallback(NetworkLibrary* cros, WifiNetwork* wifi) {
  cros->ConnectToWifiNetwork(wifi);
}

void VirtualNetworkConnectCallback(NetworkLibrary* cros, VirtualNetwork* vpn) {
  cros->ConnectToVirtualNetwork(vpn);
}

}  // namespace

TEST(NetworkLibraryTest, DecodeNonAsciiSSID) {

  // Sets network name.
  {
    std::string wifi_setname = "SSID TEST";
    std::string wifi_setname_result = "SSID TEST";
    WifiNetwork* wifi = new WifiNetwork("fw");
    wifi->SetName(wifi_setname);
    EXPECT_EQ(wifi->name(), wifi_setname_result);
    delete wifi;
  }

  // Truncates invalid UTF-8
  {
    std::string wifi_setname2 = "SSID TEST \x01\xff!";
    std::string wifi_setname2_result = "SSID TEST \xEF\xBF\xBD\xEF\xBF\xBD!";
    WifiNetwork* wifi = new WifiNetwork("fw");
    wifi->SetName(wifi_setname2);
    EXPECT_EQ(wifi->name(), wifi_setname2_result);
    delete wifi;
  }

  // UTF8 SSID
  {
    std::string wifi_utf8 = "UTF-8 \u3042\u3044\u3046";
    std::string wifi_utf8_result = "UTF-8 \xE3\x81\x82\xE3\x81\x84\xE3\x81\x86";
    WifiNetwork* wifi = new WifiNetwork("fw");
    WifiNetwork::TestApi test_wifi(wifi);
    test_wifi.SetSsid(wifi_utf8);
    EXPECT_EQ(wifi->name(), wifi_utf8_result);
    delete wifi;
  }

  // latin1 SSID -> UTF8 SSID
  {
    std::string wifi_latin1 = "latin-1 \xc0\xcb\xcc\xd6\xfb";
    std::string wifi_latin1_result = "latin-1 \u00c0\u00cb\u00cc\u00d6\u00fb";
    WifiNetwork* wifi = new WifiNetwork("fw");
    WifiNetwork::TestApi test_wifi(wifi);
    test_wifi.SetSsid(wifi_latin1);
    EXPECT_EQ(wifi->name(), wifi_latin1_result);
    delete wifi;
  }

  // Hex SSID
  {
    std::string wifi_hex = "5468697320697320484558205353494421";
    std::string wifi_hex_result = "This is HEX SSID!";
    WifiNetwork* wifi = new WifiNetwork("fw");
    WifiNetwork::TestApi test_wifi(wifi);
    test_wifi.SetHexSsid(wifi_hex);
    EXPECT_EQ(wifi->name(), wifi_hex_result);
    delete wifi;
  }
}

// Create a stub libcros for testing NetworkLibrary functionality through
// NetworkLibraryStubImpl.
// NOTE: It would be of little value to test stub functions that simply return
// predefined values, e.g. ethernet_available(). However, many other functions
// such as connected_network() return values which are set indirectly and thus
// we can test the logic of those setters.

class NetworkLibraryStubTest : public testing::Test {
 public:
  NetworkLibraryStubTest() : cros_(NULL) {}

  static void SetUpTestCase() {
    // Ideally, we'd open a test DB for each test case, and close it
    // again, removing the temp dir, but unfortunately, there's a
    // bug in NSS that prevents this from working, so we just open
    // it once, and empty it for each test case.  Here's the bug:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=588269
    ASSERT_TRUE(crypto::OpenTestNSSDB());
    // There is no matching TearDownTestCase call to close the test NSS DB
    // because that would leave NSS in a potentially broken state for further
    // tests, due to https://bugzilla.mozilla.org/show_bug.cgi?id=588269
  }

 protected:
  virtual void SetUp() {
    slot_ = net::NSSCertDatabase::GetInstance()->GetPublicModule();
    cros_ = CrosLibrary::Get()->GetNetworkLibrary();
    ASSERT_TRUE(cros_) << "GetNetworkLibrary() Failed!";

    // Test db should be empty at start of test.
    EXPECT_EQ(0U, ListCertsInSlot(slot_->os_module_handle()).size());
  }
  virtual void TearDown() {
    cros_ = NULL;
    EXPECT_TRUE(CleanupSlotContents(slot_->os_module_handle()));
    EXPECT_EQ(0U, ListCertsInSlot(slot_->os_module_handle()).size());
  }

  virtual void GetTestData(const std::string& filename, std::string* contents) {
      FilePath path;
      std::string error;
      PathService::Get(chrome::DIR_TEST_DATA, &path);
      path = path.AppendASCII("chromeos").AppendASCII("cros").Append(filename);
      ASSERT_TRUE(contents != NULL);
      ASSERT_TRUE(file_util::PathExists(path))
        << "Couldn't find test data file " << path.value();
      ASSERT_TRUE(file_util::ReadFileToString(path, contents))
        << "Unable to read test data file " << path.value();
  }

  ScopedStubCrosEnabler cros_stub_;
  NetworkLibrary* cros_;
 private:
  net::CertificateList ListCertsInSlot(PK11SlotInfo* slot) {
    net::CertificateList result;
    CERTCertList* cert_list = PK11_ListCertsInSlot(slot);
    for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
         !CERT_LIST_END(node, cert_list);
         node = CERT_LIST_NEXT(node)) {
      result.push_back(net::X509Certificate::CreateFromHandle(
          node->cert, net::X509Certificate::OSCertHandles()));
    }
    CERT_DestroyCertList(cert_list);

    // Sort the result so that test comparisons can be deterministic.
    std::sort(result.begin(), result.end(), net::X509Certificate::LessThan());
    return result;
  }

  bool CleanupSlotContents(PK11SlotInfo* slot) {
    bool ok = true;
    net::CertificateList certs = ListCertsInSlot(slot);
    for (size_t i = 0; i < certs.size(); ++i) {
      if (!net::NSSCertDatabase::GetInstance()->DeleteCertAndKey(certs[i]))
        ok = false;
    }
    return ok;
  }

  scoped_refptr<net::CryptoModule> slot_;
};

// Default stub state:
// vpn1: disconnected, L2TP/IPsec + PSK
// vpn2: disconnected, L2TP/IPsec + user cert
// vpn3: disconnected, OpenVpn
// eth1: connected (active network)
// wifi1: connected
// wifi2: disconnected
// wifi3: disconnected, WEP
// wifi4: disconnected, 8021x
// wifi5: disconnected
// wifi6: disconnected
// cellular1: connected, activated, not roaming
// cellular2: disconnected, activated, roaming

TEST_F(NetworkLibraryStubTest, NetworkLibraryAccessors) {
  // Set up state.
  // Set wifi2->connecting for these tests.
  WifiNetwork* wifi2 = cros_->FindWifiNetworkByPath("wifi2");
  ASSERT_NE(static_cast<const Network*>(NULL), wifi2);
  Network::TestApi test_wifi2(wifi2);
  test_wifi2.SetConnecting();
  // Set cellular1->connecting for these tests.
  CellularNetwork* cellular1 = cros_->FindCellularNetworkByPath("cellular1");
  ASSERT_NE(static_cast<const Network*>(NULL), cellular1);
  Network::TestApi test_cellular1(cellular1);
  test_cellular1.SetConnecting();

  // Ethernet
  ASSERT_NE(static_cast<const EthernetNetwork*>(NULL),
            cros_->ethernet_network());
  EXPECT_EQ("eth1", cros_->ethernet_network()->service_path());
  EXPECT_NE(static_cast<const Network*>(NULL),
            cros_->FindNetworkByPath("eth1"));
  EXPECT_TRUE(cros_->ethernet_connected());
  EXPECT_FALSE(cros_->ethernet_connecting());

  // Wifi
  ASSERT_NE(static_cast<const WifiNetwork*>(NULL), cros_->wifi_network());
  EXPECT_EQ("wifi1", cros_->wifi_network()->service_path());
  EXPECT_NE(static_cast<const WifiNetwork*>(NULL),
            cros_->FindWifiNetworkByPath("wifi1"));
  EXPECT_TRUE(cros_->wifi_connected());
  EXPECT_FALSE(cros_->wifi_connecting());  // Only true for active wifi.
  EXPECT_GE(cros_->wifi_networks().size(), 1U);

  // Cellular
  ASSERT_NE(static_cast<const CellularNetwork*>(NULL),
            cros_->cellular_network());
  EXPECT_EQ("cellular1", cros_->cellular_network()->service_path());
  EXPECT_NE(static_cast<const CellularNetwork*>(NULL),
            cros_->FindCellularNetworkByPath("cellular1"));
  EXPECT_FALSE(cros_->cellular_connected());
  EXPECT_TRUE(cros_->cellular_connecting());
  EXPECT_GE(cros_->cellular_networks().size(), 1U);

  // VPN
  ASSERT_EQ(static_cast<const VirtualNetwork*>(NULL), cros_->virtual_network());
  EXPECT_GE(cros_->virtual_networks().size(), 1U);

  // Active network and global state
  EXPECT_TRUE(cros_->Connected());
  ASSERT_NE(static_cast<const Network*>(NULL), cros_->active_network());
  EXPECT_EQ("eth1", cros_->active_network()->service_path());
  ASSERT_NE(static_cast<const Network*>(NULL), cros_->connected_network());
  EXPECT_EQ("eth1", cros_->connected_network()->service_path());
  // The "wifi1" is connected, so we do not return "wifi2" for the connecting
  // network. There is no conencted cellular network, so "cellular1" is
  // returned by connecting_network().
  EXPECT_TRUE(cros_->Connecting());
  ASSERT_NE(static_cast<const Network*>(NULL), cros_->connecting_network());
  EXPECT_EQ("cellular1", cros_->connecting_network()->service_path());
}

TEST_F(NetworkLibraryStubTest, NetworkConnectWifi) {
  WifiNetwork* wifi1 = cros_->FindWifiNetworkByPath("wifi1");
  ASSERT_NE(static_cast<const WifiNetwork*>(NULL), wifi1);
  EXPECT_TRUE(wifi1->connected());
  cros_->DisconnectFromNetwork(wifi1);
  EXPECT_FALSE(wifi1->connected());
  EXPECT_TRUE(cros_->CanConnectToNetwork(wifi1));
  cros_->ConnectToWifiNetwork(wifi1);
  EXPECT_TRUE(wifi1->connected());
}

TEST_F(NetworkLibraryStubTest, NetworkConnectOncWifi) {
  // Import a wireless network via loading an ONC file.
  std::string test_blob;
  GetTestData("cert-pattern.onc", &test_blob);
  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);
  ASSERT_TRUE(parser.parse_error().empty());
  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(2, parser.GetCertificatesSize());
  scoped_ptr<Network> network(parser.ParseNetwork(0, NULL));
  ASSERT_TRUE(network.get());
  EXPECT_EQ(CLIENT_CERT_TYPE_PATTERN, network->client_cert_type());

  StubEnrollmentDelegate* enrollment_delegate =
      new StubEnrollmentDelegate(&parser);

  network->SetEnrollmentDelegate(enrollment_delegate);
  EXPECT_FALSE(enrollment_delegate->did_enroll);
  EXPECT_FALSE(enrollment_delegate->correct_args);
  WifiNetwork* wifi1 = static_cast<WifiNetwork*>(network.get());

  ASSERT_NE(static_cast<const WifiNetwork*>(NULL), wifi1);
  EXPECT_FALSE(wifi1->connected());
  EXPECT_TRUE(cros_->CanConnectToNetwork(wifi1));
  EXPECT_FALSE(wifi1->connected());
  wifi1->AttemptConnection(
      base::Bind(&WifiNetworkConnectCallback, cros_, wifi1));
  EXPECT_TRUE(wifi1->connected());
  EXPECT_TRUE(enrollment_delegate->did_enroll);
  EXPECT_TRUE(enrollment_delegate->correct_args);
}

TEST_F(NetworkLibraryStubTest, NetworkConnectOncVPN) {
  // Import a wireless network via loading an ONC file.
  std::string test_blob;
  GetTestData("cert-pattern-vpn.onc", &test_blob);
  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);
  ASSERT_TRUE(parser.parse_error().empty());
  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(2, parser.GetCertificatesSize());
  scoped_ptr<Network> network(parser.ParseNetwork(0, NULL));
  ASSERT_TRUE(network.get());
  EXPECT_EQ(CLIENT_CERT_TYPE_PATTERN, network->client_cert_type());

  StubEnrollmentDelegate* enrollment_delegate =
      new StubEnrollmentDelegate(&parser);

  network->SetEnrollmentDelegate(enrollment_delegate);
  EXPECT_FALSE(enrollment_delegate->did_enroll);
  EXPECT_FALSE(enrollment_delegate->correct_args);
  VirtualNetwork* vpn1 = static_cast<VirtualNetwork*>(network.get());

  ASSERT_NE(static_cast<const VirtualNetwork*>(NULL), vpn1);
  EXPECT_FALSE(vpn1->connected());
  EXPECT_TRUE(cros_->CanConnectToNetwork(vpn1));
  EXPECT_FALSE(vpn1->connected());
  vpn1->AttemptConnection(
      base::Bind(&VirtualNetworkConnectCallback, cros_, vpn1));
  EXPECT_TRUE(vpn1->connected());
  EXPECT_TRUE(enrollment_delegate->did_enroll);
  EXPECT_TRUE(enrollment_delegate->correct_args);
}

TEST_F(NetworkLibraryStubTest, NetworkConnectVPN) {
  VirtualNetwork* vpn1 = cros_->FindVirtualNetworkByPath("vpn1");
  EXPECT_NE(static_cast<const VirtualNetwork*>(NULL), vpn1);
  EXPECT_FALSE(vpn1->connected());
  EXPECT_TRUE(cros_->CanConnectToNetwork(vpn1));
  cros_->ConnectToVirtualNetwork(vpn1);
  EXPECT_TRUE(vpn1->connected());
  ASSERT_NE(static_cast<const VirtualNetwork*>(NULL), cros_->virtual_network());
  EXPECT_EQ("vpn1", cros_->virtual_network()->service_path());
}

// TODO(stevenjb): Test remembered networks.

// TODO(stevenjb): Test network profiles.

// TODO(stevenjb): Test network devices.

// TODO(stevenjb): Test data plans.

// TODO(stevenjb): Test monitor network / device.

}  // namespace chromeos
