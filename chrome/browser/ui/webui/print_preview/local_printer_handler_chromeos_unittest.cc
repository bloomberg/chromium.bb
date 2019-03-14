// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.cc"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/print_job_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

using chromeos::CupsPrintersManager;
using chromeos::Printer;
using chromeos::PrinterConfigurer;
using chromeos::PrinterSetupCallback;
using chromeos::PrinterSetupResult;

// Used as a callback to StartGetPrinters in tests.
// Increases |*call_count| and records values returned by StartGetPrinters.
void RecordPrinterList(size_t* call_count,
                       std::unique_ptr<base::ListValue>* printers_out,
                       const base::ListValue& printers) {
  ++(*call_count);
  printers_out->reset(printers.DeepCopy());
}

// Used as a callback to StartGetPrinters in tests.
// Records that the test is done.
void RecordPrintersDone(bool* is_done_out) {
  *is_done_out = true;
}

void RecordGetCapability(std::unique_ptr<base::Value>* capabilities_out,
                         base::Value capability) {
  capabilities_out->reset(capability.DeepCopy());
}

Printer CreateTestPrinter(const std::string& id, const std::string& name) {
  Printer printer;
  printer.set_id(id);
  printer.set_display_name(name);
  return printer;
}

Printer CreateEnterprisePrinter(const std::string& id,
                                const std::string& name) {
  Printer printer = CreateTestPrinter(id, name);
  printer.set_source(Printer::SRC_POLICY);
  return printer;
}

class FakeCupsPrintersManager : public CupsPrintersManager {
 public:
  FakeCupsPrintersManager() : printers_(kNumPrinterClasses) {}

  std::vector<Printer> GetPrinters(PrinterClass printer_class) const override {
    return printers_[printer_class];
  }

  void RemoveUnavailablePrinters(std::vector<Printer>*) const override {}
  void UpdateConfiguredPrinter(const Printer& printer) override {}
  void RemoveConfiguredPrinter(const std::string& printer_id) override {}
  void AddObserver(CupsPrintersManager::Observer* observer) override {}
  void RemoveObserver(CupsPrintersManager::Observer* observer) override {}
  void PrinterInstalled(const Printer& printer, bool is_automatic) override {}
  void RecordSetupAbandoned(const Printer& printer) override {}

  bool IsPrinterInstalled(const Printer& printer) const override {
    return installed_.contains(printer.id());
  }

  std::unique_ptr<Printer> GetPrinter(const std::string& id) const override {
    for (const std::vector<Printer>& v : printers_) {
      auto iter = std::find_if(
          v.begin(), v.end(), [&id](const Printer& p) { return p.id() == id; });
      if (iter != v.end()) {
        return std::make_unique<Printer>(*iter);
      }
    }
    return nullptr;
  }

  // Add |printer| to the corresponding list in |printers_| bases on the given
  // |printer_class|.
  void AddPrinter(const Printer& printer, PrinterClass printer_class) {
    ASSERT_LT(printer_class, printers_.size());
    printers_[printer_class].push_back(printer);
  }

  void InstallPrinter(const std::string& id) { installed_.insert(id); }

 private:
  std::vector<std::vector<Printer>> printers_;
  base::flat_set<std::string> installed_;
};

class FakePrinterConfigurer : public PrinterConfigurer {
 public:
  void SetUpPrinter(const Printer& printer,
                    PrinterSetupCallback callback) override {
    std::move(callback).Run(PrinterSetupResult::kSuccess);
  }
};

// Converts JSON string to base::ListValue object.
// On failure, returns NULL and fills |*error| string.
std::unique_ptr<base::ListValue> GetJSONAsListValue(const std::string& json,
                                                    std::string* error) {
  auto ret = base::ListValue::From(
      JSONStringValueDeserializer(json).Deserialize(nullptr, error));
  if (!ret)
    *error = "Value is not a list.";
  return ret;
}

}  // namespace

class LocalPrinterHandlerChromeosTest : public testing::Test {
 public:
  LocalPrinterHandlerChromeosTest() = default;
  ~LocalPrinterHandlerChromeosTest() override = default;

  void SetUp() override {
    test_backend_ = base::MakeRefCounted<TestPrintBackend>();
    PrintBackend::SetPrintBackendForTesting(test_backend_.get());
    local_printer_handler_ = LocalPrinterHandlerChromeos::CreateForTesting(
        &profile_, nullptr, &printers_manager_,
        std::make_unique<FakePrinterConfigurer>());
  }

 protected:
  // Must outlive |profile_|.
  content::TestBrowserThreadBundle thread_bundle_;
  // Must outlive |printers_manager_|.
  TestingProfile profile_;
  scoped_refptr<TestPrintBackend> test_backend_;
  FakeCupsPrintersManager printers_manager_;
  std::unique_ptr<LocalPrinterHandlerChromeos> local_printer_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalPrinterHandlerChromeosTest);
};

