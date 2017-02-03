// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ptr_util.h"
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

  EXPECT_EQ(effective_make_and_model,
            result->ppd_reference().effective_make_and_model);
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
  ppd.effective_make_and_model = effective_make_and_model;
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

  EXPECT_EQ(effective_make_and_model,
            result->ppd_reference().effective_make_and_model);
}

}  // namespace printing
}  // namespace chromeos
