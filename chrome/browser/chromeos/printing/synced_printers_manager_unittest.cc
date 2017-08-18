// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/synced_printers_manager.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/printing/printers_sync_bridge.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/model/fake_model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kTestPrinterId[] = "UUID-UUID-UUID-PRINTER";
const char kTestPrinterId2[] = "UUID-UUID-UUID-PRINTR2";
const char kTestUri[] = "ipps://printer.chromium.org/ipp/print";

const char kLexJson[] = R"({
        "display_name": "LexaPrint",
        "description": "Laser on the test shelf",
        "manufacturer": "LexaPrint, Inc.",
        "model": "MS610de",
        "uri": "ipp://192.168.1.5",
        "ppd_resource": {
          "effective_manufacturer": "LexaPrint",
          "effective_model": "MS610de",
        },
      } )";

const char kColorLaserJson[] = R"json({
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

// Helper class to record observed events.
class LoggingObserver : public SyncedPrintersManager::Observer {
 public:
  void OnConfiguredPrintersChanged(
      const std::vector<Printer>& printers) override {
    configured_printers_ = printers;
  }

  void OnEnterprisePrintersChanged(
      const std::vector<Printer>& printer) override {
    enterprise_printers_ = printer;
  }

  const std::vector<Printer>& configured_printers() const {
    return configured_printers_;
  }
  const std::vector<Printer>& enterprise_printers() const {
    return enterprise_printers_;
  }

 private:
  std::vector<Printer> configured_printers_;
  std::vector<Printer> enterprise_printers_;
};

class SyncedPrintersManagerTest : public testing::Test {
 protected:
  SyncedPrintersManagerTest()
      : manager_(SyncedPrintersManager::Create(
            &profile_,
            base::MakeUnique<PrintersSyncBridge>(
                base::Bind(&syncer::ModelTypeStore::CreateInMemoryStoreForTest,
                           syncer::PRINTERS),
                base::BindRepeating(
                    base::IgnoreResult(&base::debug::DumpWithoutCrashing))))) {
    base::RunLoop().RunUntilIdle();
  }

  // Must outlive |profile_|.
  content::TestBrowserThreadBundle thread_bundle_;

  // Must outlive |manager_|.
  TestingProfile profile_;

  std::unique_ptr<SyncedPrintersManager> manager_;
};

// Add a test failure if the ids of printers are not those in expected.  Order
// is not considered.
void ExpectPrinterIdsAre(const std::vector<Printer>& printers,
                         std::vector<std::string> expected_ids) {
  std::sort(expected_ids.begin(), expected_ids.end());
  std::vector<std::string> printer_ids;
  for (const Printer& printer : printers) {
    printer_ids.push_back(printer.id());
  }
  std::sort(printer_ids.begin(), printer_ids.end());
  if (printer_ids != expected_ids) {
    ADD_FAILURE() << "Expected to find ids: {"
                  << base::JoinString(expected_ids, ",") << "}; found ids: {"
                  << base::JoinString(printer_ids, ",") << "}";
  }
}

TEST_F(SyncedPrintersManagerTest, AddPrinter) {
  LoggingObserver observer;
  manager_->AddObserver(&observer);
  manager_->UpdateConfiguredPrinter(Printer(kTestPrinterId));

  auto printers = manager_->GetConfiguredPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kTestPrinterId, printers[0].id());
  EXPECT_EQ(Printer::Source::SRC_USER_PREFS, printers[0].source());

  ExpectPrinterIdsAre(observer.configured_printers(), {kTestPrinterId});
}

TEST_F(SyncedPrintersManagerTest, UpdatePrinterAssignsId) {
  manager_->UpdateConfiguredPrinter(Printer());

  auto printers = manager_->GetConfiguredPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_FALSE(printers[0].id().empty());
}

TEST_F(SyncedPrintersManagerTest, UpdatePrinter) {
  manager_->UpdateConfiguredPrinter(Printer(kTestPrinterId));
  Printer updated_printer(kTestPrinterId);
  updated_printer.set_uri(kTestUri);

  // Register observer so it only receives the update event.
  LoggingObserver observer;
  manager_->AddObserver(&observer);

  manager_->UpdateConfiguredPrinter(updated_printer);

  auto printers = manager_->GetConfiguredPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kTestUri, printers[0].uri());

  ExpectPrinterIdsAre(observer.configured_printers(), {kTestPrinterId});
}

