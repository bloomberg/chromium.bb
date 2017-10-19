// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

const char kCollate[] = "collate";
const char kDisplayName[] = "display_name";
const char kDpi[] = "dpi";
const char kId[] = "id";
const char kIsDefault[] = "is_default";
const char kMediaSizes[] = "media_sizes";
const char kPagesPerSheet[] = "Pages per sheet";
const char kPaperType[] = "Paper Type";
const char kValue[] = "value";
const char kVendorCapability[] = "vendor_capability";

base::DictionaryValue GetCapabilitiesFull() {
  base::DictionaryValue printer;

  base::Value::ListStorage list_media;
  list_media.push_back(base::Value("Letter"));
  list_media.push_back(base::Value("A4"));
  printer.SetPath({kMediaSizes}, base::Value(list_media));

  base::Value::ListStorage list_dpi;
  list_dpi.push_back(base::Value(300));
  list_dpi.push_back(base::Value(600));

  base::Value::DictStorage options;
  options[kOptionKey] = std::make_unique<base::Value>(list_dpi);
  printer.SetPath({kDpi}, base::Value(options));

  printer.SetPath({kCollate}, base::Value(true));

  base::Value::ListStorage pages_per_sheet;
  for (int i = 1; i <= 8; i *= 2) {
    base::Value::DictStorage option;
    option[kDisplayName] = std::make_unique<base::Value>(std::to_string(i));
    option[kValue] = std::make_unique<base::Value>(i);
    if (i == 1)
      option[kIsDefault] = std::make_unique<base::Value>(true);
    pages_per_sheet.push_back(base::Value(option));
  }
  base::Value::DictStorage pages_per_sheet_option;
  pages_per_sheet_option[kOptionKey] =
      std::make_unique<base::Value>(pages_per_sheet);
  base::Value::DictStorage pages_per_sheet_capability;
  pages_per_sheet_capability[kDisplayName] =
      std::make_unique<base::Value>(kPagesPerSheet);
  pages_per_sheet_capability[kId] =
      std::make_unique<base::Value>(kPagesPerSheet);
  pages_per_sheet_capability[kTypeKey] =
      std::make_unique<base::Value>(kSelectString);
  pages_per_sheet_capability[kSelectCapKey] =
      std::make_unique<base::Value>(pages_per_sheet_option);

  base::Value::ListStorage paper_types;
  base::Value::DictStorage option1;
  option1[kDisplayName] = std::make_unique<base::Value>("Plain");
  option1[kValue] = std::make_unique<base::Value>("Plain");
  option1[kIsDefault] = std::make_unique<base::Value>(true);
  base::Value::DictStorage option2;
  option2[kDisplayName] = std::make_unique<base::Value>("Photo");
  option2[kValue] = std::make_unique<base::Value>("Photo");
  paper_types.push_back(base::Value(option1));
  paper_types.push_back(base::Value(option2));
  base::Value::DictStorage paper_type_option;
  paper_type_option[kOptionKey] = std::make_unique<base::Value>(paper_types);
  base::Value::DictStorage paper_type_capability;
  paper_type_capability[kDisplayName] =
      std::make_unique<base::Value>(kPaperType);
  paper_type_capability[kId] = std::make_unique<base::Value>(kPaperType);
  paper_type_capability[kTypeKey] =
      std::make_unique<base::Value>(kSelectString);
  paper_type_capability[kSelectCapKey] =
      std::make_unique<base::Value>(paper_type_option);

  base::Value::ListStorage vendor_capabilities;
  vendor_capabilities.push_back(base::Value(pages_per_sheet_capability));
  vendor_capabilities.push_back(base::Value(paper_type_capability));
  printer.SetPath({kVendorCapability}, base::Value(vendor_capabilities));

  return printer;
}

base::Value ValidList(const base::Value* list) {
  auto out_list = list->Clone();
  base::EraseIf(out_list.GetList(),
                [](const base::Value& v) { return v.is_none(); });
  return out_list;
}

bool HasValidEntry(const base::Value* list) {
  return list && !list->GetList().empty() && !ValidList(list).GetList().empty();
}

void CompareStringKeys(const base::Value& expected,
                       const base::Value& actual,
                       base::StringPiece key) {
  EXPECT_EQ(*(expected.FindPathOfType({key}, base::Value::Type::STRING)),
            *(actual.FindPathOfType({key}, base::Value::Type::STRING)));
}

