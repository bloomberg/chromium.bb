// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"

// Test crashes on Aura due to initiator tab's native view having no parent.
// http://crbug.com/104284
#if defined(USE_AURA)
#define MAYBE_StickyMarginsCustom DISABLED_StickyMarginsCustom
#define MAYBE_StickyMarginsDefault DISABLED_StickyMarginsDefault
#define MAYBE_StickyMarginsCustomThenDefault \
    DISABLED_StickyMarginsCustomThenDefault
#define MAYBE_GetLastUsedMarginSettingsCustom \
    DISABLED_GetLastUsedMarginSettingsCustom
#define MAYBE_GetLastUsedMarginSettingsDefault \
    DISABLED_GetLastUsedMarginSettingsDefault
#else
#define MAYBE_StickyMarginsCustom StickyMarginsCustom
#define MAYBE_StickyMarginsDefault StickyMarginsDefault
#define MAYBE_StickyMarginsCustomThenDefault StickyMarginsCustomThenDefault
#define MAYBE_GetLastUsedMarginSettingsCustom GetLastUsedMarginSettingsCustom
#define MAYBE_GetLastUsedMarginSettingsDefault GetLastUsedMarginSettingsDefault
#endif

namespace {

DictionaryValue* GetCustomMarginsDictionary(
    const double margin_top, const double margin_right,
    const double margin_bottom, const double margin_left) {
  base::DictionaryValue* custom_settings = new base::DictionaryValue();
  custom_settings->SetDouble(printing::kSettingMarginTop, margin_top);
  custom_settings->SetDouble(printing::kSettingMarginRight, margin_right);
  custom_settings->SetDouble(printing::kSettingMarginBottom, margin_bottom);
  custom_settings->SetDouble(printing::kSettingMarginLeft, margin_left);
  return custom_settings;
}

}  // namespace

class PrintPreviewHandlerTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() {
    BrowserWithTestWindowTest::SetUp();
#if !defined(GOOGLE_CHROME_BUILD) || defined(OS_CHROMEOS)
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnablePrintPreview);
#endif
    ASSERT_TRUE(browser());
    BrowserList::SetLastActive(browser());
    ASSERT_TRUE(BrowserList::GetLastActive());

    browser()->NewTab();
    EXPECT_EQ(1, browser()->tab_count());
    OpenPrintPreviewTab();
  }

  virtual void TearDown() {
    DeletePrintPreviewTab();
    ClearStickySettings();
  }

  void OpenPrintPreviewTab() {
    TabContentsWrapper* initiator_tab =
        browser()->GetSelectedTabContentsWrapper();
    ASSERT_TRUE(initiator_tab);

    printing::PrintPreviewTabController* controller =
        printing::PrintPreviewTabController::GetInstance();
    ASSERT_TRUE(controller);

    initiator_tab->print_view_manager()->PrintPreviewNow();
    preview_tab_ = controller->GetOrCreatePreviewTab(initiator_tab);
    ASSERT_TRUE(preview_tab_);

    preview_ui_ = static_cast<PrintPreviewUI*>(preview_tab_->web_ui());
    ASSERT_TRUE(preview_ui_);
  }

  void DeletePrintPreviewTab() {
    printing::BackgroundPrintingManager* bg_printing_manager =
        g_browser_process->background_printing_manager();
    ASSERT_TRUE(bg_printing_manager->HasPrintPreviewTab(preview_tab_));

    // Deleting TabContentsWrapper* to avoid warings from pref_notifier_impl.cc
    // after the test ends.
    delete preview_tab_;
  }

  void CheckCustomMargins(const double margin_top,
                          const double margin_right,
                          const double margin_bottom,
                          const double margin_left) {
    EXPECT_EQ(PrintPreviewHandler::last_used_page_size_margins_->margin_top,
              margin_top);
    EXPECT_EQ(PrintPreviewHandler::last_used_page_size_margins_->margin_right,
              margin_right);
    EXPECT_EQ(PrintPreviewHandler::last_used_page_size_margins_->margin_bottom,
              margin_bottom);
    EXPECT_EQ(PrintPreviewHandler::last_used_page_size_margins_->margin_left,
              margin_left);
  }

  void RequestPrintWithDefaultMargins() {
    // Set the minimal dummy settings to make the HandlePrint() code happy.
    DictionaryValue settings;
    settings.SetBoolean(printing::kSettingPreviewModifiable, true);
    settings.SetInteger(printing::kSettingColor, printing::COLOR);
    settings.SetBoolean(printing::kSettingPrintToPDF, false);
    settings.SetInteger(printing::kSettingMarginsType,
                        printing::DEFAULT_MARGINS);

    // Put |settings| in to |args| as a JSON string.
    std::string json_string;
    base::JSONWriter::Write(&settings, false, &json_string);
    ListValue args;
    args.Append(new base::StringValue(json_string));  // |args| takes ownership.
    preview_ui_->handler_->HandlePrint(&args);
  }

  void RequestPrintWithCustomMargins(
    const double margin_top, const double margin_right,
    const double margin_bottom, const double margin_left) {
    // Set the minimal dummy settings to make the HandlePrint() code happy.
    DictionaryValue settings;
    settings.SetBoolean(printing::kSettingPreviewModifiable, true);
    settings.SetInteger(printing::kSettingColor, printing::COLOR);
    settings.SetBoolean(printing::kSettingPrintToPDF, false);
    settings.SetInteger(printing::kSettingMarginsType,
                        printing::CUSTOM_MARGINS);

    // Creating custom margins dictionary and nesting it in |settings|.
    DictionaryValue* custom_settings = GetCustomMarginsDictionary(
        margin_top, margin_right, margin_bottom, margin_left);
    // |settings| takes ownership.
    settings.Set(printing::kSettingMarginsCustom, custom_settings);

    // Put |settings| in to |args| as a JSON string.
    std::string json_string;
    base::JSONWriter::Write(&settings, false, &json_string);
    ListValue args;
    args.Append(new base::StringValue(json_string));  // |args| takes ownership.
    preview_ui_->handler_->HandlePrint(&args);
  }

  TabContentsWrapper* preview_tab_;
  PrintPreviewUI* preview_ui_;

 private:
  void ClearStickySettings() {
    PrintPreviewHandler::last_used_margins_type_ = printing::DEFAULT_MARGINS;
    delete PrintPreviewHandler::last_used_page_size_margins_;
    PrintPreviewHandler::last_used_page_size_margins_ = NULL;
  }
};

// Tests that margin settings are saved correctly when printing with custom
// margins selected.
TEST_F(PrintPreviewHandlerTest, MAYBE_StickyMarginsCustom) {
  const double kMarginTop = 25.5;
  const double kMarginRight = 26.5;
  const double kMarginBottom = 27.5;
  const double kMarginLeft = 28.5;
  RequestPrintWithCustomMargins(
      kMarginTop, kMarginRight, kMarginBottom, kMarginLeft);
  EXPECT_EQ(1, browser()->tab_count());

  // Checking that sticky settings were saved correctly.
  EXPECT_EQ(PrintPreviewHandler::last_used_color_model_, printing::COLOR);
  EXPECT_EQ(PrintPreviewHandler::last_used_margins_type_,
            printing::CUSTOM_MARGINS);
  ASSERT_TRUE(PrintPreviewHandler::last_used_page_size_margins_);
  CheckCustomMargins(kMarginTop, kMarginRight, kMarginBottom, kMarginLeft);
}

// Tests that margin settings are saved correctly when printing with default
// margins selected.
TEST_F(PrintPreviewHandlerTest, MAYBE_StickyMarginsDefault) {
  RequestPrintWithDefaultMargins();
  EXPECT_EQ(1, browser()->tab_count());

  // Checking that sticky settings were saved correctly.
  EXPECT_EQ(PrintPreviewHandler::last_used_color_model_, printing::COLOR);
  EXPECT_EQ(PrintPreviewHandler::last_used_margins_type_,
            printing::DEFAULT_MARGINS);
  ASSERT_FALSE(PrintPreviewHandler::last_used_page_size_margins_);
}

