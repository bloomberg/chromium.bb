// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/onc_network_parser.h"

#include <cert.h>
#include <pk11pub.h>

#include "base/lazy_instance.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "crypto/nss_util.h"
#include "net/base/cert_database.h"
#include "net/base/crypto_module.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char kNetworkConfigurationOpenVPN[] =
      "    {"
      "      \"GUID\": \"{408290ea-9299-4757-ab04-8957d55f0f13}\","
      "      \"Type\": \"VPN\","
      "      \"Name\": \"MyVPN\","
      "      \"VPN\": {"
      "        \"Host\": \"vpn.acme.org\","
      "        \"Type\": \"OpenVPN\","
      "        \"OpenVPN\": {"
      "          \"AuthRetry\": \"interact\","
      "          \"CompLZO\": \"true\","
      "          \"KeyDirection\": \"1\","
      "          \"Port\": 443,"
      "          \"Proto\": \"udp\","
      "          \"PushPeerInfo\": true,"
      "          \"RemoteCertEKU\": \"TLS Web Server Authentication\","
      "          \"RemoteCertKU\": ["
      "            \"eo\""
      "          ],"
      "          \"RemoteCertTLS\": \"server\","
      "          \"RenegSec\": 0,"
      "          \"ServerPollTimeout\": 10,"
      "          \"StaticChallenge\": \"My static challenge\","
      "          \"TLSAuthContents\": \""
      "-----BEGIN OpenVPN Static key V1-----\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "END OpenVPN Static key V1-----\n\","
      "          \"TLSRemote\": \"MyOpenVPNServer\","
      "          \"SaveCredentials\": false,"
      "          \"ServerCARef\": \"{55ca78f6-0842-4e1b-96a3-09a9e1a26ef5}\","
      "          \"ClientCertType\": \"Pattern\","
      "          \"ClientCertPattern\": {"
      "            \"IssuerRef\": \"{68a2ed90-13a1-4120-a1fe-282508320e18}\","
      "            \"EnrollmentURI\": ["
      "              \"chrome-extension://abc/keygen-cert.html\""
      "            ]"
      "          },"
      "        }"
      "      }"
      "    }";

const char kCertificateWebAuthority[] =
      "        {"
      "            \"GUID\": \"{f998f760-272b-6939-4c2beffe428697ab}\","
      "            \"Type\": \"Authority\","
      "            \"Trust\": [\"Web\"],"
      "            \"X509\": \"MIIDojCCAwugAwIBAgIJAKGvi5ZgEWDVMA0GCSqGSIb3D"
      "QEBBAUAMIGTMRUwEwYDVQQKEwxHb29nbGUsIEluYy4xETAPBgNVBAsTCENocm9tZU9TMS"
      "IwIAYJKoZIhvcNAQkBFhNnc3BlbmNlckBnb29nbGUuY29tMRowGAYDVQQHExFNb3VudGF"
      "pbiBWaWV3LCBDQTELMAkGA1UECBMCQ0ExCzAJBgNVBAYTAlVTMQ0wCwYDVQQDEwRsbWFv"
      "MB4XDTExMDMxNjIzNDcxMFoXDTEyMDMxNTIzNDcxMFowgZMxFTATBgNVBAoTDEdvb2dsZ"
      "SwgSW5jLjERMA8GA1UECxMIQ2hyb21lT1MxIjAgBgkqhkiG9w0BCQEWE2dzcGVuY2VyQG"
      "dvb2dsZS5jb20xGjAYBgNVBAcTEU1vdW50YWluIFZpZXcsIENBMQswCQYDVQQIEwJDQTE"
      "LMAkGA1UEBhMCVVMxDTALBgNVBAMTBGxtYW8wgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJ"
      "AoGBAMDX6BQz2JUzIAVjetiXxDznd2wdqVqVHfNkbSRW+xBywgqUaIXmFEGUol7VzPfme"
      "FV8o8ok/eFlQB0h6ycqgwwMd0KjtJs2ys/k0F5GuN0G7fsgr+NRnhVgxj21yF6gYTN/8a"
      "9kscla/svdmp8ekexbALFnghbLBx3CgcqUxT+tAgMBAAGjgfswgfgwDAYDVR0TBAUwAwE"
      "B/zAdBgNVHQ4EFgQUbYygbSkl4kpjCNuxoezFGupA97UwgcgGA1UdIwSBwDCBvYAUbYyg"
      "bSkl4kpjCNuxoezFGupA97WhgZmkgZYwgZMxFTATBgNVBAoTDEdvb2dsZSwgSW5jLjERM"
      "A8GA1UECxMIQ2hyb21lT1MxIjAgBgkqhkiG9w0BCQEWE2dzcGVuY2VyQGdvb2dsZS5jb2"
      "0xGjAYBgNVBAcTEU1vdW50YWluIFZpZXcsIENBMQswCQYDVQQIEwJDQTELMAkGA1UEBhM"
      "CVVMxDTALBgNVBAMTBGxtYW+CCQChr4uWYBFg1TANBgkqhkiG9w0BAQQFAAOBgQCDq9wi"
      "Q4uVuf1CQU3sXfXCy1yqi5m8AsO9FxHvah5/SVFNwKllqTfedpCaWEswJ55YAojW9e+pY"
      "2Fh3Fo/Y9YkF88KCtLuBjjqDKCRLxF4LycjHODKyQQ7mN/t5AtP9yKOsNvWF+M4IfReg5"
      "1kohau6FauQx87by5NIRPdkNPvkQ==\""
      "        }";


class OncNetworkParserTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    ASSERT_TRUE(temp_db_dir_.Get().CreateUniqueTempDir());
    // Ideally, we'd open a test DB for each test case, and close it
    // again, removing the temp dir, but unfortunately, there's a
    // bug in NSS that prevents this from working, so we just open
    // it once, and empty it for each test case.  Here's the bug:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=588269
    ASSERT_TRUE(
        crypto::OpenTestNSSDB(temp_db_dir_.Get().path(),
                              "OncNetworkParserTest db"));
  }

  static void TearDownTestCase() {
    ASSERT_TRUE(temp_db_dir_.Get().Delete());
  }

  virtual void SetUp() {
    slot_ = cert_db_.GetPublicModule();

    // Don't run the test if the setup failed.
    ASSERT_TRUE(slot_->os_module_handle());

    // Test db should be empty at start of test.
    EXPECT_EQ(0U, ListCertsInSlot(slot_->os_module_handle()).size());
  }

  virtual void TearDown() {
    EXPECT_TRUE(CleanupSlotContents(slot_->os_module_handle()));
    EXPECT_EQ(0U, ListCertsInSlot(slot_->os_module_handle()).size());
  }

  const base::Value* GetExpectedProperty(const Network* network,
                                         PropertyIndex index,
                                         base::Value::Type expected_type);
  void CheckStringProperty(const Network* network,
                           PropertyIndex index,
                           const char* expected);
  void CheckBooleanProperty(const Network* network,
                            PropertyIndex index,
                            bool expected);

 protected:
  scoped_refptr<net::CryptoModule> slot_;
  net::CertDatabase cert_db_;

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
      if (!cert_db_.DeleteCertAndKey(certs[i]))
        ok = false;
    }
    return ok;
  }

  static base::LazyInstance<ScopedTempDir> temp_db_dir_;
};

const base::Value* OncNetworkParserTest::GetExpectedProperty(
    const Network* network,
    PropertyIndex index,
    base::Value::Type expected_type) {
  const base::Value* value;
  if (!network->GetProperty(index, &value)) {
    ADD_FAILURE() << "Property " << index << " does not exist";
    return NULL;
  }
  if (!value->IsType(expected_type)) {
    ADD_FAILURE() << "Property " << index << " expected type "
                  << expected_type << " actual type "
                  << value->GetType();
    return NULL;
  }
  return value;
}

void OncNetworkParserTest::CheckStringProperty(const Network* network,
                                               PropertyIndex index,
                                               const char* expected) {
  const base::Value* value =
    GetExpectedProperty(network, index, base::Value::TYPE_STRING);
  if (!value)
    return;
  std::string string_value;
  value->GetAsString(&string_value);
  EXPECT_EQ(expected, string_value);
}

void OncNetworkParserTest::CheckBooleanProperty(const Network* network,
                                                PropertyIndex index,
                                                bool expected) {
  const base::Value* value =
    GetExpectedProperty(network, index, base::Value::TYPE_BOOLEAN);
  if (!value)
    return;
  bool bool_value = false;
  value->GetAsBoolean(&bool_value);
  EXPECT_EQ(expected, bool_value);
}

// static
base::LazyInstance<ScopedTempDir> OncNetworkParserTest::temp_db_dir_ =
    LAZY_INSTANCE_INITIALIZER;

