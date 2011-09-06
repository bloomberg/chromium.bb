// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mobile_config.h"

#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kBadManifest[] = "{\"version\": \"1\"}";

const char kGoodMobileConfig[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"carriers\" : {\n"
    "    \"carrier (country)\" : {\n"
    "      \"ids\" : [\n"
    "        {\n"
    "          \"id\": \"cr (country)\",\n"
    "          \"_comment\" : \"Based on SPN.\",\n"
    "        },\n"
    "        {\n"
    "          \"id\": \"Carrier (country)\",\n"
    "          \"_comment\" : \"Legacy ID when SPN is empty.\",\n"
    "        },\n"
    "      ],\n"
    "      \"top_up_url\" : \"http://www.carrier.com/\",\n"
    "      \"deals\" : [\n"
    "        {\n"
    "          \"deal_id\" : \"0\",\n"
    "          \"locales\" : [ \"en-US\", ],\n"
    "          \"expire_date\" : \"31/12/12 0:0\",\n"
    "          \"notification_count\" : 1,\n"
    "          \"localized_content\" : {\n"
    "            \"en-US\" : {\n"
    "              \"notification_text\" : \"3G connectivity : Carrier.\",\n"
    "            },\n"
    "            \"default\" : {\n"
    "              \"notification_text\" : \"default_text.\",\n"
    "            },\n"
    "          },\n"
    "        },\n"
    "      ],\n"
    "    },"
    "  },"
    "}";

const char kOldDealMobileConfig[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"carriers\" : {\n"
     "    \"Carrier (country)\" : {\n"
     "      \"top_up_url\" : \"http://www.carrier.com/\",\n"
     "      \"deals\" : [\n"
     "        {\n"
     "          \"deal_id\" : \"0\",\n"
     "          \"locales\" : [ \"en-US\", ],\n"
     "          \"expire_date\" : \"01/01/01 0:0\",\n"
     "          \"notification_count\" : 1,\n"
     "          \"localized_content\" : {\n"
     "            \"en-US\" : {\n"
     "              \"notification_text\" : \"3G connectivity : Carrier.\",\n"
     "            },\n"
     "            \"default\" : {\n"
     "              \"notification_text\" : \"default_text.\",\n"
     "            },\n"
     "          },\n"
     "        },\n"
     "      ],\n"
     "    },"
     "  },"
    "}";

const char kLocalMobileConfigNoDeals[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"exclude_deals\": true,"
    "  \"carriers\" : {\n"
     "  },"
    "}";

const char kLocalMobileConfig[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"carriers\" : {\n"
    "    \"carrier (country)\" : {\n"
    "      \"exclude_deals\": true,"
    "      \"top_up_url\" : \"http://www.carrier-new-url.com/\",\n"
    "      \"deals\" : [\n"
    "        {\n"
    "          \"deal_id\" : \"1\",\n"
    "          \"locales\" : [ \"en-GB\", ],\n"
    "          \"expire_date\" : \"31/12/13 0:0\",\n"
    "          \"notification_count\" : 2,\n"
    "          \"localized_content\" : {\n"
    "            \"en-GB\" : {\n"
    "              \"notification_text\" : \"3G connectivity : Carrier.\",\n"
    "            },\n"
    "            \"default\" : {\n"
    "              \"notification_text\" : \"default_text from local.\",\n"
    "            },\n"
    "          },\n"
    "        },\n"
    "      ],\n"
    "    },"
     "  },"
    "}";

}  // anonymous namespace

namespace chromeos {

TEST(MobileConfigTest, Basic) {
  MobileConfig config(kGoodMobileConfig, "en-US");
  EXPECT_TRUE(config.IsReady());

  const MobileConfig::Carrier* carrier;
  carrier = config.GetCarrier("Carrier (country)");
  EXPECT_TRUE(carrier != NULL);
  carrier = config.GetCarrier("cr (country)");
  EXPECT_TRUE(carrier != NULL);
  EXPECT_EQ("http://www.carrier.com/", carrier->top_up_url());
  const MobileConfig::CarrierDeal* deal;
  deal = carrier->GetDefaultDeal();
  EXPECT_TRUE(deal != NULL);
  deal = carrier->GetDeal("0");
  EXPECT_TRUE(deal != NULL);
  EXPECT_EQ("en-US", deal->locales()[0]);
  EXPECT_EQ(1, deal->notification_count());
  EXPECT_EQ("3G connectivity : Carrier.",
            deal->GetLocalizedString("en-US", "notification_text"));
  EXPECT_EQ("default_text.",
            deal->GetLocalizedString("en", "notification_text"));

  base::Time reference_time;
  base::Time::FromString("31/12/12 0:00", &reference_time);
  EXPECT_EQ(reference_time, deal->expire_date());
}

TEST(MobileConfigTest, OldDeal) {
  MobileConfig config(kOldDealMobileConfig, "en-US");
  EXPECT_TRUE(config.IsReady());
  const MobileConfig::Carrier* carrier;
  carrier = config.GetCarrier("Carrier (country)");
  EXPECT_TRUE(carrier != NULL);
  const MobileConfig::CarrierDeal* deal;
  // TODO(nkostylev): Pass fixed time instead of relying on Time::Now().
  deal = carrier->GetDefaultDeal();
  EXPECT_TRUE(deal == NULL);
}

TEST(MobileConfigTest, DealOtherLocale) {
  MobileConfig config(kGoodMobileConfig, "en-GB");
  EXPECT_TRUE(config.IsReady());
  const MobileConfig::Carrier* carrier;
  carrier = config.GetCarrier("Carrier (country)");
  EXPECT_TRUE(carrier != NULL);
  const MobileConfig::CarrierDeal* deal;
  deal = carrier->GetDefaultDeal();
  EXPECT_TRUE(deal == NULL);
}

TEST(MobileConfigTest, BadManifest) {
  MobileConfig config(kBadManifest, "en-US");
  EXPECT_FALSE(config.IsReady());
}

TEST(MobileConfigTest, LocalConfigNoDeals) {
  MobileConfig config(kGoodMobileConfig, "en-US");
  EXPECT_TRUE(config.IsReady());
  config.LoadManifestFromString(kLocalMobileConfigNoDeals);
  EXPECT_TRUE(config.IsReady());
  const MobileConfig::Carrier* carrier;
  carrier = config.GetCarrier("Carrier (country)");
  EXPECT_TRUE(carrier != NULL);
  const MobileConfig::CarrierDeal* deal;
  deal = carrier->GetDefaultDeal();
  EXPECT_TRUE(deal == NULL);
  deal = carrier->GetDeal("0");
  EXPECT_TRUE(deal == NULL);
}

TEST(MobileConfigTest, LocalConfig) {
  MobileConfig config(kGoodMobileConfig, "en-GB");
  EXPECT_TRUE(config.IsReady());
  config.LoadManifestFromString(kLocalMobileConfig);
  EXPECT_TRUE(config.IsReady());

  const MobileConfig::Carrier* carrier;
  carrier = config.GetCarrier("Carrier (country)");
  EXPECT_TRUE(carrier != NULL);
  EXPECT_EQ("http://www.carrier-new-url.com/", carrier->top_up_url());

  const MobileConfig::CarrierDeal* deal;
  deal = carrier->GetDeal("0");
  EXPECT_TRUE(deal == NULL);
  deal = carrier->GetDefaultDeal();
  EXPECT_TRUE(deal != NULL);
  deal = carrier->GetDeal("1");
  EXPECT_TRUE(deal != NULL);
  EXPECT_EQ("en-GB", deal->locales()[0]);
  EXPECT_EQ(2, deal->notification_count());
  EXPECT_EQ("3G connectivity : Carrier.",
            deal->GetLocalizedString("en-GB", "notification_text"));
  EXPECT_EQ("default_text from local.",
            deal->GetLocalizedString("en", "notification_text"));
  base::Time reference_time;
  base::Time::FromString("31/12/13 0:00", &reference_time);
  EXPECT_EQ(reference_time, deal->expire_date());
}

}  // namespace chromeos
