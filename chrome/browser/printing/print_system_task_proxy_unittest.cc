// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cups/cups.h>
#include <cups/ppd.h>

#include <cstring>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/printing/print_system_task_proxy.h"
#include "printing/backend/print_backend.h"
#include "printing/print_job_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_MACOSX)
namespace {

// TestEntry stores the printer name and the expected number of options.
struct TestEntry {
  TestEntry(std::string name, int count)
      : printer_name(name),
        expected_option_count(count) {}

  std::string printer_name;
  int expected_option_count;
};

// Verify the option marked in |ppd|.
void verifyOptionValue(ppd_file_t* ppd,
                       const std::string& option_name,
                       const std::string& expected_choice_value) {
  ppd_choice_t* option_choice = ppdFindMarkedChoice(ppd, option_name.c_str());
  if (option_choice == NULL) {
    ppd_option_t* option = ppdFindOption(ppd, option_name.c_str());
    if (option != NULL)
      option_choice = ppdFindChoice(option, option->defchoice);
  }
  ASSERT_TRUE(option_choice);
  EXPECT_EQ(strcmp(option_choice->choice, expected_choice_value.c_str()), 0);
}

}  // namespace

using printing_internal::parse_lpoptions;

// Test to verify that lpoption custom settings are marked on the ppd file.
TEST(PrintSystemTaskProxyTest, MarkLpoptionsInPPD) {
  const std::string kColorModel = "ColorModel";
  const std::string kBlack = "Black";
  const std::string kGray = "Gray";

  const std::string kDuplex = "Duplex";
  const std::string kDuplexNone = "None";
  const std::string kDuplexNoTumble = "DuplexNoTumble";
  const std::string kDuplexTumble = "DuplexTumble";
  const std::string kTestPrinterName = "printerE";

  ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  std::string system_lpoptions;  // Specifies the system lpoption data.
  system_lpoptions.append("Dest printerE ColorModel=Black Duplex=");
  system_lpoptions.append(kDuplexNone+" ");

  // Create and write the system lpoptions to a temp file.
  FilePath system_lp_options_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_directory.path(),
                                                  &system_lp_options_file));
  ASSERT_TRUE(file_util::WriteFile(system_lp_options_file,
                                   system_lpoptions.c_str(),
                                   system_lpoptions.size()));

  // Specifies the user lpoption data.
  std::string user_lpoptions;
  user_lpoptions.append("Dest printerE          Duplex="+kDuplexNoTumble+"\n");

  // Create and write the user lpoptions to a temp file.
  FilePath user_lp_options_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_directory.path(),
                                                  &user_lp_options_file));
  ASSERT_TRUE(file_util::WriteFile(user_lp_options_file, user_lpoptions.c_str(),
                                   user_lpoptions.size()));
  // Specifies the test ppd data.
  std::string test_ppd_data;
  test_ppd_data.append("*PPD-Adobe: \"4.3\"\n\n"
                       "*OpenGroup: General/General\n\n"
                       "*OpenUI *ColorModel/Color Model: PickOne\n"
                       "*DefaultColorModel: Gray\n"
                       "*ColorModel Gray/Grayscale: \""
                       "<</cupsColorSpace 0/cupsColorOrder 0>>"
                       "setpagedevice\"\n"
                       "*ColorModel Black/Inverted Grayscale: \""
                       "<</cupsColorSpace 3/cupsColorOrder 0>>"
                       "setpagedevice\"\n"
                       "*CloseUI: *ColorModel\n"
                       "*OpenUI *Duplex/2-Sided Printing: PickOne\n"
                       "*DefaultDuplex: DuplexTumble\n"
                       "*Duplex None/Off: \"<</Duplex false>>"
                       "setpagedevice\"\n"
                       "*Duplex DuplexNoTumble/LongEdge: \""
                       "<</Duplex true/Tumble false>>setpagedevice\"\n"
                       "*Duplex DuplexTumble/ShortEdge: \""
                       "<</Duplex true/Tumble true>>setpagedevice\"\n"
                       "*CloseUI: *Duplex\n\n"
                       "*CloseGroup: General\n");

  // Create a test ppd file.
  FilePath ppd_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_directory.path(),
                                                  &ppd_file_path));
  ASSERT_TRUE(file_util::WriteFile(ppd_file_path, test_ppd_data.c_str(),
                                   test_ppd_data.size()));

  ppd_file_t* ppd = ppdOpenFile(ppd_file_path.value().c_str());
  ASSERT_TRUE(ppd);
  ppdMarkDefaults(ppd);

  // Verify the default settings.
  verifyOptionValue(ppd, kDuplex, kDuplexTumble);
  verifyOptionValue(ppd, kColorModel, kGray);

  // Parse the system lpoptions data.
  int num_options = 0;
  cups_option_t* options = NULL;
  parse_lpoptions(system_lp_options_file, kTestPrinterName, &num_options,
                  &options);
  ASSERT_EQ(num_options, 2);
  EXPECT_EQ(num_options != 0, options != NULL);
  cupsMarkOptions(ppd, num_options, options);
  cupsFreeOptions(num_options, options);

  // Verify that the settings are updated as per system lpoptions.
  verifyOptionValue(ppd, kDuplex, kDuplexNone);
  verifyOptionValue(ppd, kColorModel, kBlack);

  // Parse the user lpoptions data.
  num_options = 0;
  options = NULL;
  parse_lpoptions(user_lp_options_file, kTestPrinterName, &num_options,
                  &options);
  ASSERT_EQ(num_options, 1);
  EXPECT_EQ(num_options != 0, options != NULL);
  cupsMarkOptions(ppd, num_options, options);
  cupsFreeOptions(num_options, options);

  // Verify that the settings are updated as per user lpoptions. Make sure
  // duplex setting is updated but the color setting remains the same.
  verifyOptionValue(ppd, kDuplex, kDuplexNoTumble);
  verifyOptionValue(ppd, kColorModel, kBlack);
  ppdClose(ppd);
}

