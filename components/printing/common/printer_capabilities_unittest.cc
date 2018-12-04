// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/printing/common/printer_capabilities.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

const char kDpi[] = "dpi";

}  // namespace

class PrinterCapabilitiesTest : public testing::Test {
 public:
  PrinterCapabilitiesTest() {}
  ~PrinterCapabilitiesTest() override {}

 protected:
  void SetUp() override {
    test_backend_ = base::MakeRefCounted<TestPrintBackend>();
    PrintBackend::SetPrintBackendForTesting(test_backend_.get());
  }

  void TearDown() override { test_backend_ = nullptr; }

  TestPrintBackend* print_backend() { return test_backend_.get(); }

 private:
  content::TestBrowserThreadBundle test_browser_threads_;
  scoped_refptr<TestPrintBackend> test_backend_;
};

// Verify that we don't crash for a missing printer and a nullptr is never
// returned.
TEST_F(PrinterCapabilitiesTest, NonNullForMissingPrinter) {
  std::string printer_name = "missing_printer";
  PrinterBasicInfo basic_info;

  std::unique_ptr<base::DictionaryValue> settings_dictionary =
      GetSettingsOnBlockingPool(printer_name, basic_info, nullptr);

  ASSERT_TRUE(settings_dictionary);
}

TEST_F(PrinterCapabilitiesTest, ProvidedCapabilitiesUsed) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;

  // Set a capability and add a valid printer.
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  caps->dpis = {{600, 600}};
  print_backend()->AddValidPrinter(printer_name, std::move(caps));

  std::unique_ptr<base::DictionaryValue> settings_dictionary =
      GetSettingsOnBlockingPool(printer_name, basic_info, print_backend());

  // Verify settings were created.
  ASSERT_TRUE(settings_dictionary);

  // Verify capabilities dict exists and has 2 entries. (printer and version)
  base::DictionaryValue* cdd;
  ASSERT_TRUE(settings_dictionary->GetDictionary(kSettingCapabilities, &cdd));
  EXPECT_EQ(2U, cdd->size());

  // Read the CDD for the "dpi" attribute.
  base::DictionaryValue* caps_dict;
  ASSERT_TRUE(cdd->GetDictionary(kPrinter, &caps_dict));
  EXPECT_TRUE(caps_dict->HasKey(kDpi));
}

// Ensure that the capabilities dictionary is present but empty if the backend
// doesn't return capabilities.
TEST_F(PrinterCapabilitiesTest, NullCapabilitiesExcluded) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;

  // Return false when attempting to retrieve capabilities.
  print_backend()->AddValidPrinter(printer_name, nullptr);

  std::unique_ptr<base::DictionaryValue> settings_dictionary =
      GetSettingsOnBlockingPool(printer_name, basic_info, print_backend());

  // Verify settings were created.
  ASSERT_TRUE(settings_dictionary);

  // Verify that capabilities is an empty dictionary.
  base::DictionaryValue* caps_dict;
  ASSERT_TRUE(
      settings_dictionary->GetDictionary(kSettingCapabilities, &caps_dict));
  EXPECT_TRUE(caps_dict->empty());
}

}  // namespace printing
