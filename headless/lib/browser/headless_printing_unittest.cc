// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/values.h"
#include "headless/lib/browser/headless_devtools_manager_delegate.h"
#include "headless/lib/browser/headless_print_manager.h"
#include "printing/features/features.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

TEST(ParsePrintSettingsTest, Landscape) {
  HeadlessPrintSettings settings;
  EXPECT_FALSE(settings.landscape);

  auto params = std::make_unique<base::DictionaryValue>();
  params->SetBoolean("landscape", true);
  std::unique_ptr<base::DictionaryValue> response =
      ParsePrintSettings(0, params.get(), &settings);
  EXPECT_TRUE(settings.landscape);
  EXPECT_EQ(nullptr, response);
}

TEST(ParsePrintSettingsTest, HeaderFooter) {
  HeadlessPrintSettings settings;
  EXPECT_FALSE(settings.display_header_footer);

  auto params = std::make_unique<base::DictionaryValue>();
  params->SetBoolean("displayHeaderFooter", true);
  std::unique_ptr<base::DictionaryValue> response =
      ParsePrintSettings(0, params.get(), &settings);
  EXPECT_TRUE(settings.display_header_footer);
  EXPECT_EQ(nullptr, response);
}

TEST(ParsePrintSettingsTest, PrintBackground) {
  HeadlessPrintSettings settings;
  EXPECT_FALSE(settings.should_print_backgrounds);

  auto params = std::make_unique<base::DictionaryValue>();
  params->SetBoolean("printBackground", true);
  std::unique_ptr<base::DictionaryValue> response =
      ParsePrintSettings(0, params.get(), &settings);
  EXPECT_TRUE(settings.should_print_backgrounds);
  EXPECT_EQ(nullptr, response);
}

TEST(ParsePrintSettingsTest, Scale) {
  HeadlessPrintSettings settings;
  EXPECT_DOUBLE_EQ(1, settings.scale);

  auto params = std::make_unique<base::DictionaryValue>();
  params->SetDouble("scale", 0.5);
  std::unique_ptr<base::DictionaryValue> response =
      ParsePrintSettings(0, params.get(), &settings);
  EXPECT_DOUBLE_EQ(0.5, settings.scale);
  EXPECT_EQ(nullptr, response);

  params->SetDouble("scale", -1);
  response = ParsePrintSettings(0, params.get(), &settings);
  EXPECT_NE(nullptr, response);

  params->SetDouble("scale", 10);
  response = ParsePrintSettings(0, params.get(), &settings);
  EXPECT_NE(nullptr, response);
}

TEST(ParsePrintSettingsTest, PageRanges) {
  HeadlessPrintSettings settings;
  EXPECT_EQ("", settings.page_ranges);

  auto params = std::make_unique<base::DictionaryValue>();
  params->SetString("pageRanges", "abcd");
  params->SetBoolean("ignoreInvalidPageRanges", true);
  std::unique_ptr<base::DictionaryValue> response =
      ParsePrintSettings(0, params.get(), &settings);
  // Pass pageRanges text to settings directly and return no error, even if
  // it is invalid. The validation is deferred to HeadlessPrintMangager once the
  // total page count is available. See the PageRangeTextToPagesTest.
  EXPECT_EQ("abcd", settings.page_ranges);
  EXPECT_TRUE(settings.ignore_invalid_page_ranges);
  EXPECT_EQ(nullptr, response);
}

TEST(ParsePrintSettingsTest, Paper) {
  HeadlessPrintSettings settings;

  auto params = std::make_unique<base::DictionaryValue>();
  std::unique_ptr<base::DictionaryValue> response =
      ParsePrintSettings(0, params.get(), &settings);
  EXPECT_EQ(printing::kLetterWidthInch * printing::kPointsPerInch,
            settings.paper_size_in_points.width());
  EXPECT_EQ(printing::kLetterHeightInch * printing::kPointsPerInch,
            settings.paper_size_in_points.height());
  EXPECT_EQ(nullptr, response);

  params->SetDouble("paperWidth", 1);
  params->SetDouble("paperHeight", 2);
  response = ParsePrintSettings(0, params.get(), &settings);
  EXPECT_EQ(1 * printing::kPointsPerInch,
            settings.paper_size_in_points.width());
  EXPECT_EQ(2 * printing::kPointsPerInch,
            settings.paper_size_in_points.height());
  EXPECT_EQ(nullptr, response);

  params->SetDouble("paperWidth", -1);
  params->SetDouble("paperHeight", 2);
  response = ParsePrintSettings(0, params.get(), &settings);
  EXPECT_NE(nullptr, response);

  params->SetDouble("paperWidth", 1);
  params->SetDouble("paperHeight", -2);
  response = ParsePrintSettings(0, params.get(), &settings);
  EXPECT_NE(nullptr, response);
}