void ValidateList(const base::Value* list_out, const base::Value* input_list) {
  auto input_list_valid = ValidList(input_list);
  ASSERT_EQ(list_out->GetList().size(), input_list_valid.GetList().size());
  for (size_t index = 0; index < list_out->GetList().size(); index++) {
    EXPECT_EQ(list_out->GetList()[index], input_list_valid.GetList()[index]);
  }
}

void ValidateMedia(const base::Value* printer_out,
                   const base::Value* expected_list) {
  const base::Value* media_out =
      printer_out->FindPathOfType({kMediaSizes}, base::Value::Type::LIST);
  if (!HasValidEntry(expected_list)) {
    EXPECT_FALSE(media_out);
    return;
  }
  ValidateList(media_out, expected_list);
}

void ValidateDpi(const base::Value* printer_out,
                 const base::Value* expected_dpi) {
  const base::Value* dpi_option_out =
      printer_out->FindPathOfType({kDpi}, base::Value::Type::DICTIONARY);
  if (!expected_dpi) {
    EXPECT_FALSE(dpi_option_out);
    return;
  }
  const base::Value* dpi_list =
      expected_dpi->FindPathOfType({kOptionKey}, base::Value::Type::LIST);
  if (!HasValidEntry(dpi_list)) {
    EXPECT_FALSE(dpi_option_out);
    return;
  }
  ASSERT_TRUE(dpi_option_out);
  const base::Value* dpi_list_out =
      dpi_option_out->FindPathOfType({kOptionKey}, base::Value::Type::LIST);
  ASSERT_TRUE(dpi_list_out);
  ValidateList(dpi_list_out, dpi_list);
}

void ValidateCollate(const base::Value* printer_out) {
  const base::Value* collate_out =
      printer_out->FindPathOfType({kCollate}, base::Value::Type::BOOLEAN);
  ASSERT_TRUE(collate_out);
}

void ValidateVendorCaps(const base::Value* printer_out,
                        const base::Value* input_vendor_caps) {
  const base::Value* vendor_capability_out =
      printer_out->FindPathOfType({kVendorCapability}, base::Value::Type::LIST);
  if (!HasValidEntry(input_vendor_caps)) {
    ASSERT_FALSE(vendor_capability_out);
    return;
  }

  ASSERT_TRUE(vendor_capability_out);
  size_t index = 0;
  const base::Value::ListStorage& output_list =
      vendor_capability_out->GetList();
  for (const auto& input_entry : input_vendor_caps->GetList()) {
    if (!HasValidEntry(
            input_entry
                .FindPathOfType({kSelectCapKey}, base::Value::Type::DICTIONARY)
                ->FindPathOfType({kOptionKey}, base::Value::Type::LIST))) {
      continue;
    }
    CompareStringKeys(input_entry, output_list[index], kDisplayName);
    CompareStringKeys(input_entry, output_list[index], kId);
    CompareStringKeys(input_entry, output_list[index], kTypeKey);
    const base::Value* select_cap = output_list[index].FindPathOfType(
        {kSelectCapKey}, base::Value::Type::DICTIONARY);
    ASSERT_TRUE(select_cap);
    const base::Value* list =
        select_cap->FindPathOfType({kOptionKey}, base::Value::Type::LIST);
    ASSERT_TRUE(list);
    ValidateList(
        list,
        input_entry
            .FindPathOfType({kSelectCapKey}, base::Value::Type::DICTIONARY)
            ->FindPathOfType({kOptionKey}, base::Value::Type::LIST));
    index++;
  }
}

void ValidatePrinter(const base::DictionaryValue* cdd_out,
                     const base::DictionaryValue& printer) {
  const base::Value* printer_out =
      cdd_out->FindPathOfType({kPrinter}, base::Value::Type::DICTIONARY);
  ASSERT_TRUE(printer_out);

  const base::Value* media =
      printer.FindPathOfType({kMediaSizes}, base::Value::Type::LIST);
  ValidateMedia(printer_out, media);

  const base::Value* dpi_dict =
      printer.FindPathOfType({kDpi}, base::Value::Type::DICTIONARY);
  ValidateDpi(printer_out, dpi_dict);
  ValidateCollate(printer_out);

  const base::Value* capabilities_list =
      printer.FindPathOfType({kVendorCapability}, base::Value::Type::LIST);
  ValidateVendorCaps(printer_out, capabilities_list);
}

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
  ASSERT_TRUE(settings_dictionary->GetDictionary(kSettingCapabilities, &cdd));

  // read the CDD for the dpi attribute.
  base::DictionaryValue* caps_dict;
  ASSERT_TRUE(cdd->GetDictionary(kPrinter, &caps_dict));
  EXPECT_TRUE(caps_dict->HasKey(kDpi));
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
  ASSERT_TRUE(
      settings_dictionary->GetDictionary(kSettingCapabilities, &caps_dict));
  EXPECT_TRUE(caps_dict->empty());
}