TEST_F(SyncedPrintersManagerTest, RemovePrinter) {
  manager_->UpdateConfiguredPrinter(Printer("OtherUUID"));
  manager_->UpdateConfiguredPrinter(Printer(kTestPrinterId));
  manager_->UpdateConfiguredPrinter(Printer());

  manager_->RemoveConfiguredPrinter(kTestPrinterId);

  auto printers = manager_->GetConfiguredPrinters();

  // One of the remaining ids should be "OtherUUID", the other should have
  // been automatically generated by the manager.
  ASSERT_EQ(2U, printers.size());
  EXPECT_NE(kTestPrinterId, printers.at(0).id());
  EXPECT_NE(kTestPrinterId, printers.at(1).id());
}

// Tests for policy printers

TEST_F(SyncedPrintersManagerTest, EnterprisePrinters) {
  std::string first_printer = kColorLaserJson;
  std::string second_printer = kLexJson;

  auto value = base::MakeUnique<base::ListValue>();
  value->AppendString(first_printer);
  value->AppendString(second_printer);

  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile_.GetTestingPrefService();
  // TestingPrefSyncableService assumes ownership of |value|.
  prefs->SetManagedPref(prefs::kRecommendedNativePrinters, std::move(value));

  auto printers = manager_->GetEnterprisePrinters();
  ASSERT_EQ(2U, printers.size());
  EXPECT_EQ("Color Laser", printers[0].display_name());
  EXPECT_EQ("ipp://192.168.1.5", printers[1].uri());
  EXPECT_EQ(Printer::Source::SRC_POLICY, printers[1].source());
}

TEST_F(SyncedPrintersManagerTest, GetEnterprisePrinter) {
  std::string printer = kLexJson;
  auto value = base::MakeUnique<base::ListValue>();
  value->AppendString(printer);

  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile_.GetTestingPrefService();
  // TestingPrefSyncableService assumes ownership of |value|.
  prefs->SetManagedPref(prefs::kRecommendedNativePrinters, std::move(value));

  auto printers = manager_->GetEnterprisePrinters();

  const Printer& from_list = printers.front();
  std::unique_ptr<Printer> retrieved = manager_->GetPrinter(from_list.id());

  EXPECT_EQ(from_list.id(), retrieved->id());
  EXPECT_EQ("LexaPrint", from_list.display_name());
  EXPECT_EQ(Printer::Source::SRC_POLICY, from_list.source());
}

TEST_F(SyncedPrintersManagerTest, PrinterNotInstalled) {
  Printer printer(kTestPrinterId, base::Time::FromInternalValue(1000));
  EXPECT_FALSE(manager_->IsConfigurationCurrent(printer));
}

TEST_F(SyncedPrintersManagerTest, PrinterIsInstalled) {
  Printer printer(kTestPrinterId, base::Time::FromInternalValue(1000));
  manager_->PrinterInstalled(printer);
  EXPECT_TRUE(manager_->IsConfigurationCurrent(printer));
}

// Test that PrinterInstalled configures a printer if it doesn't appear in the
// enterprise or configured printer lists.
TEST_F(SyncedPrintersManagerTest, PrinterInstalledConfiguresPrinter) {
  // Set up an enterprise printer.
  auto value = base::MakeUnique<base::ListValue>();
  value->AppendString(kColorLaserJson);

  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile_.GetTestingPrefService();
  prefs->SetManagedPref(prefs::kRecommendedNativePrinters, std::move(value));

  // Figure out the id of the enterprise printer that was just installed.
  std::string enterprise_id = manager_->GetEnterprisePrinters().at(0).id();

  Printer configured(kTestPrinterId, base::Time::Now());

  // Install a Configured printer
  manager_->UpdateConfiguredPrinter(configured);

  // Installing the configured printer should *not* update it.
  configured.set_display_name("display name");
  manager_->PrinterInstalled(configured);
  EXPECT_TRUE(manager_->GetPrinter(kTestPrinterId)->display_name().empty());

  // Installing the enterprise printer should *not* generate a configuration
  // update.
  manager_->PrinterInstalled(Printer(enterprise_id, base::Time::Now()));
  EXPECT_EQ(1U, manager_->GetConfiguredPrinters().size());

  // Installing a printer we don't know about *should* generate a configuration
  // update.
  manager_->PrinterInstalled(Printer(kTestPrinterId2, base::Time::Now()));
  EXPECT_EQ(2U, manager_->GetConfiguredPrinters().size());
}

TEST_F(SyncedPrintersManagerTest, UpdatedPrinterConfiguration) {
  Printer printer(kTestPrinterId, base::Time::FromInternalValue(1000));
  manager_->PrinterInstalled(printer);

  Printer updated_printer(kTestPrinterId, base::Time::FromInternalValue(2000));
  EXPECT_FALSE(manager_->IsConfigurationCurrent(updated_printer));
}

}  // namespace
}  // namespace chromeos
