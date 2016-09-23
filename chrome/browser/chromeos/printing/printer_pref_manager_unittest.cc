// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_pref_manager.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/printing/printer_pref_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kPrinterId[] = "UUID-UUID-UUID-PRINTER";
const char kUri[] = "ipps://printer.chromium.org/ipp/print";

}  // namespace

TEST(PrinterPrefManagerTest, AddPrinter) {
  content::TestBrowserThreadBundle thread_bundle;
  std::unique_ptr<Profile> profile = base::MakeUnique<TestingProfile>();
  PrinterPrefManager* manager =
      PrinterPrefManagerFactory::GetForBrowserContext(profile.get());

  manager->RegisterPrinter(base::MakeUnique<Printer>(kPrinterId));

  auto printers = manager->GetPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kPrinterId, printers[0]->id());
}

TEST(PrinterPrefManagerTest, UpdatePrinterAssignsId) {
  content::TestBrowserThreadBundle thread_bundle;
  std::unique_ptr<Profile> profile = base::MakeUnique<TestingProfile>();
  PrinterPrefManager* manager =
      PrinterPrefManagerFactory::GetForBrowserContext(profile.get());

  manager->RegisterPrinter(base::MakeUnique<Printer>());

  auto printers = manager->GetPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_FALSE(printers[0]->id().empty());
}

TEST(PrinterPrefManagerTest, UpdatePrinter) {
  content::TestBrowserThreadBundle thread_bundle;
  std::unique_ptr<Profile> profile = base::MakeUnique<TestingProfile>();
  PrinterPrefManager* manager =
      PrinterPrefManagerFactory::GetForBrowserContext(profile.get());

  manager->RegisterPrinter(base::MakeUnique<Printer>(kPrinterId));
  std::unique_ptr<Printer> updated_printer =
      base::MakeUnique<Printer>(kPrinterId);
  updated_printer->set_uri(kUri);
  manager->RegisterPrinter(std::move(updated_printer));

  auto printers = manager->GetPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kUri, printers[0]->uri());
}

TEST(PrinterPrefManagerTest, RemovePrinter) {
  content::TestBrowserThreadBundle thread_bundle;
  std::unique_ptr<Profile> profile = base::MakeUnique<TestingProfile>();
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

}  // namespace chromeos
