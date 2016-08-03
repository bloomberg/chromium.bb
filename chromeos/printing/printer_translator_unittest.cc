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

// PPDFile test data
const int kPPDiD = 13334;
const char kFileName[] = "path/to/ppd/file.ppd";
const int kVersion = 47773;
const bool kQuirks = false;

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

TEST(PrinterTranslatorTest, PrinterToPrefPPDFile) {
  Printer::PPDFile ppd;
  ppd.id = kPPDiD;
  ppd.file_name = kFileName;
  ppd.version_number = kVersion;
  ppd.from_quirks_server = kQuirks;

  Printer printer("UNIQUE_ID");
  printer.SetPPD(base::MakeUnique<Printer::PPDFile>(std::move(ppd)));

  std::unique_ptr<base::DictionaryValue> actual = PrinterToPref(printer);

  base::ExpectDictIntegerValue(kPPDiD, *actual, "ppd.id");
  base::ExpectDictStringValue(kFileName, *actual, "ppd.file_name");
  base::ExpectDictIntegerValue(kVersion, *actual, "ppd.version_number");
  base::ExpectDictBooleanValue(kQuirks, *actual, "ppd.from_quirks_server");
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

  preference.SetInteger("ppd.id", kPPDiD);
  preference.SetString("ppd.file_name", kFileName);
  preference.SetInteger("ppd.version_number", kVersion);
  preference.SetBoolean("ppd.from_quirks_server", kQuirks);

  std::unique_ptr<Printer> printer = PrefToPrinter(preference);
  std::unique_ptr<base::DictionaryValue> pref_copy = PrinterToPref(*printer);

  EXPECT_TRUE(preference.Equals(pref_copy.get()));
}

}  // namespace printing
}  // namespace chromeos
