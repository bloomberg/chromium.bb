// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/onc_network_parser.h"

#include <cert.h>
#include <keyhi.h>
#include <pk11pub.h>

#include "base/json/json_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "chrome/browser/net/pref_proxy_config_tracker_impl.h"
#include "crypto/nss_util.h"
#include "net/base/cert_database.h"
#include "net/base/cert_type.h"
#include "net/base/crypto_module.h"
#include "net/base/x509_certificate.h"
#include "net/proxy/proxy_config.h"
#include "net/third_party/mozilla_security_manager/nsNSSCertTrust.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace msm = mozilla_security_manager;
namespace chromeos {

namespace {
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

const char kCertificateWebAuthorityAlternate[] =
    "    {"
    "      \"Trust\": [\"Web\"],"
    "      \"GUID\": \"{f998f760-272b-6939-4c2beffe428697ab}\","
    "      \"Type\": \"Authority\","
    "      \"X509\": \"MIIDyDCCAzGgAwIBAgIJAJ0FxY4vamdaMA0GCSqGSIb3DQEBBQU"
    "AMIGfMQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEWMBQGA1UEBxMNTW91"
    "bnRhaW4gVmlldzEVMBMGA1UEChMMR29vZ2xlLCBJbmMuMREwDwYDVQQLEwhDaHJvbWVPU"
    "zEVMBMGA1UEAxMMR3JlZyBTcGVuY2VyMSIwIAYJKoZIhvcNAQkBFhNnc3BlbmNlckBnb2"
    "9nbGUuY29tMB4XDTExMDkyMjE1NTYyNFoXDTEyMDkyMTE1NTYyNFowgZ8xCzAJBgNVBAY"
    "TAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MRUw"
    "EwYDVQQKEwxHb29nbGUsIEluYy4xETAPBgNVBAsTCENocm9tZU9TMRUwEwYDVQQDEwxHc"
    "mVnIFNwZW5jZXIxIjAgBgkqhkiG9w0BCQEWE2dzcGVuY2VyQGdvb2dsZS5jb20wgZ8wDQ"
    "YJKoZIhvcNAQEBBQADgY0AMIGJAoGBANsX+zqngnujuSgcbRKboGwIk7Q65YYibHOTLsn"
    "DOFC2drgpKR4wqrFVD+SczVpD1ecG2VLmjdv9KZfWM0J1chqUrH0dF4dzDAGoxWSO153t"
    "7Q3xkbLtAUz+I4Tfmjirner9qXu1ov64csjOUc5NMMczfc5XHb4VNLskwuESyGm1AgMBA"
    "AGjggEIMIIBBDAdBgNVHQ4EFgQUbncQaaTUwuIXUvgmVAxLyTYEPc8wgdQGA1UdIwSBzD"
    "CByYAUbncQaaTUwuIXUvgmVAxLyTYEPc+hgaWkgaIwgZ8xCzAJBgNVBAYTAlVTMRMwEQY"
    "DVQQIEwpDYWxpZm9ybmlhMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MRUwEwYDVQQKEwxH"
    "b29nbGUsIEluYy4xETAPBgNVBAsTCENocm9tZU9TMRUwEwYDVQQDEwxHcmVnIFNwZW5jZ"
    "XIxIjAgBgkqhkiG9w0BCQEWE2dzcGVuY2VyQGdvb2dsZS5jb22CCQCdBcWOL2pnWjAMBg"
    "NVHRMEBTADAQH/MA0GCSqGSIb3DQEBBQUAA4GBAHeqgc0f5didWwAmMQ9HwSdxIt4QWam"
    "L2zGYeRVBbYTQTMhzvpEudl/m5hF9r7qcr/76mSNN8+joJHH2HR2uTRonGety+3lSY1oo"
    "uiTp2PeYpeEPBZS4VzEi0loa4l3gEJZ9HYAf85QvQNlVvYInRGV07N/8PaRnwfIpQjyVZ"
    "OKs\""
    "    }";


const char kCertificateServer[] =
    "        {"
    "            \"Trust\": [\"Web\"],"
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
    "        }";

const char kCertificateServerAlternate[] =
    "        {"
    "          \"Trust\": [\"Web\"],"
    "          \"GUID\": \"{f998f760-272b-6939-4c2beffe428697aa}\","
    "          \"Type\": \"Server\","
    "          \"X509\": \"MIIFdDCCA1wCCQCTrmiGB2gCoDANBgkqhkiG9w0BAQUFADB"
    "8MQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEWMBQGA1UEBxMNTW91bnRh"
    "aW4gVmlldzEVMBMGA1UEChMMR29vZ2xlLCBJbmMuMQ8wDQYDVQQLEwZDaHJvbWUxGDAWB"
    "gNVBAMTD0dvb2dsZSBFbmdpbmVlcjAeFw0xMTEyMTQxODM0MzdaFw0xMjEyMTMxODM0Mz"
    "daMHwxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRYwFAYDVQQHEw1Nb3V"
    "udGFpbiBWaWV3MRUwEwYDVQQKEwxHb29nbGUsIEluYy4xDzANBgNVBAsTBkNocm9tZTEY"
    "MBYGA1UEAxMPR29vZ2xlIEVuZ2luZWVyMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICC"
    "gKCAgEAv6IXCd7TjdXf50jblIh0/VeoiwI8ZXE5LIaSGW0biAgOx68OxlR0YDa2c5xd03"
    "N1NwUjuAEr+UQcXEjMfIVaHOhnjba7ZTsAKKd+z5/JY7Lrha8TjH2gJM9Ccfzx420bmBT"
    "fZJiV7pk33ZuUlHDLfkL3zRZ2eHPtU21hUcyPthCngHy7+ZG+kEt3WX/TMsf73XAHHHZI"
    "ipXZ+0bGmOUfCAlgsfxWmbqvcIAx8AO2j0FbA1fGi8chYsd0+fsUypf0U+UTmshvlpcWE"
    "X+7D584+biexawM+T4rpPoO7ESBmiA+Ta356Zgw9rDbOC4fr8PQzb7thjJgOu6JbZHnIr"
    "3pdbJF0OaEoqoe8znLrT96Bk4QRTcnpmpgY5au9kCsNAVd3G+C6eJSX+FuQYN2kMEOAX1"
    "Wjrcmz83p3a+1JONLKnxOquBi8EGCHq89xZzjgWYMl8OY+Vpcrz2vcZ4h0reyqAjs9+rr"
    "ILqHFK3TQ13RJN1VqhdDiWbcGWg69o/TXVdBR6Ozne+RWizTAUKU8K7bv0jbatw31saDe"
    "fHae1pdFgdf/+lTxdC6AZJLge1Xp7dc9tsjdo9HUStE709fxEdSdTcyxtDEKBrCTA1iJa"
    "d/8v4SkTk8DLQtfX+bg80ta22qFNFVISKJc/ZPvcAbNUL/TuLBhDAV93KCXEODA4uH0ME"
    "CAwEAATANBgkqhkiG9w0BAQUFAAOCAgEAQkgjl83nneWjDxKjErNdHlNzO0vJHXeEUa0C"
    "CDlEt6ckLNUgNcUD8WSo92aFboAs4KlZ142f/6ofqLZJ5T9erRCGFTm//LmQfd0pgujBw"
    "l4zSal8N/bPqkZaDO/9It9/VaERzcA/NT/74kmpdtN3rCvLIX7RjhTa1u9ew++9lXq/Es"
    "mKVCJPMHirWZXWxeSDFmJjDsHwmTJm6SXDpOTC/2wpV4/WK+RJvNxB68slXggpK+K5kHp"
    "gS7RWF7G6BqQ8nbMF4ygY81Pf10Yq7Gw7Pj/uY3oHDgByyEKk/XZoHgZGVogiylxWez47"
    "qwTSbzwRgeXctjK/dK05rPf2RejCIEKK7vpxe2/DwM3MKqBT0kRpgK+UCBI/RPkIUIw+k"
    "qnO38CEKajIazsH7xy+fFSc8NyXqWTWNP2KQOQIelNcUyPpQl3e4CSIIG9+TrCMsN3EKt"
    "EkT0A9iWICuufcf1dtAynyNP08gWG1jp0lof01mK3mWpfcN79nNeYsidOd3p63VKmN/CV"
    "AFSbV5cwlDSCSLb0ybmCnRqJ18yslKNojTRVMl/5BMlS79hrJLPEknDgfLSKq7LGuiVfh"
    "sKARtDGj0HlHRMWfeN4dof1mbtHCUXFOxVVP2No+zZonwEZ7zxwR6PfPdgXy/SD1uPa4T"
    "AFjRGPNI8OOx+tWLTiskKzENKs=\""
    "        }";

const char kCertificateClient[] =
    "        {"
    "            \"GUID\": \"{f998f760-272b-6939-4c2beffe428697ac}\","
    "            \"Type\": \"Client\","
    "            \"PKCS12\": \"MIIGUQIBAzCCBhcGCSqGSIb3DQEHAaCCBggEggYEMII"
    "GADCCAv8GCSqGSIb3DQEHBqCCAvAwggLsAgEAMIIC5QYJKoZIhvcNAQcBMBwGCiqGSIb3"
    "DQEMAQYwDgQIHnFaWM2Y0BgCAggAgIICuG4ou9mxkhpus8WictLJe+JOnSQrdNXV3FMQr"
    "4pPJ6aJJFBMKZ80W2GpR8XNY/SSKkdaNr1puDm1bDBFGaHQuCKXYcWO8ynBQ1uoZaFaTT"
    "FxWbbHo89Jrvw+gIrgpoOHQ0KECEbh5vOZCjGHoaQb4QZOkw/6Cuc4QRoCPJAI3pbSPG4"
    "4kRbOuOaTZvBHSIPkGf3+R6byTvZ3Yiuw7IIzxUp2fYjtpCWd/NvtI70heJCWdb5hwCeN"
    "afIEpX+MTVuhUegysIFkOMMlUBIQSI5ky8kjx0Yi82BT/dpz9QgrqFL8NnTMXp0JlKFGL"
    "QwsIQhvGjw/E52fEWRy85B5eezgNsD4QOLeZkF0bQAz8kXfLi+0djxsHvH9W9X2pwaFiA"
    "veXR15/v+wfCwQGSsRhISGLzg/gO1agbQdaexI9GlEeZW0FEY7TblarKh8TVGNrauU7GC"
    "GDmD2w7wx2HTXfo9SbViFoYVKuxcrpHGGEtBffnIeAwN6BBee4v11jxv0i/QUdK5G6FbH"
    "qlD1AhHsm0YvidYKqJ0cnN262xIJH7dhKq/qUiAT+qk3+d3/obqxbvVY+bDoJQ10Gzj1A"
    "SMy4zcSL7KW1l99xxMr6OlKr4Sr23oGw4BIN73FB8S8qMzz/VzL4azDUyGpPkzWl0yXPs"
    "HpFWh1nZlsQehyknyWDH/waKrrG8tVWxHZLgq+zrFxQTh63UHXSD+TXB+AQg2xmQMeWlf"
    "vRcsKL8titZ6PnWCHTmZY+3ibv5avDsg7He6OcZOi9ZmYMx82QHuzb4aZ/T+OC05oA97n"
    "VNbTN6t8okkRtBamMvVhtTJANVpsdPi8saEaVF8e9liwmpq2w7pqXnzgdzvjSUpPAa4dZ"
    "BjWnZJvFOHuxZqiRzQdZbeh9+bXwsQJhRNe+d4EgFwuqebQOczeUi4NVTHTFiuPEjCCAv"
    "kGCSqGSIb3DQEHAaCCAuoEggLmMIIC4jCCAt4GCyqGSIb3DQEMCgECoIICpjCCAqIwHAY"
    "KKoZIhvcNAQwBAzAOBAi0znbEekG/MgICCAAEggKAJfFPaQyYYLohEA1ruAZfepwMVrR8"
    "eLMx00kkfXN9EoZeFPj2q7TGdqmbkUSqXnZK1ums7pFCPLgP1CsPlsq/4ZPDT2LLVFZNL"
    "OgmdQBOSTvycfsj0iKYrwRC55wJI2OXsc062sT7oa99apkgrEyHq7JbOhszfnv5+aVy/6"
    "O115dncqFPW2ei4CBzLEZyYa+Mka6CGqSdm97WVmv0emDKTFEP/FN4TH/tS8Qm6Y7DTKG"
    "CujC+hb6lTRFYJAD4uld132dv0xQFkwDZGfdnuGJuNZBDC0gZk3BYvOaCUD8Y9UB5IjfG"
    "Jax2yrurY1wSGSlTurafDTPrKqIdBovwCPsad2xz1YHC2Yy0h1FyR+2uitDyNfTiETfug"
    "3bFbjwodu9wmt31A2ZFn4JpUrTYoZ3LZXngC3nNTayU0Tkd1ICMep2GbCReL3ajOlgOKG"
    "FVoOm/qDnhiH6W/ebtAQXqVpuKut8uY0X0Ocmx7mTpmxlfDSRiBY9rvnrGfnpfLMxtFeF"
    "9jv3n8vSwvA0Xn0okAv1FWYLStiCpNxnD6lmXQvcmL/skAlJJpHY9/58qt/e5sGYrkKBw"
    "3jnX40zaK4W7GeJvhij0MRr6yUL2lvaEcWDnK6K1F90G/ybKRCTHBCJzyBe7yHhZCc+Zc"
    "vKK6DTi83fELTyupy08BkXt7oPdapxmKlZxTldo9FpPXSqrdRtAWhDkEkIEf8dMf8QrQr"
    "3glCWfbcQ047URYX45AHRnLTLLkJfdY8+Y3KsHoqL2UrOrct+J1u0mmnLbonN3pB2B4nd"
    "9X9vf9/uSFrgvk0iPO0Ro3UPRUIIYEP2Kx51pZZVDd++hl5gXtqe0NIpphGhxLycIdzEl"
    "MCMGCSqGSIb3DQEJFTEWBBR1uVpGjHRddIEYuJhz/FgG4Onh6jAxMCEwCQYFKw4DAhoFA"
    "AQU1M+0WRDkoVGbGg1jj7q2fI67qHIECBzRYESpgt5iAgIIAA==\""
    "        }";

const char kCertificateClientAlternate[] =
    "        {"
    "            \"GUID\": \"{f998f760-272b-6939-4c2beffe428697ac}\","
    "            \"Type\": \"Client\","
    "          \"PKCS12\": \"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\""
    "        }";

const char kNetworkConfigurationWifiWepPskBegin[] =
  "{"
  "  \"NetworkConfigurations\": [{"
  "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
  "    \"Type\": \"WiFi\","
  "    \"WiFi\": {"
  "      \"Security\": \"WEP-PSK\","
  "      \"SSID\": \"ssid\","
  "      \"Passphrase\": \"pass\",";

const char kNetworkConfigurationWifiWepPskEnd[] =
  "    }"
  "  }]"
  "}";

const char g_token_name[] = "OncNetworkParserTest token";

net::CertType GetCertType(const net::X509Certificate* cert) {
  DCHECK(cert);
  msm::nsNSSCertTrust trust(cert->os_cert_handle()->trust);
  if (trust.HasAnyUser())
    return net::USER_CERT;
  if (trust.HasPeer(PR_TRUE, PR_FALSE, PR_FALSE))
    return net::SERVER_CERT;
  if (trust.HasAnyCA() || CERT_IsCACert(cert->os_cert_handle(), NULL))
    return net::CA_CERT;
  return net::UNKNOWN_CERT;
}

}  // namespace

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
        crypto::OpenTestNSSDB(temp_db_dir_.Get().path(), g_token_name));
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

  void TestProxySettings(const std::string proxy_settings_blob,
                         net::ProxyConfig* net_config);

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
  ScopedStubCrosEnabler stub_cros_enabler_;
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

