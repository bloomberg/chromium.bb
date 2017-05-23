// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/printing/specifics_translation.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/sync/protocol/printer_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char id[] = "UNIQUE_ID";
const char display_name[] = "Best Printer Ever";
const char description[] = "The green one";
const char manufacturer[] = "Manufacturer";
const char model[] = "MODEL";
const char uri[] = "ipps://notaprinter.chromium.org/ipp/print";
const char uuid[] = "UUIDUUIDUUID";
const base::Time kUpdateTime = base::Time::FromInternalValue(22114455660000);

const char kUserSuppliedPPD[] = "file://foo/bar/baz/eeaaaffccdd00";
const char effective_make_and_model[] = "Manufacturer Model T1000";

}  // namespace

namespace chromeos {
namespace printing {

TEST(SpecificsTranslationTest, SpecificsToPrinter) {
  sync_pb::PrinterSpecifics specifics;
  specifics.set_id(id);
  specifics.set_display_name(display_name);
  specifics.set_description(description);
  specifics.set_manufacturer(manufacturer);
  specifics.set_model(model);
  specifics.set_uri(uri);
  specifics.set_uuid(uuid);
  specifics.set_updated_timestamp(kUpdateTime.ToJavaTime());

  sync_pb::PrinterPPDReference ppd;
  ppd.set_effective_make_and_model(effective_make_and_model);
  *specifics.mutable_ppd_reference() = ppd;

  std::unique_ptr<Printer> result = SpecificsToPrinter(specifics);
  EXPECT_EQ(id, result->id());
  EXPECT_EQ(display_name, result->display_name());
  EXPECT_EQ(description, result->description());
  EXPECT_EQ(manufacturer, result->manufacturer());
  EXPECT_EQ(model, result->model());
  EXPECT_EQ(uri, result->uri());
  EXPECT_EQ(uuid, result->uuid());
  EXPECT_EQ(kUpdateTime, result->last_updated());

  EXPECT_EQ(effective_make_and_model,
            result->ppd_reference().effective_make_and_model);
  EXPECT_FALSE(result->IsIppEverywhere());
}

TEST(SpecificsTranslationTest, PrinterToSpecifics) {
  Printer printer;
  printer.set_id(id);
  printer.set_display_name(display_name);
  printer.set_description(description);
  printer.set_manufacturer(manufacturer);
  printer.set_model(model);
  printer.set_uri(uri);
  printer.set_uuid(uuid);

  Printer::PpdReference ppd;
  ppd.effective_make_and_model = effective_make_and_model;
  *printer.mutable_ppd_reference() = ppd;

  std::unique_ptr<sync_pb::PrinterSpecifics> result =
      PrinterToSpecifics(printer);
  EXPECT_EQ(id, result->id());
  EXPECT_EQ(display_name, result->display_name());
  EXPECT_EQ(description, result->description());
  EXPECT_EQ(manufacturer, result->manufacturer());
  EXPECT_EQ(model, result->model());
  EXPECT_EQ(uri, result->uri());
  EXPECT_EQ(uuid, result->uuid());

  EXPECT_EQ(effective_make_and_model,
            result->ppd_reference().effective_make_and_model());
}

TEST(SpecificsTranslationTest, SpecificsToPrinterRoundTrip) {
  Printer printer;
  printer.set_id(id);
  printer.set_display_name(display_name);
  printer.set_description(description);
  printer.set_manufacturer(manufacturer);
  printer.set_model(model);
  printer.set_uri(uri);
  printer.set_uuid(uuid);

  Printer::PpdReference ppd;
  ppd.autoconf = true;
  *printer.mutable_ppd_reference() = ppd;

  std::unique_ptr<sync_pb::PrinterSpecifics> temp = PrinterToSpecifics(printer);
  std::unique_ptr<Printer> result = SpecificsToPrinter(*temp);

  EXPECT_EQ(id, result->id());
  EXPECT_EQ(display_name, result->display_name());
  EXPECT_EQ(description, result->description());
  EXPECT_EQ(manufacturer, result->manufacturer());
  EXPECT_EQ(model, result->model());
  EXPECT_EQ(uri, result->uri());
  EXPECT_EQ(uuid, result->uuid());

  EXPECT_TRUE(result->ppd_reference().effective_make_and_model.empty());
  EXPECT_TRUE(result->ppd_reference().autoconf);
}

TEST(SpecificsTranslationTest, MergePrinterToSpecifics) {
  sync_pb::PrinterSpecifics original;
  original.set_id(id);
  original.mutable_ppd_reference()->set_autoconf(true);

  Printer printer(id);
  printer.mutable_ppd_reference()->effective_make_and_model =
      effective_make_and_model;

  MergePrinterToSpecifics(printer, &original);

  EXPECT_EQ(id, original.id());
  EXPECT_EQ(effective_make_and_model,
            original.ppd_reference().effective_make_and_model());

  // Verify that autoconf is cleared.
  EXPECT_FALSE(original.ppd_reference().autoconf());
}

// Tests that the autoconf value overrides other PpdReference fields.
TEST(SpecificsTranslationTest, AutoconfOverrides) {
  sync_pb::PrinterSpecifics original;
  original.set_id(id);
  auto* ppd_reference = original.mutable_ppd_reference();
  ppd_reference->set_autoconf(true);
  ppd_reference->set_user_supplied_ppd_url(kUserSuppliedPPD);

  auto printer = SpecificsToPrinter(original);

  EXPECT_TRUE(printer->ppd_reference().autoconf);
  EXPECT_TRUE(printer->ppd_reference().user_supplied_ppd_url.empty());
  EXPECT_TRUE(printer->ppd_reference().effective_make_and_model.empty());
}

// Tests that user_supplied_ppd_url overwrites other PpdReference fields if
// autoconf is false.
TEST(SpecificsTranslationTest, UserSuppliedOverrides) {
  sync_pb::PrinterSpecifics original;
  original.set_id(id);
  auto* ppd_reference = original.mutable_ppd_reference();
  ppd_reference->set_user_supplied_ppd_url(kUserSuppliedPPD);
  ppd_reference->set_effective_make_and_model(effective_make_and_model);

  auto printer = SpecificsToPrinter(original);

  EXPECT_FALSE(printer->ppd_reference().autoconf);
  EXPECT_FALSE(printer->ppd_reference().user_supplied_ppd_url.empty());
  EXPECT_TRUE(printer->ppd_reference().effective_make_and_model.empty());
}

}  // namespace printing
}  // namespace chromeos