// Tests that margin settings are saved correctly when printing with custom
// margins selected and then again with default margins selected.
TEST_F(PrintPreviewHandlerTest, MAYBE_StickyMarginsCustomThenDefault) {
  const double kMarginTop = 125.5;
  const double kMarginRight = 126.5;
  const double kMarginBottom = 127.5;
  const double kMarginLeft = 128.5;
  RequestPrintWithCustomMargins(
      kMarginTop, kMarginRight, kMarginBottom, kMarginLeft);
  EXPECT_EQ(1, browser()->tab_count());
  DeletePrintPreviewTab();
  EXPECT_EQ(PrintPreviewHandler::last_used_margins_type_,
            printing::CUSTOM_MARGINS);
  ASSERT_TRUE(PrintPreviewHandler::last_used_page_size_margins_);
  CheckCustomMargins(kMarginTop, kMarginRight, kMarginBottom, kMarginLeft);

  OpenPrintPreviewTab();
  RequestPrintWithDefaultMargins();

  // Checking that sticky settings were saved correctly.
  EXPECT_EQ(PrintPreviewHandler::last_used_color_model_, printing::COLOR);
  EXPECT_EQ(PrintPreviewHandler::last_used_margins_type_,
            printing::DEFAULT_MARGINS);
  ASSERT_TRUE(PrintPreviewHandler::last_used_page_size_margins_);
  CheckCustomMargins(kMarginTop, kMarginRight, kMarginBottom, kMarginLeft);
}

// Tests that margin settings are retrieved correctly after printing with custom
// margins.
TEST_F(PrintPreviewHandlerTest, MAYBE_GetLastUsedMarginSettingsCustom) {
  const double kMarginTop = 125.5;
  const double kMarginRight = 126.5;
  const double kMarginBottom = 127.5;
  const double kMarginLeft = 128.5;
  RequestPrintWithCustomMargins(
      kMarginTop, kMarginRight, kMarginBottom, kMarginLeft);
  base::DictionaryValue initial_settings;
  preview_ui_->handler_->GetLastUsedMarginSettings(&initial_settings);
  int margins_type;
  EXPECT_TRUE(initial_settings.GetInteger(printing::kSettingMarginsType,
                                          &margins_type));
  EXPECT_EQ(margins_type, printing::CUSTOM_MARGINS);
  double margin_value;
  EXPECT_TRUE(initial_settings.GetDouble(printing::kSettingMarginTop,
                                         &margin_value));
  EXPECT_EQ(kMarginTop, margin_value);
  EXPECT_TRUE(initial_settings.GetDouble(printing::kSettingMarginRight,
                                         &margin_value));
  EXPECT_EQ(kMarginRight, margin_value);
  EXPECT_TRUE(initial_settings.GetDouble(printing::kSettingMarginBottom,
                                         &margin_value));
  EXPECT_EQ(kMarginBottom, margin_value);
  EXPECT_TRUE(initial_settings.GetDouble(printing::kSettingMarginLeft,
                                         &margin_value));
  EXPECT_EQ(kMarginLeft, margin_value);
}

// Tests that margin settings are retrieved correctly after printing with
// default margins.
TEST_F(PrintPreviewHandlerTest, MAYBE_GetLastUsedMarginSettingsDefault) {
  RequestPrintWithDefaultMargins();
  base::DictionaryValue initial_settings;
  preview_ui_->handler_->GetLastUsedMarginSettings(&initial_settings);
  int margins_type;
  EXPECT_TRUE(initial_settings.GetInteger(printing::kSettingMarginsType,
                                          &margins_type));
  EXPECT_EQ(margins_type, printing::DEFAULT_MARGINS);
  double margin_value;
  EXPECT_FALSE(initial_settings.GetDouble(printing::kSettingMarginTop,
                                          &margin_value));
  EXPECT_FALSE(initial_settings.GetDouble(printing::kSettingMarginRight,
                                          &margin_value));
  EXPECT_FALSE(initial_settings.GetDouble(printing::kSettingMarginBottom,
                                          &margin_value));
  EXPECT_FALSE(initial_settings.GetDouble(printing::kSettingMarginLeft,
                                          &margin_value));
}
