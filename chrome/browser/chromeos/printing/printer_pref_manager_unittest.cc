// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_pref_manager.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/printing/printer_pref_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kPrinterId[] = "UUID-UUID-UUID-PRINTER";
const char kUri[] = "ipps://printer.chromium.org/ipp/print";

const char kLexJson[] = R"json({
        "display_name": "LexaPrint",
        "description": "Laser on the test shelf",
        "manufacturer": "LexaPrint, Inc.",
        "model": "MS610de",
        "uri": "ipp://192.168.1.5",
        "ppd_resource": {
          "effective_manufacturer": "LexaPrint",
          "effective_model": "MS610de",
        },
      } )json";

}  // namespace

TEST(PrinterPrefManagerTest, AddPrinter) {
  content::TestBrowserThreadBundle thread_bundle;
  auto profile = base::MakeUnique<TestingProfile>();
  PrinterPrefManager* manager =
      PrinterPrefManagerFactory::GetForBrowserContext(profile.get());

  manager->RegisterPrinter(base::MakeUnique<Printer>(kPrinterId));

  auto printers = manager->GetPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kPrinterId, printers[0]->id());
  EXPECT_EQ(Printer::Source::SRC_USER_PREFS, printers[0]->source());
}

TEST(PrinterPrefManagerTest, UpdatePrinterAssignsId) {
  content::TestBrowserThreadBundle thread_bundle;
  auto profile = base::MakeUnique<TestingProfile>();
  PrinterPrefManager* manager =
      PrinterPrefManagerFactory::GetForBrowserContext(profile.get());

  manager->RegisterPrinter(base::MakeUnique<Printer>());

  auto printers = manager->GetPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_FALSE(printers[0]->id().empty());
}

TEST(PrinterPrefManagerTest, UpdatePrinter) {
  content::TestBrowserThreadBundle thread_bundle;
  auto profile = base::MakeUnique<TestingProfile>();
  PrinterPrefManager* manager =
      PrinterPrefManagerFactory::GetForBrowserContext(profile.get());

  manager->RegisterPrinter(base::MakeUnique<Printer>(kPrinterId));
  auto updated_printer = base::MakeUnique<Printer>(kPrinterId);
  updated_printer->set_uri(kUri);
  manager->RegisterPrinter(std::move(updated_printer));

  auto printers = manager->GetPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kUri, printers[0]->uri());
}

TEST(PrinterPrefManagerTest, RemovePrinter) {
  content::TestBrowserThreadBundle thread_bundle;
  auto profile = base::MakeUnique<TestingProfile>();
  PrinterPrefManager* manager =
      PrinterPrefManagerFactory::GetForBrowserContext(profile.get());

  manager->RegisterPrinter(base::MakeUnique<Printer>("OtherUUID"));
  manager->RegisterPrinter(base::MakeUnique<Printer>(kPrinterId));
  manager->RegisterPrinter(base::MakeUnique<Printer>());

  manager->RemovePrinter(kPrinterId);

  auto printers = manager->GetPrinters();
  ASSERT_EQ(2U, printers.size());
  EXPECT_NE(kPrinterId, printers.at(0)->id());
  EXPECT_NE(kPrinterId, printers.at(1)->id());
}

TEST(PrinterPrefManagerTest, RecommendedPrinters) {
  content::TestBrowserThreadBundle thread_bundle;

  std::string first_printer =
      R"json({
      "display_name": "Color Laser",
      "description": "The printer next to the water cooler.",
      "manufacturer": "Printer Manufacturer",
      "model":"Color Laser 2004",
      "uri":"ipps://print-server.intranet.example.com:443/ipp/cl2k4",
      "uuid":"1c395fdb-5d93-4904-b246-b2c046e79d12",
      "ppd_resource":{
          "effective_manufacturer": "MakesPrinters",
          "effective_model":"ColorLaser2k4"
       }
      })json";

  std::string second_printer = kLexJson;

  auto value = base::MakeUnique<base::ListValue>();
  value->AppendString(first_printer);
  value->AppendString(second_printer);

  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();
  // TestingPrefSyncableService assumes ownership of |value|.
  prefs->SetManagedPref(prefs::kRecommendedNativePrinters, value.release());

  PrinterPrefManager* manager =
      PrinterPrefManagerFactory::GetForBrowserContext(&profile);

  auto printers = manager->GetRecommendedPrinters();
  ASSERT_EQ(2U, printers.size());
  EXPECT_EQ("Color Laser", printers[0]->display_name());
  EXPECT_EQ("ipp://192.168.1.5", printers[1]->uri());
  EXPECT_EQ(Printer::Source::SRC_POLICY, printers[1]->source());
}

TEST(PrinterPrefManagerTest, GetRecommendedPrinter) {
  content::TestBrowserThreadBundle thread_bundle;

  std::string printer = kLexJson;
  auto value = base::MakeUnique<base::ListValue>();
  value->AppendString(printer);

  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();
  // TestingPrefSyncableService assumes ownership of |value|.
  prefs->SetManagedPref(prefs::kRecommendedNativePrinters, value.release());

  PrinterPrefManager* manager =
      PrinterPrefManagerFactory::GetForBrowserContext(&profile);

  auto printers = manager->GetRecommendedPrinters();

  const Printer& from_list = *(printers.front());
  std::unique_ptr<Printer> retrieved = manager->GetPrinter(from_list.id());

  EXPECT_EQ(from_list.id(), retrieved->id());
  EXPECT_EQ("LexaPrint", from_list.display_name());
  EXPECT_EQ(Printer::Source::SRC_POLICY, from_list.source());
}

}  // namespace chromeos