// Test the lpoption parsing code.
TEST(PrintSystemTaskProxyTest, ParseLpoptionData) {
  // Specifies the user lpoption data.
  std::string user_lpoptions;

  // Printer A default printer settings.
  user_lpoptions.append("Default printerA Duplex=None landscape=true ");
  user_lpoptions.append("media=A4 Collate=True sides=two-sided-long-edge ");
  user_lpoptions.append("ColorModel=Color nUp=7\n");

  // PrinterB custom settings.
  user_lpoptions.append("Dest printerB Duplex=None scaling=98 ");
  user_lpoptions.append("landscape=true media=A4 collate=True\n");

  // PrinterC has a invalid key and value but the format is valid.
  user_lpoptions.append("Dest printerC invalidKey1=invalidValue1 ");
  user_lpoptions.append("invalidKey2=invalidValue2 ");
  user_lpoptions.append("invalidKey3=invalidValue3 \n");

  // PrinterA instance custom settings. These settings will not override
  // PrinterA settings.
  user_lpoptions.append("Dest printerA/instanceA ");
  user_lpoptions.append("scaling=33 Duplex=DuplexTumble landscape=true\n");

  // PrinterD custom settings but the format is invalid because of the tab key
  // delimiter.
  user_lpoptions.append("Dest printerD\tDuplex=DuplexNoTumble\n");

  // PrinterE custom settings.
  user_lpoptions.append("Dest printerE          Duplex=DuplexNoTumble\n");

  ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  // Create and write the user lpoptions to a temp file.
  FilePath userLpOptionsFile;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_directory.path(),
                                                  &userLpOptionsFile));
  ASSERT_TRUE(file_util::WriteFile(userLpOptionsFile, user_lpoptions.c_str(),
                                   user_lpoptions.size()));
  std::vector<TestEntry> test_cases;
  test_cases.push_back(
      TestEntry("printerA", 7));  // Parse generic printer.
  test_cases.push_back(
      TestEntry("printerB", 5));  // Valid printer info.
  test_cases.push_back(
      TestEntry("printerC", 3));  // Invalid settings found.
  test_cases.push_back(
      TestEntry("printerD", 0));  // Tab key delimiter used.
  test_cases.push_back(
      TestEntry("printerE", 1));  // user specified custom settings.
  test_cases.push_back(
      TestEntry("printerF", 0));  // Custom settings not found.

  // Parse the lpoptions for each printer. Parse the system file followed by the
  // user file. Ordering is important.
  int num_options;
  cups_option_t* options;
  for (std::vector<TestEntry>::iterator it = test_cases.begin();
       it != test_cases.end(); ++it) {
    num_options = 0;
    options = NULL;
    printing_internal::parse_lpoptions(userLpOptionsFile, it->printer_name,
                                       &num_options, &options);
    ASSERT_EQ(num_options, it->expected_option_count);
    EXPECT_EQ(num_options != 0, options != NULL);
    cupsFreeOptions(num_options, options);
  }
}
#endif  // !defined(OS_MACOSX)

