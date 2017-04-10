// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/utility/payment_manifest_parser.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

// Payment method manifest parsing:

TEST(PaymentManifestParserTest, NullPaymentMethodManifestIsMalformed) {
  EXPECT_TRUE(
      PaymentManifestParser::ParsePaymentMethodManifestIntoVector(std::string())
          .empty());
}

TEST(PaymentManifestParserTest, NonJsonPaymentMethodManifestIsMalformed) {
  EXPECT_TRUE(PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
                  "this is not json")
                  .empty());
}

TEST(PaymentManifestParserTest, StringPaymentMethodManifestIsMalformed) {
  EXPECT_TRUE(PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
                  "\"this is a string\"")
                  .empty());
}

TEST(PaymentManifestParserTest,
     EmptyDictionaryPaymentMethodManifestIsMalformed) {
  EXPECT_TRUE(PaymentManifestParser::ParsePaymentMethodManifestIntoVector("{}")
                  .empty());
}

TEST(PaymentManifestParserTest, NullDefaultApplicationIsMalformed) {
  EXPECT_TRUE(PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
                  "{\"default_applications\": null}")
                  .empty());
}

TEST(PaymentManifestParserTest, NumberDefaultApplicationIsMalformed) {
  EXPECT_TRUE(PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
                  "{\"default_applications\": 0}")
                  .empty());
}

TEST(PaymentManifestParserTest, ListOfNumbersDefaultApplicationIsMalformed) {
  EXPECT_TRUE(PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
                  "{\"default_applications\": [0]}")
                  .empty());
}

TEST(PaymentManifestParserTest, EmptyListOfDefaultApplicationsIsMalformed) {
  EXPECT_TRUE(PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
                  "{\"default_applications\": []}")
                  .empty());
}

TEST(PaymentManifestParserTest, ListOfEmptyDefaultApplicationsIsMalformed) {
  EXPECT_TRUE(PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
                  "{\"default_applications\": [\"\"]}")
                  .empty());
}

TEST(PaymentManifestParserTest, DefaultApplicationsShouldNotHaveNulCharacters) {
  EXPECT_TRUE(
      PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
          "{\"default_applications\": [\"https://bobpay.com/app\0json\"]}")
          .empty());
}

TEST(PaymentManifestParserTest, DefaultApplicationKeyShouldBeLowercase) {
  EXPECT_TRUE(
      PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
          "{\"Default_Applications\": [\"https://bobpay.com/app.json\"]}")
          .empty());
}

TEST(PaymentManifestParserTest, DefaultApplicationsShouldBeAbsoluteUrls) {
  EXPECT_TRUE(PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
                  "{\"default_applications\": ["
                  "\"https://bobpay.com/app.json\","
                  "\"app.json\"]}")
                  .empty());
}

TEST(PaymentManifestParserTest, DefaultApplicationsShouldBeHttps) {
  EXPECT_TRUE(PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
                  "{\"default_applications\": ["
                  "\"https://bobpay.com/app.json\","
                  "\"http://alicepay.com/app.json\"]}")
                  .empty());
}

TEST(PaymentManifestParserTest, WellFormedPaymentMethodManifest) {
  std::vector<GURL> expected = {GURL("https://bobpay.com/app.json"),
                                GURL("https://alicepay.com/app.json")};
  EXPECT_EQ(expected,
            PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
                "{\"default_applications\": ["
                "\"https://bobpay.com/app.json\","
                "\"https://alicepay.com/app.json\"]}"));
}

// Web app manifest parsing:

void ExpectUnableToParse(const std::string& input) {
  std::vector<mojom::WebAppManifestSectionPtr> actual_output =
      PaymentManifestParser::ParseWebAppManifestIntoVector(input);
  EXPECT_TRUE(actual_output.empty());
}

void ExpectParsed(
    const std::string& input,
    const std::string& expected_id,
    int64_t expected_min_version,
    const std::vector<std::vector<uint8_t>>& expected_fingerprints) {
  std::vector<mojom::WebAppManifestSectionPtr> actual_output =
      PaymentManifestParser::ParseWebAppManifestIntoVector(input);
  ASSERT_EQ(1U, actual_output.size());
  EXPECT_EQ(expected_id, actual_output.front()->id);
  EXPECT_EQ(expected_min_version, actual_output.front()->min_version);
  EXPECT_EQ(expected_fingerprints, actual_output.front()->fingerprints);
}

TEST(PaymentManifestParserTest, NullContentIsMalformed) {
  ExpectUnableToParse(std::string());
}

TEST(PaymentManifestParserTest, NonJsonContentIsMalformed) {
  ExpectUnableToParse("this is not json");
}

TEST(PaymentManifestParserTest, StringContentIsMalformed) {
  ExpectUnableToParse("\"this is a string\"");
}

TEST(PaymentManifestParserTest, EmptyDictionaryIsMalformed) {
  ExpectUnableToParse("{}");
}

TEST(PaymentManifestParserTest, NullRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParse("{\"related_applications\": null}");
}

TEST(PaymentManifestParserTest, NumberRelatedApplicationsectionIsMalformed) {
  ExpectUnableToParse("{\"related_applications\": 0}");
}

TEST(PaymentManifestParserTest,
     ListOfNumbersRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParse("{\"related_applications\": [0]}");
}

TEST(PaymentManifestParserTest,
     ListOfEmptyDictionariesRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParse("{\"related_applications\": [{}]}");
}

TEST(PaymentManifestParserTest, NoPlayPlatformIsMalformed) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, NoPackageNameIsMalformed) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, NoVersionIsMalformed) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, NoFingerprintIsMalformed) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\""
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, EmptyFingerprintsIsMalformed) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": []"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, EmptyFingerprintsDictionaryIsMalformed) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{}]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, NoFingerprintTypeIsMalformed) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, NoFingerprintValueIsMalformed) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, PlatformShouldNotHaveNullCharacters) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"pl\0ay\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, PackageNameShouldNotHaveNullCharacters) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bob\0pay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, VersionShouldNotHaveNullCharacters) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\01\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintTypeShouldNotHaveNullCharacters) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_c\0ert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintValueShouldNotHaveNullCharacters) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": "
      "\"00:01:02:0\0:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, KeysShouldBeLowerCase) {
  ExpectUnableToParse(
      "{"
      "  \"Related_applications\": [{"
      "    \"Platform\": \"play\", "
      "    \"Id\": \"com.bobpay.app\", "
      "    \"Min_version\": \"1\", "
      "    \"Fingerprints\": [{"
      "      \"Type\": \"sha256_cert\", "
      "      \"Value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintsShouldBeSha256) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha1_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintBytesShouldBeColonSeparated) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\" \"00010203040506070809A0A1A2A3A4A5A6A7A8A9B0B1B2B3B4B5B6"
      "B7B8B9C0C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintBytesShouldBeUpperCase) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:a0:a1:a2:a3:a4:a5:a6:a7"
      ":a8:a9:b0:b1:b2:b3:b4:b5:b6:b7:b8:b9:c0:c1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintsShouldContainsThirtyTwoBytes) {
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1:C2\""
      "    }]"
      "  }]"
      "}");
  ExpectUnableToParse(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, WellFormed) {
  ExpectParsed(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}",
      "com.bobpay.app", 1,
      {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1}});
}

TEST(PaymentManifestParserTest, DuplicateSignaturesWellFormed) {
  ExpectParsed(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }, {"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}",
      "com.bobpay.app", 1,
      {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1},
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1}});
}

}  // namespace
}  // namespace payments
