// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printers_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/printing/printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/printers_sync_bridge.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/model/fake_model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
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

// Helper class to record observed events.
class LoggingObserver : public PrintersManager::Observer {
 public:
  void OnPrinterAdded(const Printer& printer) override {
    last_added_ = printer;
  }

  void OnPrinterUpdated(const Printer& printer) override {
    last_updated_ = printer;
  }

  void OnPrinterRemoved(const Printer& printer) override {
    last_removed_ = printer;
  }

  // Returns true if OnPrinterAdded was called.
  bool AddCalled() const { return last_added_.has_value(); }

  // Returns true if OnPrinterUpdated was called.
  bool UpdateCalled() const { return last_updated_.has_value(); }

  // Returns true if OnPrinterRemoved was called.
  bool RemoveCalled() const { return last_removed_.has_value(); }

  // Returns the last printer that was added.  If AddCalled is false, there will
  // be no printer.
  base::Optional<Printer> LastAdded() const { return last_added_; }
  // Returns the last printer that was updated.  If UpdateCalled is false, there
  // will be no printer.
  base::Optional<Printer> LastUpdated() const { return last_updated_; }
  // Returns the last printer that was removed.  If RemoveCalled is false, there
  // will be no printer.
  base::Optional<Printer> LastRemoved() const { return last_removed_; }

 private:
  base::Optional<Printer> last_added_;
  base::Optional<Printer> last_updated_;
  base::Optional<Printer> last_removed_;
};

}  // namespace

class PrintersManagerTest : public testing::Test {
 protected:
  PrintersManagerTest() : profile_(base::MakeUnique<TestingProfile>()) {
    thread_bundle_ = base::MakeUnique<content::TestBrowserThreadBundle>();

    auto sync_bridge = base::MakeUnique<PrintersSyncBridge>(
        base::Bind(&syncer::ModelTypeStore::CreateInMemoryStoreForTest,
                   syncer::PRINTERS),
        base::BindRepeating(
            base::IgnoreResult(&base::debug::DumpWithoutCrashing)));

    manager_ = base::MakeUnique<PrintersManager>(profile_.get(),
                                                 std::move(sync_bridge));

    base::RunLoop().RunUntilIdle();
  }

  ~PrintersManagerTest() override {
    manager_.reset();

    // Explicitly release the profile before the thread_bundle.  Otherwise, the
    // profile destructor throws an error.
    profile_.reset();
    thread_bundle_.reset();
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PrintersManager> manager_;

 private:
  std::unique_ptr<content::TestBrowserThreadBundle> thread_bundle_;
};

TEST_F(PrintersManagerTest, AddPrinter) {
  LoggingObserver observer;
  manager_->AddObserver(&observer);
  manager_->RegisterPrinter(base::MakeUnique<Printer>(kPrinterId));

  auto printers = manager_->GetPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kPrinterId, printers[0]->id());
  EXPECT_EQ(Printer::Source::SRC_USER_PREFS, printers[0]->source());

  EXPECT_TRUE(observer.AddCalled());
  EXPECT_FALSE(observer.UpdateCalled());
}

TEST_F(PrintersManagerTest, UpdatePrinterAssignsId) {
  manager_->RegisterPrinter(base::MakeUnique<Printer>());

  auto printers = manager_->GetPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_FALSE(printers[0]->id().empty());
}

TEST_F(PrintersManagerTest, UpdatePrinter) {
  manager_->RegisterPrinter(base::MakeUnique<Printer>(kPrinterId));
  auto updated_printer = base::MakeUnique<Printer>(kPrinterId);
  updated_printer->set_uri(kUri);

  // Register observer so it only receives the update event.
  LoggingObserver observer;
  manager_->AddObserver(&observer);

  manager_->RegisterPrinter(std::move(updated_printer));

  auto printers = manager_->GetPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kUri, printers[0]->uri());

  EXPECT_TRUE(observer.UpdateCalled());
  EXPECT_FALSE(observer.AddCalled());
}

TEST_F(PrintersManagerTest, RemovePrinter) {
  manager_->RegisterPrinter(base::MakeUnique<Printer>("OtherUUID"));
  manager_->RegisterPrinter(base::MakeUnique<Printer>(kPrinterId));
  manager_->RegisterPrinter(base::MakeUnique<Printer>());

  manager_->RemovePrinter(kPrinterId);

  auto printers = manager_->GetPrinters();
  ASSERT_EQ(2U, printers.size());
  EXPECT_NE(kPrinterId, printers.at(0)->id());
  EXPECT_NE(kPrinterId, printers.at(1)->id());
}

// Tests for policy printers

TEST_F(PrintersManagerTest, RecommendedPrinters) {
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

  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile_->GetTestingPrefService();
  // TestingPrefSyncableService assumes ownership of |value|.
  prefs->SetManagedPref(prefs::kRecommendedNativePrinters, value.release());

  auto printers = manager_->GetRecommendedPrinters();
  ASSERT_EQ(2U, printers.size());
  EXPECT_EQ("Color Laser", printers[0]->display_name());
  EXPECT_EQ("ipp://192.168.1.5", printers[1]->uri());
  EXPECT_EQ(Printer::Source::SRC_POLICY, printers[1]->source());
}

TEST_F(PrintersManagerTest, GetRecommendedPrinter) {
  std::string printer = kLexJson;
  auto value = base::MakeUnique<base::ListValue>();
  value->AppendString(printer);

  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile_->GetTestingPrefService();
  // TestingPrefSyncableService assumes ownership of |value|.
  prefs->SetManagedPref(prefs::kRecommendedNativePrinters, value.release());

  auto printers = manager_->GetRecommendedPrinters();

  const Printer& from_list = *(printers.front());
  std::unique_ptr<Printer> retrieved = manager_->GetPrinter(from_list.id());

  EXPECT_EQ(from_list.id(), retrieved->id());
  EXPECT_EQ("LexaPrint", from_list.display_name());
  EXPECT_EQ(Printer::Source::SRC_POLICY, from_list.source());
}

}  // namespace chromeos