TEST_F(OncNetworkParserTest, TestCreateNetworkWifi1) {
  std::string test_blob(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"WiFi\": {"
      "      \"Security\": \"WEP-PSK\","
      "      \"SSID\": \"ssid\","
      "      \"Passphrase\": \"pass\","
      "    }"
      "  }]"
      "}");
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
  EXPECT_FALSE(parser.ParseNetwork(1));
  scoped_ptr<Network> network(parser.ParseNetwork(0));
  ASSERT_TRUE(network.get());

  EXPECT_EQ(network->type(), chromeos::TYPE_WIFI);
  WifiNetwork* wifi = static_cast<WifiNetwork*>(network.get());
  EXPECT_EQ(wifi->encryption(), chromeos::SECURITY_WEP);
  CheckStringProperty(wifi, PROPERTY_INDEX_SECURITY, flimflam::kSecurityWep);
  EXPECT_EQ(wifi->name(), "ssid");
  CheckStringProperty(wifi, PROPERTY_INDEX_SSID, "ssid");
  EXPECT_EQ(wifi->auto_connect(), false);
  EXPECT_EQ(wifi->passphrase(), "pass");
  CheckStringProperty(wifi, PROPERTY_INDEX_PASSPHRASE, "pass");
}

TEST_F(OncNetworkParserTest, TestCreateNetworkWifiEAP1) {
  std::string test_blob(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"WiFi\": {"
      "      \"Security\": \"WPA-EAP\","
      "      \"SSID\": \"ssid\","
      "      \"AutoConnect\": true,"
      "      \"EAP\": {"
      "        \"Outer\": \"PEAP\","
      "        \"UseSystemCAs\": false,"
      "      }"
      "    }"
      "  }]"
      "}");
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
  EXPECT_FALSE(parser.ParseNetwork(1));
  scoped_ptr<Network> network(parser.ParseNetwork(0));
  ASSERT_TRUE(network.get());

  EXPECT_EQ(network->type(), chromeos::TYPE_WIFI);
  WifiNetwork* wifi = static_cast<WifiNetwork*>(network.get());
  EXPECT_EQ(wifi->encryption(), chromeos::SECURITY_8021X);
  CheckStringProperty(wifi, PROPERTY_INDEX_SECURITY, flimflam::kSecurity8021x);
  EXPECT_EQ(wifi->name(), "ssid");
  EXPECT_EQ(wifi->auto_connect(), true);
  CheckBooleanProperty(wifi, PROPERTY_INDEX_AUTO_CONNECT, true);
  EXPECT_EQ(wifi->eap_method(), EAP_METHOD_PEAP);
  CheckStringProperty(wifi, PROPERTY_INDEX_EAP_METHOD,
                      flimflam::kEapMethodPEAP);
  EXPECT_EQ(wifi->eap_use_system_cas(), false);
}

TEST_F(OncNetworkParserTest, TestCreateNetworkWifiEAP2) {
  std::string test_blob(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"WiFi\": {"
      "      \"Security\": \"WPA-EAP\","
      "      \"SSID\": \"ssid\","
      "      \"AutoConnect\": false,"
      "      \"EAP\": {"
      "        \"Outer\": \"LEAP\","
      "        \"Identity\": \"user\","
      "        \"Password\": \"pass\","
      "        \"AnonymousIdentity\": \"anon\","
      "      }"
      "    }"
      "  }]"
      "}");
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
  EXPECT_FALSE(parser.ParseNetwork(1));
  scoped_ptr<Network> network(parser.ParseNetwork(0));
  ASSERT_TRUE(network.get());

  EXPECT_EQ(network->type(), chromeos::TYPE_WIFI);
  WifiNetwork* wifi = static_cast<WifiNetwork*>(network.get());
  EXPECT_EQ(wifi->encryption(), chromeos::SECURITY_8021X);
  EXPECT_EQ(wifi->name(), "ssid");
  EXPECT_EQ(wifi->auto_connect(), false);
  EXPECT_EQ(wifi->eap_method(), EAP_METHOD_LEAP);
  EXPECT_EQ(wifi->eap_use_system_cas(), true);
  EXPECT_EQ(wifi->eap_identity(), "user");
  CheckStringProperty(wifi, PROPERTY_INDEX_EAP_IDENTITY, "user");
  EXPECT_EQ(wifi->eap_passphrase(), "pass");
  CheckStringProperty(wifi, PROPERTY_INDEX_EAP_PASSWORD, "pass");
  EXPECT_EQ(wifi->eap_anonymous_identity(), "anon");
  CheckStringProperty(wifi, PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY, "anon");
}