TEST_F(PrinterCapabilitiesTest, FullCddPassthrough) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  base::DictionaryValue cdd;
  cdd.SetPath({kPrinter}, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(cdd);
  ValidatePrinter(cdd_out.get(), printer);
}

TEST_F(PrinterCapabilitiesTest, FilterBadList) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  printer.RemovePath({kMediaSizes});
  base::Value::ListStorage list_media;
  list_media.push_back(base::Value());
  list_media.push_back(base::Value());
  printer.SetPath({kMediaSizes}, base::Value(list_media));
  base::DictionaryValue cdd;
  cdd.SetPath({kPrinter}, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(cdd);
  ValidatePrinter(cdd_out.get(), printer);
}

TEST_F(PrinterCapabilitiesTest, FilterBadOptionOneElement) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  printer.RemovePath({kDpi});
  base::Value::DictStorage options;
  base::Value::ListStorage list_dpi;
  list_dpi.push_back(base::Value());
  list_dpi.push_back(base::Value(600));
  options[kOptionKey] = std::make_unique<base::Value>(list_dpi);
  printer.SetPath({kDpi}, base::Value(options));
  base::DictionaryValue cdd;
  cdd.SetPath({kPrinter}, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(cdd);
  ValidatePrinter(cdd_out.get(), printer);
}

TEST_F(PrinterCapabilitiesTest, FilterBadOptionAllElement) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  printer.RemovePath({kDpi});
  base::Value::DictStorage options;
  base::Value::ListStorage list_dpi;
  list_dpi.push_back(base::Value());
  list_dpi.push_back(base::Value());
  options[kOptionKey] = std::make_unique<base::Value>(list_dpi);
  printer.SetPath({kDpi}, base::Value(options));
  base::DictionaryValue cdd;
  cdd.SetPath({kPrinter}, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(cdd);
  ValidatePrinter(cdd_out.get(), printer);
}

TEST_F(PrinterCapabilitiesTest, FilterBadVendorCapabilityAllElement) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  base::Value* select_cap_0 =
      printer.FindPathOfType({kVendorCapability}, base::Value::Type::LIST)
          ->GetList()[0]
          .FindPathOfType({kSelectCapKey}, base::Value::Type::DICTIONARY);
  select_cap_0->RemovePath({kOptionKey});
  base::Value::ListStorage option_list;
  option_list.push_back(base::Value());
  option_list.push_back(base::Value());
  select_cap_0->SetPath({kOptionKey}, base::Value(option_list));
  base::DictionaryValue cdd;
  cdd.SetPath({kPrinter}, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(cdd);
  ValidatePrinter(cdd_out.get(), printer);
}

TEST_F(PrinterCapabilitiesTest, FilterBadVendorCapabilityOneElement) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  base::Value* vendor_dictionary =
      printer.FindPathOfType({kVendorCapability}, base::Value::Type::LIST)
          ->GetList()[0]
          .FindPathOfType({kSelectCapKey}, base::Value::Type::DICTIONARY);
  vendor_dictionary->RemovePath({kOptionKey});
  base::Value::ListStorage pages_per_sheet;
  for (int i = 1; i <= 8; i *= 2) {
    if (i == 2) {
      pages_per_sheet.push_back(base::Value());
      continue;
    }
    base::Value::DictStorage option;
    option[kDisplayName] = std::make_unique<base::Value>(std::to_string(i));
    option[kValue] = std::make_unique<base::Value>(i);
    if (i == 1)
      option[kIsDefault] = std::make_unique<base::Value>(true);
    pages_per_sheet.push_back(base::Value(option));
  }
  vendor_dictionary->SetPath({kOptionKey}, base::Value(pages_per_sheet));

  base::DictionaryValue cdd;
  cdd.SetPath({kPrinter}, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(cdd);
  ValidatePrinter(cdd_out.get(), printer);
}

}  // namespace printing