void OncNetworkParserTest::TestProxySettings(
    const std::string proxy_settings_blob,
    net::ProxyConfig* net_config) {
  std::string test_blob(
    std::string(kNetworkConfigurationWifiWepPskBegin) +
    "    }," +
    "    \"ProxySettings\": {" +
    proxy_settings_blob +
    std::string(kNetworkConfigurationWifiWepPskEnd));

  // Parse Network Configuration including ProxySettings dictionary.
  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);
  scoped_ptr<Network> network(parser.ParseNetwork(0));
  ASSERT_TRUE(network.get());
  EXPECT_FALSE(network->proxy_config().empty());

  // Deserialize ProxyConfig string property of Network into
  // ProxyConfigDictionary and decode into net::ProxyConfig.
  JSONStringValueSerializer serializer(network->proxy_config());
  scoped_ptr<Value> value(serializer.Deserialize(NULL, NULL));
  EXPECT_TRUE(value.get() && value->GetType() == Value::TYPE_DICTIONARY);
  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  ProxyConfigDictionary proxy_dict(dict);
  EXPECT_TRUE(PrefProxyConfigTrackerImpl::PrefConfigToNetConfig(proxy_dict,
                                                                net_config));
}

// static
base::LazyInstance<ScopedTempDir> OncNetworkParserTest::temp_db_dir_ =
    LAZY_INSTANCE_INITIALIZER;