TEST_F(OncNetworkParserTest, TestCreateNetworkOpenVPN) {
  std::string test_blob(std::string(
      "{"
      "  \"NetworkConfigurations\": [") +
      std::string(kNetworkConfigurationOpenVPN) + std::string(
      "  ]}"));
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
  scoped_ptr<Network> network(parser.ParseNetwork(0));
  ASSERT_TRUE(network.get() != NULL);

  EXPECT_EQ(network->type(), chromeos::TYPE_VPN);
  CheckStringProperty(network.get(), PROPERTY_INDEX_TYPE, flimflam::kTypeVPN);
  VirtualNetwork* vpn = static_cast<VirtualNetwork*>(network.get());
  EXPECT_EQ("MyVPN", vpn->name());
  EXPECT_EQ(PROVIDER_TYPE_OPEN_VPN, vpn->provider_type());
  CheckStringProperty(vpn, PROPERTY_INDEX_PROVIDER_TYPE,
                      flimflam::kProviderOpenVpn);
  EXPECT_EQ("vpn.acme.org", vpn->server_hostname());
  CheckStringProperty(vpn, PROPERTY_INDEX_PROVIDER_HOST, "vpn.acme.org");
  CheckStringProperty(vpn, PROPERTY_INDEX_VPN_DOMAIN, "");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_AUTHRETRY, "interact");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_CACERT,
                      "{55ca78f6-0842-4e1b-96a3-09a9e1a26ef5}");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_COMPLZO, "true");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_KEYDIRECTION, "1");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_PORT, "443");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_PROTO, "udp");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_PUSHPEERINFO, "true");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_REMOTECERTEKU,
                      "TLS Web Server Authentication");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_REMOTECERTKU, "eo");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_REMOTECERTTLS, "server");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_RENEGSEC, "0");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_SERVERPOLLTIMEOUT, "10");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_STATICCHALLENGE,
                      "My static challenge");
  // Check the default properties are set.
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_AUTHUSERPASS, "");
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_MGMT_ENABLE, "");

  std::string tls_auth_contents;
  const Value* tls_auth_value =
    GetExpectedProperty(vpn, PROPERTY_INDEX_OPEN_VPN_TLSAUTHCONTENTS,
                        base::Value::TYPE_STRING);
  if (tls_auth_value != NULL) {
    tls_auth_value->GetAsString(&tls_auth_contents);
    EXPECT_NE(std::string::npos,
              tls_auth_contents.find("END OpenVPN Static key V1-----\n"));
    EXPECT_NE(std::string::npos,
              tls_auth_contents.find(
                  "-----BEGIN OpenVPN Static key V1-----\n"));
  }
  CheckStringProperty(vpn, PROPERTY_INDEX_OPEN_VPN_TLSREMOTE,
                      "MyOpenVPNServer");
  EXPECT_FALSE(vpn->save_credentials());
  EXPECT_EQ("{55ca78f6-0842-4e1b-96a3-09a9e1a26ef5}", vpn->ca_cert_nss());
}

TEST_F(OncNetworkParserTest, TestCreateNetworkL2TPIPsec) {
  std::string test_blob(
      "{"
      "  \"NetworkConfigurations\": ["
      "    {"
      "      \"GUID\": \"{926b84e4-f2c5-0972-b9bbb8f44c4316f5}\","
      "      \"Name\": \"MyL2TPVPN\","
      "      \"Type\": \"VPN\","
      "      \"VPN\": {"
      "        \"Host\": \"l2tp.acme.org\","
      "        \"Type\": \"L2TP-IPsec\","
      "        \"IPsec\": {"
      "          \"IKEVersion\": 1,"
      "          \"AuthenticationType\": \"PSK\","
      "          \"PSK\": \"passphrase\""
      "        },"
      "        \"L2TP\": {"
      "          \"SaveCredentials\": false"
      "        }"
      "      }"
      "    }"
      "  ],"
      "  \"Certificates\": []"
      "}");
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
  scoped_ptr<Network> network(parser.ParseNetwork(0));
  ASSERT_TRUE(network != NULL);

  EXPECT_EQ(network->type(), chromeos::TYPE_VPN);
  CheckStringProperty(network.get(), PROPERTY_INDEX_TYPE, flimflam::kTypeVPN);
  VirtualNetwork* vpn = static_cast<VirtualNetwork*>(network.get());
  EXPECT_EQ("MyL2TPVPN", vpn->name());
  EXPECT_EQ(PROVIDER_TYPE_L2TP_IPSEC_PSK, vpn->provider_type());
  CheckStringProperty(vpn, PROPERTY_INDEX_PROVIDER_TYPE,
                      flimflam::kProviderL2tpIpsec);
  EXPECT_EQ("l2tp.acme.org", vpn->server_hostname());
  CheckStringProperty(vpn, PROPERTY_INDEX_PROVIDER_HOST, "l2tp.acme.org");
  CheckStringProperty(vpn, PROPERTY_INDEX_VPN_DOMAIN, "");
  EXPECT_EQ("passphrase", vpn->psk_passphrase());
  CheckStringProperty(vpn, PROPERTY_INDEX_L2TPIPSEC_PSK, "passphrase");
  CheckStringProperty(vpn, PROPERTY_INDEX_IPSEC_IKEVERSION, "1");
  EXPECT_FALSE(vpn->save_credentials());
}

