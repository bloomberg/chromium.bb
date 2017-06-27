// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

class PrinterCapabilitiesTest : public testing::Test {
 public:
  PrinterCapabilitiesTest() {}
  ~PrinterCapabilitiesTest() override {}

 protected:
  void SetUp() override {
    test_backend_ = new TestPrintBackend();
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
  PrinterBasicInfo basic_info;
  std::string printer_name = "missing_printer";

  std::unique_ptr<base::DictionaryValue> settings_dictionary =
      GetSettingsOnBlockingPool(printer_name, basic_info);

  ASSERT_TRUE(settings_dictionary);
}

TEST_F(PrinterCapabilitiesTest, ProvidedCapabilitiesUsed) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;
  auto caps = base::MakeUnique<PrinterSemanticCapsAndDefaults>();

  // set a capability
  caps->dpis = {gfx::Size(600, 600)};

  print_backend()->AddValidPrinter(printer_name, std::move(caps));

  std::unique_ptr<base::DictionaryValue> settings_dictionary =
      GetSettingsOnBlockingPool(printer_name, basic_info);

  // verify settings were created
  ASSERT_TRUE(settings_dictionary);

  // verify capabilities and have one entry
  base::DictionaryValue* cdd;
  ASSERT_TRUE(settings_dictionary->GetDictionary("capabilities", &cdd));

  // read the CDD for the dpi attribute.
  base::DictionaryValue* caps_dict;
  ASSERT_TRUE(cdd->GetDictionary("printer", &caps_dict));
  EXPECT_TRUE(caps_dict->HasKey("dpi"));
}

// Ensure that the capabilities dictionary is present but empty if the backend
// doesn't return capabilities.
TEST_F(PrinterCapabilitiesTest, NullCapabilitiesExcluded) {
  std::string printer_name = "test_printer";
  PrinterBasicInfo basic_info;

  // return false when attempting to retrieve capabilities
  print_backend()->AddValidPrinter(printer_name, nullptr);

  std::unique_ptr<base::DictionaryValue> settings_dictionary =
      GetSettingsOnBlockingPool(printer_name, basic_info);

  // verify settings were created
  ASSERT_TRUE(settings_dictionary);

  // verify that capabilities is an empty dictionary
  base::DictionaryValue* caps_dict;
  ASSERT_TRUE(settings_dictionary->GetDictionary("capabilities", &caps_dict));
  EXPECT_TRUE(caps_dict->empty());
}

}  // namespace printing
