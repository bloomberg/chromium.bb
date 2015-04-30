// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mime_util/mime_util.h"

#include "build/build_config.h"
#include "net/base/mime_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mime_util {

TEST(MimeUtilTest, LookupTypes) {
  EXPECT_FALSE(IsUnsupportedTextMimeType("text/banana"));
  EXPECT_TRUE(IsUnsupportedTextMimeType("text/vcard"));

  EXPECT_TRUE(IsSupportedImageMimeType("image/jpeg"));
  EXPECT_TRUE(IsSupportedImageMimeType("Image/JPEG"));
  EXPECT_FALSE(IsSupportedImageMimeType("image/lolcat"));
  EXPECT_FALSE(IsSupportedImageMimeType("Image/LolCat"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/html"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/css"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("text/banana"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("Text/Banana"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("text/vcard"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("application/virus"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("Application/VIRUS"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/x-x509-user-cert"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/json"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/+json"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/x-suggestions+json"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/x-s+json;x=2"));
#if defined(OS_ANDROID)
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/x-x509-ca-cert"));
  EXPECT_TRUE(IsSupportedNonImageMimeType("application/x-pkcs12"));
#if 0  // Disabled until http://crbug.com/318217 is resolved.
  EXPECT_TRUE(IsSupportedMediaMimeType("application/vnd.apple.mpegurl"));
  EXPECT_TRUE(IsSupportedMediaMimeType("application/x-mpegurl"));
  EXPECT_TRUE(IsSupportedMediaMimeType("Application/X-MPEGURL"));
#endif
#endif

  EXPECT_TRUE(IsSupportedMimeType("image/jpeg"));
  EXPECT_FALSE(IsSupportedMimeType("image/lolcat"));
  EXPECT_FALSE(IsSupportedMimeType("Image/LOLCAT"));
  EXPECT_TRUE(IsSupportedMimeType("text/html"));
  EXPECT_TRUE(IsSupportedMimeType("text/banana"));
  EXPECT_TRUE(IsSupportedMimeType("Text/BANANA"));
  EXPECT_FALSE(IsSupportedMimeType("text/vcard"));
  EXPECT_FALSE(IsSupportedMimeType("application/virus"));
  EXPECT_FALSE(IsSupportedMimeType("application/x-json"));
  EXPECT_FALSE(IsSupportedMimeType("Application/X-JSON"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("application/vnd.doc;x=y+json"));
  EXPECT_FALSE(IsSupportedNonImageMimeType("Application/VND.DOC;X=Y+JSON"));
}

TEST(MimeUtilTest, TestGetCertificateMimeTypeForMimeType) {
  EXPECT_EQ(net::CERTIFICATE_MIME_TYPE_X509_USER_CERT,
            GetCertificateMimeTypeForMimeType("application/x-x509-user-cert"));
  EXPECT_EQ(net::CERTIFICATE_MIME_TYPE_X509_USER_CERT,
            GetCertificateMimeTypeForMimeType("Application/X-X509-USER-CERT"));
#if defined(OS_ANDROID)
  // Only Android supports CA Certs and PKCS12 archives.
  EXPECT_EQ(net::CERTIFICATE_MIME_TYPE_X509_CA_CERT,
            GetCertificateMimeTypeForMimeType("application/x-x509-ca-cert"));
  EXPECT_EQ(net::CERTIFICATE_MIME_TYPE_PKCS12_ARCHIVE,
            GetCertificateMimeTypeForMimeType("application/x-pkcs12"));
#else
  EXPECT_EQ(net::CERTIFICATE_MIME_TYPE_UNKNOWN,
            GetCertificateMimeTypeForMimeType("application/x-x509-ca-cert"));
  EXPECT_EQ(net::CERTIFICATE_MIME_TYPE_UNKNOWN,
            GetCertificateMimeTypeForMimeType("application/x-pkcs12"));
#endif
  EXPECT_EQ(net::CERTIFICATE_MIME_TYPE_UNKNOWN,
            GetCertificateMimeTypeForMimeType("text/plain"));
}

}  // namespace mime_util