TEST_F(OncNetworkParserTest, TestAddServerCertificate) {
  std::string test_blob(
      "{"
      "    \"Certificates\": ["
      "        {"
      "            \"GUID\": \"{f998f760-272b-6939-4c2beffe428697aa}\","
      "            \"Type\": \"Server\","
      "            \"X509\": \"MIICWDCCAcECAxAAATANBgkqhkiG9w0BAQQFADCBkzEVM"
      "BMGA1UEChMMR29vZ2xlLCBJbmMuMREwDwYDVQQLEwhDaHJvbWVPUzEiMCAGCSqGSIb3DQ"
      "EJARYTZ3NwZW5jZXJAZ29vZ2xlLmNvbTEaMBgGA1UEBxMRTW91bnRhaW4gVmlldywgQ0E"
      "xCzAJBgNVBAgTAkNBMQswCQYDVQQGEwJVUzENMAsGA1UEAxMEbG1hbzAeFw0xMTAzMTYy"
      "MzQ5MzhaFw0xMjAzMTUyMzQ5MzhaMFMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEVM"
      "BMGA1UEChMMR29vZ2xlLCBJbmMuMREwDwYDVQQLEwhDaHJvbWVPUzENMAsGA1UEAxMEbG"
      "1hbzCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA31WiJ9LvprrhKtDlW0RdLFAO7Qj"
      "kvs+sG6j2Vp2aBSrlhALG/0BVHUhWi4F/HHJho+ncLHAg5AGO0sdAjYUdQG6tfPqjLsIA"
      "LtoKEZZdFe/JhmqOEaxWsSdu2S2RdPgCQOsP79EH58gXwu2gejCkJDmU22WL4YLuqOc17"
      "nxbDC8CAwEAATANBgkqhkiG9w0BAQQFAAOBgQCv4vMD+PMlfnftu4/6Yf/oMLE8yCOqZT"
      "Q/dWCxB9PiJnOefiBeSzSZE6Uv3G7qnblZPVZaFeJMd+ostt0viCyPucFsFgLMyyoV1dM"
      "VPVwJT5Iq1AHehWXnTBbxUK9wioA5jOEKdroKjuSSsg/Q8Wx6cpJmttQz5olGPgstmACR"
      "WA==\""
      "        }"
      "    ],"
      "}");
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetCertificatesSize());
  EXPECT_TRUE(parser.ParseCertificate(0));
}

TEST_F(OncNetworkParserTest, TestAddAuthorityCertificate) {
  const std::string test_blob(std::string("{"
      "    \"Certificates\": [") +
      std::string(kCertificateWebAuthority) + std::string(
      "    ],"
      "}"));
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetCertificatesSize());
  EXPECT_TRUE(parser.ParseCertificate(0));
}

TEST_F(OncNetworkParserTest, TestAddClientCertificate) {
  std::string test_blob(
      "{"
      "    \"Certificates\": ["
      "        {"
      "            \"GUID\": \"{f998f760-272b-6939-4c2beffe428697ac}\","
      "            \"Type\": \"Client\","
      "            \"PKCS12\": \"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\""
      "        }"
      "    ],"
      "}");
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetCertificatesSize());
  EXPECT_TRUE(parser.ParseCertificate(0));
}

TEST_F(OncNetworkParserTest, TestNetworkAndCertificate) {
  std::string test_blob(std::string(
      "{"
      "  \"NetworkConfigurations\": [") +
      std::string(kNetworkConfigurationOpenVPN) + std::string(
      "  ],"
      "  \"Certificates\": [") +
      std::string(kCertificateWebAuthority) + std::string(
      "  ],"
      "}"));
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetCertificatesSize());
  EXPECT_TRUE(parser.ParseCertificate(0));

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  scoped_ptr<Network> network(parser.ParseNetwork(0));
  ASSERT_TRUE(network.get() != NULL);
  EXPECT_EQ(network->type(), chromeos::TYPE_VPN);
  VirtualNetwork* vpn = static_cast<VirtualNetwork*>(network.get());
  EXPECT_EQ(PROVIDER_TYPE_OPEN_VPN, vpn->provider_type());
}

}  // namespace chromeos