TEST_F(OncNetworkParserTest, TestCreateNetworkWifi1) {
  std::string test_blob(
    std::string(kNetworkConfigurationWifiWepPskBegin) +
    std::string(kNetworkConfigurationWifiWepPskEnd));

  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
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

TEST_F(OncNetworkParserTest, TestLoadEncryptedOnc) {
  std::string test_blob(
      "{"
      "  \"Cipher\": \"AES256\","
      "  \"Ciphertext\": \"eQ9/r6v29/83M745aa0JllEj4lklt3Nfy4kPPvXgjBt1eTBy"
      "xXB+FnsdvL6Uca5JBU5aROxfiol2+ZZOkxPmUNNIFZj70pkdqOGVe09ncf0aVBDsAa27"
      "veGIG8rG/VQTTbAo7d8QaxdNNbZvwQVkdsAXawzPCu7zSh4NF/hDnDbYjbN/JEm1NzvW"
      "gEjeOfqnnw3PnGUYCArIaRsKq9uD0a1NccU+16ZSzyDhX724JNrJjsuxohotk5YXsCK0"
      "lP7ZXuXj+nSR0aRIETSQ+eqGhrew2octLXq8cXK05s6ZuVAc0mFKPkntSI/fzBACuPi4"
      "ZaGd3YEYiKzNOgKJ+qEwgoE39xp0EXMZOZyjMOAtA6e1ZZDQGWG7vKdTLmLKNztHGrXv"
      "lZkyEf1RDs10YgkwwLgUhm0yBJ+eqbxO/RiBXz7O2/UVOkkkVcmeI6yh3BdL6HIYsMMy"
      "gnZa5WRkd/2/EudoqEnjcqUyGsL+YUqV6KRTC0PH+z7zSwvFs2KygrSM7SIAZM2yiQHT"
      "QACkA/YCJDwACkkQOBFnRWTWiX0xmN55WMbgrs/wqJ4zGC9LgdAInOBlc3P+76+i7QLa"
      "NjMovQ==\","
      "  \"HMAC\": \"3ylRy5InlhVzFGakJ/9lvGSyVH0=\","
      "  \"HMACMethod\": \"SHA1\","
      "  \"IV\": \"hcm6OENfqG6C/TVO6p5a8g==\","
      "  \"Iterations\": 20000,"
      "  \"Salt\": \"/3O73QadCzA=\","
      "  \"Stretch\": \"PBKDF2\","
      "  \"Type\": \"EncryptedConfiguration\""
      "}");
  OncNetworkParser parser(test_blob,
                          "test0000",
                          NetworkUIData::ONC_SOURCE_USER_IMPORT);
  ASSERT_TRUE(parser.parse_error().empty());
  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
  scoped_ptr<Network> network(parser.ParseNetwork(0));
  ASSERT_TRUE(network.get());

  EXPECT_EQ(network->type(), chromeos::TYPE_WIFI);
  WifiNetwork* wifi = static_cast<WifiNetwork*>(network.get());
  EXPECT_EQ(wifi->encryption(), chromeos::SECURITY_NONE);
  EXPECT_EQ(wifi->name(), "WirelessNetwork");
  EXPECT_EQ(wifi->auto_connect(), false);
  EXPECT_EQ(wifi->passphrase(), "");
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
  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
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
  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
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

TEST_F(OncNetworkParserTest, TestCreateNetworkUnknownFields) {
  std::string test_blob(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"WiFi\": {"
      "      \"Security\": \"WEP-PSK\","
      "      \"SSID\": \"ssid\","
      "      \"Passphrase\": \"pass\","
      "    },"
      "    \"UnknownField1\": \"Value1\","
      "    \"UnknownField2\": {"
      "      \"UnknownSubField\": \"Value2\""
      "    },"
      "  }]"
      "}");
  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);
  scoped_ptr<Network> network(parser.ParseNetwork(0));
  ASSERT_TRUE(network.get());

  EXPECT_EQ(network->type(), chromeos::TYPE_WIFI);
  WifiNetwork* wifi = static_cast<WifiNetwork*>(network.get());
  EXPECT_EQ(wifi->encryption(), chromeos::SECURITY_WEP);
  EXPECT_EQ(wifi->name(), "ssid");
  EXPECT_EQ(wifi->passphrase(), "pass");
}

TEST_F(OncNetworkParserTest, TestCreateNetworkOpenVPN) {
  std::string test_blob(std::string(
      "{"
      "  \"NetworkConfigurations\": [") +
      std::string(kNetworkConfigurationOpenVPN) + std::string(
      "  ]}"));
  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);

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
  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);

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

