// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace printing {

// Printer test data
const char kHash[] = "ABCDEF123456";
const char kName[] = "Chrome Super Printer";
const char kDescription[] = "first star on the left";
const char kMake[] = "Chrome";
const char kModel[] = "Inktastic Laser Magic";
const char kUri[] = "ipp://printy.domain.co:555/ipp/print";
const char kUUID[] = "UUID-UUID-UUID";

const base::Time kTimestamp = base::Time::FromInternalValue(445566);

// PpdReference test data
const char kEffectiveMakeAndModel[] = "PrintBlaster LazerInker 2000";

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinterMissingId) {
  base::DictionaryValue value;
  std::unique_ptr<Printer> printer =
      RecommendedPrinterToPrinter(value, kTimestamp);

  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingDisplayNameFails) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  // display name omitted
  preference.SetString("uri", kUri);
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer =
      RecommendedPrinterToPrinter(preference, kTimestamp);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingUriFails) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  // uri omitted
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer =
      RecommendedPrinterToPrinter(preference, kTimestamp);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingPpdResourceFails) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("uri", kUri);
  // ppd resource omitted

  std::unique_ptr<Printer> printer =
      RecommendedPrinterToPrinter(preference, kTimestamp);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingEffectiveMakeModelFails) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("uri", kUri);
  preference.SetString("ppd_resource.foobarwrongfield", "gibberish");

  std::unique_ptr<Printer> printer =
      RecommendedPrinterToPrinter(preference, kTimestamp);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, RecommendedPrinterMinimalSetup) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("uri", kUri);
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer =
      RecommendedPrinterToPrinter(preference, kTimestamp);
  EXPECT_TRUE(printer);
}

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinter) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("description", kDescription);
  preference.SetString("manufacturer", kMake);
  preference.SetString("model", kModel);
  preference.SetString("uri", kUri);
  preference.SetString("uuid", kUUID);

  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer =
      RecommendedPrinterToPrinter(preference, kTimestamp);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kHash, printer->id());
  EXPECT_EQ(kName, printer->display_name());
  EXPECT_EQ(kDescription, printer->description());
  EXPECT_EQ(kMake, printer->manufacturer());
  EXPECT_EQ(kModel, printer->model());
  EXPECT_EQ(kUri, printer->uri());
  EXPECT_EQ(kUUID, printer->uuid());
  EXPECT_EQ(kTimestamp, printer->last_updated());

  EXPECT_EQ(kEffectiveMakeAndModel,
            printer->ppd_reference().effective_make_and_model);
}

}  // namespace printing
}  // namespace chromeos