// Test duplex detection code, which regressed in http://crbug.com/103999.
TEST(PrintSystemTaskProxyTest, DetectDuplexModeCUPS) {
  // Specifies the test ppd data.
  printing::PrinterCapsAndDefaults printer_info;
  printer_info.printer_capabilities.append(
      "*PPD-Adobe: \"4.3\"\n\n"
      "*OpenGroup: General/General\n\n"
      "*OpenUI *Duplex/Double-Sided Printing: PickOne\n"
      "*DefaultDuplex: None\n"
      "*Duplex None/Off: "
      "\"<</Duplex false>>setpagedevice\"\n"
      "*Duplex DuplexNoTumble/Long Edge (Standard): "
      "\"<</Duplex true/Tumble false>>setpagedevice\"\n"
      "*Duplex DuplexTumble/Short Edge (Flip): "
      "\"<</Duplex true/Tumble true>>setpagedevice\"\n"
      "*CloseUI: *Duplex\n\n"
      "*CloseGroup: General\n");

  bool set_color_as_default = false;
  bool set_duplex_as_default = false;
  int printer_color_space_for_color = printing::UNKNOWN_COLOR_MODEL;
  int printer_color_space_for_black = printing::UNKNOWN_COLOR_MODEL;
  int default_duplex_setting_value = printing::UNKNOWN_DUPLEX_MODE;

  scoped_refptr<PrintSystemTaskProxy> test_proxy;
  bool res = test_proxy->GetPrinterCapabilitiesCUPS(
      printer_info,
      "InvalidPrinter",
      &set_color_as_default,
      &printer_color_space_for_color,
      &printer_color_space_for_black,
      &set_duplex_as_default,
      &default_duplex_setting_value);
  ASSERT_TRUE(res);
  EXPECT_FALSE(set_duplex_as_default);
  EXPECT_EQ(printing::SIMPLEX, default_duplex_setting_value);
}

TEST(PrintSystemTaskProxyTest, DetectNoDuplexModeCUPS) {
  // Specifies the test ppd data.
  printing::PrinterCapsAndDefaults printer_info;
  printer_info.printer_capabilities.append(
      "*PPD-Adobe: \"4.3\"\n\n"
      "*OpenGroup: General/General\n\n"
      "*CloseGroup: General\n");

  bool set_color_as_default = false;
  bool set_duplex_as_default = false;
  int printer_color_space_for_color = printing::UNKNOWN_COLOR_MODEL;
  int printer_color_space_for_black = printing::UNKNOWN_COLOR_MODEL;
  int default_duplex_setting_value = printing::UNKNOWN_DUPLEX_MODE;

  scoped_refptr<PrintSystemTaskProxy> test_proxy;
  bool res = test_proxy->GetPrinterCapabilitiesCUPS(
      printer_info,
      "InvalidPrinter",
      &set_color_as_default,
      &printer_color_space_for_color,
      &printer_color_space_for_black,
      &set_duplex_as_default,
      &default_duplex_setting_value);
  ASSERT_TRUE(res);
  EXPECT_FALSE(set_duplex_as_default);
  EXPECT_EQ(printing::UNKNOWN_DUPLEX_MODE, default_duplex_setting_value);
}