TEST_F(OncNetworkParserTest, TestAddClientCertificate) {
  std::string certificate_json(
      "{"
      "    \"Certificates\": ["
      + std::string(kCertificateClient) +
      "    ],"
      "}");
  std::string test_guid("{f998f760-272b-6939-4c2beffe428697ac}");
  OncNetworkParser parser(certificate_json, "",
                          NetworkUIData::ONC_SOURCE_USER_IMPORT);
  ASSERT_EQ(1, parser.GetCertificatesSize());

  scoped_refptr<net::X509Certificate> cert = parser.ParseCertificate(0).get();
  EXPECT_TRUE(cert.get() != NULL);
  EXPECT_EQ(net::USER_CERT, GetCertType(cert.get()));

  EXPECT_STREQ(test_guid.c_str(),
               cert->GetDefaultNickname(net::USER_CERT).c_str());
  net::CertificateList result_list;
  OncNetworkParser::ListCertsWithNickname(test_guid, &result_list);
  ASSERT_EQ(1ul, result_list.size());
  EXPECT_EQ(net::USER_CERT, GetCertType(result_list[0].get()));

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(slot_->os_module_handle(), NULL, NULL);
  EXPECT_TRUE(privkey_list);
  if (privkey_list) {
    SECKEYPrivateKeyListNode* node = PRIVKEY_LIST_HEAD(privkey_list);
    int count = 0;
    while (!PRIVKEY_LIST_END(node, privkey_list)) {
      char* name = PK11_GetPrivateKeyNickname(node->key);
      EXPECT_STREQ(test_guid.c_str(), name);
      PORT_Free(name);
      count++;
      node = PRIVKEY_LIST_NEXT(node);
    }
    EXPECT_EQ(1, count);
    SECKEY_DestroyPrivateKeyList(privkey_list);
  }

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(slot_->os_module_handle(), NULL);
  EXPECT_TRUE(pubkey_list);
  if (pubkey_list) {
    SECKEYPublicKeyListNode* node = PUBKEY_LIST_HEAD(pubkey_list);
    int count = 0;
    while (!PUBKEY_LIST_END(node, pubkey_list)) {
      count++;
      node = PUBKEY_LIST_NEXT(node);
    }
    EXPECT_EQ(1, count);
    SECKEY_DestroyPublicKeyList(pubkey_list);
  }
}