TEST_F(LocalPrinterHandlerChromeosTest, GetPrinters) {
  size_t call_count = 0;
  std::unique_ptr<base::ListValue> printers;
  bool is_done = false;

  Printer configured_printer = CreateTestPrinter("printer1", "configured");
  Printer enterprise_printer =
      CreateEnterprisePrinter("printer2", "enterprise");
  Printer automatic_printer = CreateTestPrinter("printer3", "automatic");

  printers_manager_.AddPrinter(configured_printer,
                               CupsPrintersManager::kConfigured);
  printers_manager_.AddPrinter(enterprise_printer,
                               CupsPrintersManager::kEnterprise);
  printers_manager_.AddPrinter(automatic_printer,
                               CupsPrintersManager::kAutomatic);

  local_printer_handler_->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, &call_count, &printers),
      base::BindOnce(&RecordPrintersDone, &is_done));

  EXPECT_EQ(call_count, 1u);
  EXPECT_TRUE(is_done);
  ASSERT_TRUE(printers);

  const std::string expected_list = R"(
    [
      {
        "cupsEnterprisePrinter": false,
        "deviceName": "printer1",
        "printerDescription": "",
        "printerName": "configured",
        "printerOptions": {
          "cupsEnterprisePrinter": "false",
          "system_driverinfo": ""
        }
      },
      {
        "cupsEnterprisePrinter": true,
        "deviceName": "printer2",
        "printerDescription": "",
        "printerName": "enterprise",
        "printerOptions": {
          "cupsEnterprisePrinter": "true",
          "system_driverinfo": ""
        }
      },
      {
        "cupsEnterprisePrinter": false,
        "deviceName": "printer3",
        "printerDescription": "",
        "printerName": "automatic",
        "printerOptions": {
          "cupsEnterprisePrinter": "false",
          "system_driverinfo": ""
        }
      }
    ]
  )";
  std::string error;
  std::unique_ptr<base::ListValue> expected_printers(
      GetJSONAsListValue(expected_list, &error));
  ASSERT_TRUE(expected_printers) << "Error deserializing printers: " << error;
  EXPECT_EQ(*printers, *expected_printers);
}

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_F(LocalPrinterHandlerChromeosTest, StartGetCapabilityValidPrinter) {
  Printer configured_printer = CreateTestPrinter("printer1", "configured");
  printers_manager_.AddPrinter(configured_printer,
                               CupsPrintersManager::kConfigured);
  printers_manager_.InstallPrinter("printer1");

  // Add printer capabilities to |test_backend_|.
  PrinterSemanticCapsAndDefaults caps;
  test_backend_->AddValidPrinter(
      "printer1", std::make_unique<PrinterSemanticCapsAndDefaults>(caps));

  std::unique_ptr<base::Value> fetched_caps;
  local_printer_handler_->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, &fetched_caps));

  thread_bundle_.RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  base::DictionaryValue* dict;
  ASSERT_TRUE(fetched_caps->GetAsDictionary(&dict));
  ASSERT_TRUE(dict->HasKey(kSettingCapabilities));
  ASSERT_TRUE(dict->HasKey(kPrinter));
}

// Test that printers which have not yet been installed are installed with
// SetUpPrinter before their capabilities are fetched.
TEST_F(LocalPrinterHandlerChromeosTest, StartGetCapabilityPrinterNotInstalled) {
  Printer discovered_printer = CreateTestPrinter("printer1", "discovered");
  // NOTE: The printer |discovered_printer| is not installed using
  // InstallPrinter.
  printers_manager_.AddPrinter(discovered_printer,
                               CupsPrintersManager::kDiscovered);

  // Add printer capabilities to |test_backend_|.
  PrinterSemanticCapsAndDefaults caps;
  test_backend_->AddValidPrinter(
      "printer1", std::make_unique<PrinterSemanticCapsAndDefaults>(caps));

  std::unique_ptr<base::Value> fetched_caps;
  local_printer_handler_->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, &fetched_caps));

  thread_bundle_.RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  base::DictionaryValue* dict;
  ASSERT_TRUE(fetched_caps->GetAsDictionary(&dict));
  ASSERT_TRUE(dict->HasKey(kSettingCapabilities));
  ASSERT_TRUE(dict->HasKey(kPrinter));
}

// In this test we expect the StartGetCapability to bail early because the
// provided printer can't be found in the CupsPrintersManager.
TEST_F(LocalPrinterHandlerChromeosTest, StartGetCapabilityInvalidPrinter) {
  std::unique_ptr<base::Value> fetched_caps;
  local_printer_handler_->StartGetCapability(
      "invalid printer", base::BindOnce(&RecordGetCapability, &fetched_caps));

  thread_bundle_.RunUntilIdle();

  ASSERT_TRUE(fetched_caps);
  EXPECT_TRUE(fetched_caps->is_none());
}

}  // namespace printing
