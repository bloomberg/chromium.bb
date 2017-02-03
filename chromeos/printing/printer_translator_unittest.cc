// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace printing {

// Printer test data
const char kGUID[] = "GUID-GUID-GUID";
const char kName[] = "Chrome Super Printer";
const char kDescription[] = "first star on the left";
const char kMake[] = "Chrome";
const char kModel[] = "Inktastic Laser Magic";
const char kUri[] = "ipp://printy.domain.co:555/ipp/print";
const char kUUID[] = "UUID-UUID-UUID";

// PpdReference test data
const char kUserSuppliedPpdUrl[] = "/some/path/to/user.url";
const char kEffectiveMakeAndModel[] = "PrintBlaster 2000";

TEST(PrinterTranslatorTest, PrefToPrinterMissingId) {
  base::DictionaryValue value;
  std::unique_ptr<Printer> printer = PrefToPrinter(value);

  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, PrefToPrinter) {
  base::DictionaryValue preference;
  preference.SetString("id", kGUID);
  preference.SetString("display_name", kName);
  preference.SetString("description", kDescription);
  preference.SetString("manufacturer", kMake);
  preference.SetString("model", kModel);
  preference.SetString("uri", kUri);
  preference.SetString("uuid", kUUID);

  std::unique_ptr<Printer> printer = PrefToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kGUID, printer->id());
  EXPECT_EQ(kName, printer->display_name());
  EXPECT_EQ(kDescription, printer->description());
  EXPECT_EQ(kMake, printer->manufacturer());
  EXPECT_EQ(kModel, printer->model());
  EXPECT_EQ(kUri, printer->uri());
  EXPECT_EQ(kUUID, printer->uuid());
}

TEST(PrinterTranslatorTest, PrinterToPref) {
  Printer printer("GLOBALLY_UNIQUE_ID");
  printer.set_display_name(kName);
  printer.set_description(kDescription);
  printer.set_manufacturer(kMake);
  printer.set_model(kModel);
  printer.set_uri(kUri);
  printer.set_uuid(kUUID);

  std::unique_ptr<base::DictionaryValue> pref = PrinterToPref(printer);

  base::ExpectDictStringValue("GLOBALLY_UNIQUE_ID", *pref, "id");
  base::ExpectDictStringValue(kName, *pref, "display_name");
  base::ExpectDictStringValue(kDescription, *pref, "description");
  base::ExpectDictStringValue(kMake, *pref, "manufacturer");
  base::ExpectDictStringValue(kModel, *pref, "model");
  base::ExpectDictStringValue(kUri, *pref, "uri");
  base::ExpectDictStringValue(kUUID, *pref, "uuid");
}

TEST(PrinterTranslatorTest, PrinterToPrefPpdReference) {
  Printer printer("UNIQUE_ID");
  auto* ppd = printer.mutable_ppd_reference();
  ppd->user_supplied_ppd_url = kUserSuppliedPpdUrl;
  ppd->effective_make_and_model = kEffectiveMakeAndModel;

  std::unique_ptr<base::DictionaryValue> actual = PrinterToPref(printer);

  base::ExpectDictStringValue(kUserSuppliedPpdUrl, *actual,
                              "ppd_reference.user_supplied_ppd_url");
  base::ExpectDictStringValue(kEffectiveMakeAndModel, *actual,
                              "ppd_reference.effective_model");
}

// Make sure we don't serialize empty fields.
TEST(PrinterTranslatorTest, PrinterToPrefPpdReferenceLazy) {
  Printer printer("UNIQUE_ID");
  std::unique_ptr<base::DictionaryValue> actual = PrinterToPref(printer);

  EXPECT_FALSE(actual->HasKey("ppd_reference.user_supplied_ppd_url"));
  EXPECT_FALSE(actual->HasKey("ppd_reference.ppd_server_key"));
}

TEST(PrinterTranslatorTest, PrefToPrinterRoundTrip) {
  base::DictionaryValue preference;
  preference.SetString("id", kGUID);
  preference.SetString("display_name", kName);
  preference.SetString("description", kDescription);
  preference.SetString("manufacturer", kMake);
  preference.SetString("model", kModel);
  preference.SetString("uri", kUri);
  preference.SetString("uuid", kUUID);

  preference.SetString("ppd_reference.user_supplied_ppd_url",
                       kUserSuppliedPpdUrl);
  preference.SetString("ppd_reference.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = PrefToPrinter(preference);
  std::unique_ptr<base::DictionaryValue> pref_copy = PrinterToPref(*printer);

  EXPECT_TRUE(preference.Equals(pref_copy.get()));
}

}  // namespace printing
}  // namespace chromeos