TEST_F(OncNetworkParserTest, TestReplaceClientCertificate) {
  std::string certificate_json(
      "{"
      "    \"Certificates\": ["
      + std::string(kCertificateClient) +
      "    ],"
      "}");

  std::string certificate_alternate_json(
      "{"
      "    \"Certificates\": ["
      + std::string(kCertificateClientAlternate) +
      "    ],"
      "}");

  std::string test_guid("{f998f760-272b-6939-4c2beffe428697ac}");
  {
    // First we import a certificate.
    OncNetworkParser parser(certificate_json, "",
                            NetworkUIData::ONC_SOURCE_USER_IMPORT);
    ASSERT_EQ(1, parser.GetCertificatesSize());

    scoped_refptr<net::X509Certificate> cert = parser.ParseCertificate(0).get();
    ASSERT_TRUE(cert.get() != NULL);
    EXPECT_EQ(net::USER_CERT, GetCertType(cert.get()));

    EXPECT_STREQ(test_guid.c_str(),
                 cert->GetDefaultNickname(net::USER_CERT).c_str());
    net::CertificateList result_list;
    OncNetworkParser::ListCertsWithNickname(test_guid, &result_list);
    ASSERT_EQ(1ul, result_list.size());
    EXPECT_EQ(net::USER_CERT, GetCertType(result_list[0].get()));
  }

  {
    // Now we import a new certificate with the same GUID as the
    // first.  It should replace the old one.
    OncNetworkParser parser(certificate_alternate_json, "",
                            NetworkUIData::ONC_SOURCE_USER_IMPORT);
    ASSERT_EQ(1, parser.GetCertificatesSize());
    scoped_refptr<net::X509Certificate> cert = parser.ParseCertificate(0).get();
    ASSERT_TRUE(cert.get() != NULL);

    EXPECT_STREQ(test_guid.c_str(),
                 cert->GetDefaultNickname(net::USER_CERT).c_str());
    net::CertificateList result_list;
    OncNetworkParser::ListCertsWithNickname(test_guid, &result_list);
    ASSERT_EQ(1ul, result_list.size());
    EXPECT_EQ(net::USER_CERT, GetCertType(result_list[0].get()));
  }}