TEST(ParsePrintSettingsTest, Margin) {
  HeadlessPrintSettings settings;

  auto params = std::make_unique<base::DictionaryValue>();
  std::unique_ptr<base::DictionaryValue> response =
      ParsePrintSettings(0, params.get(), &settings);
  int default_margin =
      1000 * printing::kPointsPerInch / printing::kHundrethsMMPerInch;
  EXPECT_DOUBLE_EQ(default_margin, settings.margins_in_points.top);
  EXPECT_DOUBLE_EQ(default_margin, settings.margins_in_points.bottom);
  EXPECT_DOUBLE_EQ(default_margin, settings.margins_in_points.left);
  EXPECT_DOUBLE_EQ(default_margin, settings.margins_in_points.right);
  EXPECT_EQ(nullptr, response);

  params->SetDouble("marginTop", 1);
  params->SetDouble("marginBottom", 2);
  params->SetDouble("marginLeft", 3);
  params->SetDouble("marginRight", 4);
  response = ParsePrintSettings(0, params.get(), &settings);
  EXPECT_DOUBLE_EQ(1 * printing::kPointsPerInch,
                   settings.margins_in_points.top);
  EXPECT_DOUBLE_EQ(2 * printing::kPointsPerInch,
                   settings.margins_in_points.bottom);
  EXPECT_DOUBLE_EQ(3 * printing::kPointsPerInch,
                   settings.margins_in_points.left);
  EXPECT_DOUBLE_EQ(4 * printing::kPointsPerInch,
                   settings.margins_in_points.right);
  EXPECT_EQ(nullptr, response);

  params->SetDouble("marginTop", -1);
  response = ParsePrintSettings(0, params.get(), &settings);
  EXPECT_NE(nullptr, response);
}

TEST(PageRangeTextToPagesTest, General) {
  using PM = HeadlessPrintManager;
  std::vector<int> pages;
  std::vector<int> expected_pages;

  // "-" is full range of pages.
  PM::PageRangeStatus status = PM::PageRangeTextToPages("-", false, 10, &pages);
  expected_pages = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // If no start page is specified, we start at first page.
  status = PM::PageRangeTextToPages("-5", false, 10, &pages);
  expected_pages = {0, 1, 2, 3, 4};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // If no end page is specified, we end at last page.
  status = PM::PageRangeTextToPages("5-", false, 10, &pages);
  expected_pages = {4, 5, 6, 7, 8, 9};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // Multiple ranges are separated by commas.
  status = PM::PageRangeTextToPages("1-3,9-10,4-6", false, 10, &pages);
  expected_pages = {0, 1, 2, 3, 4, 5, 8, 9};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // White space is ignored.
  status = PM::PageRangeTextToPages("1- 3, 9-10,4 -6", false, 10, &pages);
  expected_pages = {0, 1, 2, 3, 4, 5, 8, 9};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // End page beyond number of pages is supported and capped to number of pages.
  status = PM::PageRangeTextToPages("1-10", false, 5, &pages);
  expected_pages = {0, 1, 2, 3, 4};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // Start page beyond number of pages results in error.
  status = PM::PageRangeTextToPages("1-3,9-10,4-6", false, 5, &pages);
  EXPECT_EQ(PM::LIMIT_ERROR, status);

  // Invalid page ranges are ignored if |ignore_invalid_page_ranges| is true.
  status = PM::PageRangeTextToPages("9-10,4-6,3-1", true, 5, &pages);
  expected_pages = {3, 4};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // Invalid input results in error.
  status = PM::PageRangeTextToPages("abcd", false, 10, &pages);
  EXPECT_EQ(PM::SYNTAX_ERROR, status);

  // Invalid input results in error.
  status = PM::PageRangeTextToPages("1-3,9-a10,4-6", false, 10, &pages);
  EXPECT_EQ(PM::SYNTAX_ERROR, status);
}

}  // namespace headless