TEST_F(OncNetworkParserTest, TestAddServerCertificate) {
  std::string test_blob(
      "{"
      "    \"Certificates\": ["
      + std::string(kCertificateServer) +
      "    ],"
      "}");
  std::string test_guid("{f998f760-272b-6939-4c2beffe428697aa}");
  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);
  ASSERT_EQ(1, parser.GetCertificatesSize());

  scoped_refptr<net::X509Certificate> cert = parser.ParseCertificate(0).get();
  EXPECT_TRUE(cert.get() != NULL);
  EXPECT_EQ(net::SERVER_CERT, GetCertType(cert.get()));

  EXPECT_STREQ(test_guid.c_str(),
               cert->GetDefaultNickname(net::SERVER_CERT).c_str());
  net::CertificateList result_list;
  OncNetworkParser::ListCertsWithNickname(test_guid, &result_list);
  ASSERT_EQ(1ul, result_list.size());
  EXPECT_EQ(net::SERVER_CERT, GetCertType(result_list[0].get()));

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(slot_->os_module_handle(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(slot_->os_module_handle(), NULL);
  EXPECT_FALSE(pubkey_list);

}

TEST_F(OncNetworkParserTest, TestReplaceServerCertificate) {
  std::string certificate_json(
      "{"
      "    \"Certificates\": ["
      + std::string(kCertificateServer) +
      "    ],"
      "}");

  std::string certificate_alternate_json(
      "{"
      "    \"Certificates\": ["
      + std::string(kCertificateServerAlternate) +
      "    ],"
      "}");
  std::string test_guid("{f998f760-272b-6939-4c2beffe428697aa}");
  {
    // First we import a certificate.
    OncNetworkParser parser(certificate_json, "",
                            NetworkUIData::ONC_SOURCE_USER_IMPORT);
    ASSERT_EQ(1, parser.GetCertificatesSize());

    scoped_refptr<net::X509Certificate> cert = parser.ParseCertificate(0).get();
    ASSERT_TRUE(cert.get() != NULL);
    EXPECT_EQ(net::SERVER_CERT, GetCertType(cert.get()));

    EXPECT_STREQ(test_guid.c_str(),
                 cert->GetDefaultNickname(net::SERVER_CERT).c_str());
    net::CertificateList result_list;
    OncNetworkParser::ListCertsWithNickname(test_guid, &result_list);
    ASSERT_EQ(1ul, result_list.size());
    EXPECT_EQ(net::SERVER_CERT, GetCertType(result_list[0].get()));
  }

  {
    // Now we import a new certificate with the same GUID as the
    // first.  It should replace the old one.
    OncNetworkParser parser(certificate_alternate_json, "",
                            NetworkUIData::ONC_SOURCE_USER_IMPORT);
    ASSERT_EQ(1, parser.GetCertificatesSize());
    scoped_refptr<net::X509Certificate> cert = parser.ParseCertificate(0).get();
    ASSERT_TRUE(cert.get() != NULL);

    EXPECT_STREQ(test_guid.c_str(),
                 cert->GetDefaultNickname(net::SERVER_CERT).c_str());
    net::CertificateList result_list;
    OncNetworkParser::ListCertsWithNickname(test_guid, &result_list);
    ASSERT_EQ(1ul, result_list.size());
    EXPECT_EQ(net::SERVER_CERT, GetCertType(result_list[0].get()));
  }
}


TEST_F(OncNetworkParserTest, TestAddAuthorityCertificate) {
  const std::string test_blob("{"
      "    \"Certificates\": [" +
      std::string(kCertificateWebAuthority) +
      "    ],"
      "}");
  std::string test_guid("{f998f760-272b-6939-4c2beffe428697ab}");
  OncNetworkParser parser(test_blob, "", NetworkUIData::ONC_SOURCE_USER_IMPORT);
  ASSERT_EQ(1, parser.GetCertificatesSize());

  scoped_refptr<net::X509Certificate> cert = parser.ParseCertificate(0).get();
  EXPECT_TRUE(cert.get() != NULL);
  EXPECT_EQ(net::CA_CERT, GetCertType(cert.get()));

  EXPECT_STREQ(test_guid.c_str(),
               cert->GetDefaultNickname(net::CA_CERT).c_str());
  net::CertificateList result_list;
  OncNetworkParser::ListCertsWithNickname(test_guid, &result_list);
  ASSERT_EQ(1ul, result_list.size());
  EXPECT_EQ(net::CA_CERT, GetCertType(result_list[0].get()));

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(slot_->os_module_handle(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(slot_->os_module_handle(), NULL);
  EXPECT_FALSE(pubkey_list);
}


TEST_F(OncNetworkParserTest, TestReplaceAuthorityCertificate) {
  const std::string authority_json("{"
      "    \"Certificates\": [" +
      std::string(kCertificateWebAuthority) +
      "    ],"
      "}");
  std::string authority_alternate_json(
      "{"
      "  \"Certificates\": [" +
      std::string(kCertificateWebAuthorityAlternate) +
      "  ]"
      "}");
  std::string test_guid("{f998f760-272b-6939-4c2beffe428697ab}");

  {
    // First we import an authority certificate.
    OncNetworkParser parser(authority_json, "",
                            NetworkUIData::ONC_SOURCE_USER_IMPORT);
    ASSERT_EQ(1, parser.GetCertificatesSize());

    scoped_refptr<net::X509Certificate> cert = parser.ParseCertificate(0).get();
    ASSERT_TRUE(cert.get() != NULL);
    EXPECT_EQ(net::CA_CERT, GetCertType(cert.get()));

    EXPECT_STREQ(test_guid.c_str(),
                 cert->GetDefaultNickname(net::CA_CERT).c_str());
    net::CertificateList result_list;
    OncNetworkParser::ListCertsWithNickname(test_guid, &result_list);
    ASSERT_EQ(1ul, result_list.size());
    EXPECT_EQ(net::CA_CERT, GetCertType(result_list[0].get()));
  }

  {
    // Now we import a new authority certificate with the same GUID as the
    // first.  It should replace the old one.
    OncNetworkParser parser(authority_alternate_json, "",
                            NetworkUIData::ONC_SOURCE_USER_IMPORT);
    ASSERT_EQ(1, parser.GetCertificatesSize());
    scoped_refptr<net::X509Certificate> cert = parser.ParseCertificate(0).get();
    ASSERT_TRUE(cert.get() != NULL);

    EXPECT_STREQ(test_guid.c_str(),
                 cert->GetDefaultNickname(net::CA_CERT).c_str());
    net::CertificateList result_list;
    OncNetworkParser::ListCertsWithNickname(test_guid, &result_list);
    ASSERT_EQ(1ul, result_list.size());
    EXPECT_EQ(net::CA_CERT, GetCertType(result_list[0].get()));
  }
}

TEST_F(OncNetworkParserTest, TestNetworkAndCertificate) {
  std::string test_blob(
      "{"
      "  \"NetworkConfigurations\": [" +
      std::string(kNetworkConfigurationOpenVPN) +
      "  ],"
      "  \"Certificates\": [" +
      std::string(kCertificateWebAuthority) +
      "  ],"
      "}");
  OncNetworkParser parser(test_blob, "",
                          NetworkUIData::ONC_SOURCE_USER_IMPORT);

  EXPECT_EQ(1, parser.GetCertificatesSize());
  EXPECT_TRUE(parser.ParseCertificate(0));

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  scoped_ptr<Network> network(parser.ParseNetwork(0));
  ASSERT_TRUE(network.get() != NULL);
  EXPECT_EQ(network->type(), chromeos::TYPE_VPN);
  VirtualNetwork* vpn = static_cast<VirtualNetwork*>(network.get());
  EXPECT_EQ(PROVIDER_TYPE_OPEN_VPN, vpn->provider_type());
}

TEST_F(OncNetworkParserTest, TestProxySettingsDirect) {
  std::string proxy_settings_blob(
    "      \"Type\": \"Direct\"");

  net::ProxyConfig net_config;
  TestProxySettings(proxy_settings_blob, &net_config);
  EXPECT_EQ(net::ProxyConfig::ProxyRules::TYPE_NO_RULES,
            net_config.proxy_rules().type);
  EXPECT_FALSE(net_config.HasAutomaticSettings());
}

TEST_F(OncNetworkParserTest, TestProxySettingsWpad) {
  std::string proxy_settings_blob(
    "      \"Type\": \"WPAD\"");

  net::ProxyConfig net_config;
  TestProxySettings(proxy_settings_blob, &net_config);
  EXPECT_EQ(net::ProxyConfig::ProxyRules::TYPE_NO_RULES,
            net_config.proxy_rules().type);
  EXPECT_TRUE(net_config.HasAutomaticSettings());
  EXPECT_TRUE(net_config.auto_detect());
  EXPECT_FALSE(net_config.has_pac_url());
}

TEST_F(OncNetworkParserTest, TestProxySettingsPac) {
  std::string pac_url("http://proxyconfig.corp.google.com/wpad.dat");
  std::string proxy_settings_blob(
    "      \"Type\": \"PAC\","
    "      \"PAC\": \"" + pac_url + std::string("\""));

  net::ProxyConfig net_config;
  TestProxySettings(proxy_settings_blob, &net_config);
  EXPECT_EQ(net::ProxyConfig::ProxyRules::TYPE_NO_RULES,
            net_config.proxy_rules().type);
  EXPECT_TRUE(net_config.HasAutomaticSettings());
  EXPECT_FALSE(net_config.auto_detect());
  EXPECT_TRUE(net_config.has_pac_url());
  EXPECT_EQ(GURL(pac_url), net_config.pac_url());
}

TEST_F(OncNetworkParserTest, TestProxySettingsManual) {
  std::string http_host("http.abc.com");
  std::string https_host("https.abc.com");
  std::string ftp_host("ftp.abc.com");
  std::string socks_host("socks5://socks.abc.com");
  uint16 http_port = 1234;
  uint16 https_port = 3456;
  uint16 ftp_port = 5678;
  uint16 socks_port = 7890;
  std::string proxy_settings_blob(
    "      \"Type\": \"Manual\","
    "      \"Manual\": {"
    "        \"HTTPProxy\" : {"
    "          \"Host\" : \"" + http_host + "\","
    "          \"Port\" : %d"
    "        },"
    "        \"SecureHTTPProxy\" : {"
    "          \"Host\" : \"" + https_host + "\","
    "          \"Port\" : %d"
    "        },"
    "        \"FTPProxy\" : {"
    "          \"Host\" : \"" + ftp_host + "\","
    "          \"Port\" : %d"
    "        },"
    "        \"SOCKS\" : {"
    "          \"Host\" : \"" + socks_host + "\","
    "          \"Port\" : %d"
    "        }"
    "      },"
    "      \"ExcludeDomains\": ["
    "        \"google.com\","
    "        \"<local>\""
    "      ]");

  net::ProxyConfig net_config;
  TestProxySettings(
      base::StringPrintf(proxy_settings_blob.data(),
                         http_port, https_port, ftp_port, socks_port),
      &net_config);
  const net::ProxyConfig::ProxyRules& rules = net_config.proxy_rules();
  EXPECT_EQ(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME, rules.type);
  // Verify http proxy server.
  EXPECT_TRUE(rules.proxy_for_http.is_valid());
  EXPECT_EQ(rules.proxy_for_http,
            net::ProxyServer(net::ProxyServer::SCHEME_HTTP,
                             net::HostPortPair(http_host, http_port)));
  // Verify https proxy server.
  EXPECT_TRUE(rules.proxy_for_https.is_valid());
  EXPECT_EQ(rules.proxy_for_https,
            net::ProxyServer(net::ProxyServer::SCHEME_HTTP,
                             net::HostPortPair(https_host, https_port)));
  // Verify ftp proxy server.
  EXPECT_TRUE(rules.proxy_for_ftp.is_valid());
  EXPECT_EQ(rules.proxy_for_ftp,
            net::ProxyServer(net::ProxyServer::SCHEME_HTTP,
                             net::HostPortPair(ftp_host, ftp_port)));
  // Verify socks server.
  EXPECT_TRUE(rules.fallback_proxy.is_valid());
  EXPECT_EQ(rules.fallback_proxy,
            net::ProxyServer(net::ProxyServer::SCHEME_SOCKS5,
                             net::HostPortPair(socks_host, socks_port)));
  // Verify bypass rules.
  net::ProxyBypassRules expected_bypass_rules;
  expected_bypass_rules.AddRuleFromString("google.com");
  expected_bypass_rules.AddRuleToBypassLocal();
  EXPECT_TRUE(expected_bypass_rules.Equals(rules.bypass_rules));
}

}  // namespace chromeos
