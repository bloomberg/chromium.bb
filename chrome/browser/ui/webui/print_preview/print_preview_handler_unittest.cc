// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"

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
 public:
  PrintPreviewHandlerTest() :
      preview_ui_(NULL),
      preview_tab_(NULL) {
  }
  virtual ~PrintPreviewHandlerTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();

    profile()->GetPrefs()->SetBoolean(prefs::kPrintPreviewDisabled, false);

    chrome::NewTab(browser());
    EXPECT_EQ(1, browser()->tab_count());
    OpenPrintPreviewTab();
  }

  virtual void TearDown() OVERRIDE {
    DeletePrintPreviewTab();

    BrowserWithTestWindowTest::TearDown();
  }

  void OpenPrintPreviewTab() {
    TabContents* initiator_tab = chrome::GetActiveTabContents(browser());
    ASSERT_TRUE(initiator_tab);

    printing::PrintPreviewTabController* controller =
        printing::PrintPreviewTabController::GetInstance();
    ASSERT_TRUE(controller);

    initiator_tab->print_view_manager()->PrintPreviewNow();
    preview_tab_ = controller->GetOrCreatePreviewTab(initiator_tab);
    ASSERT_TRUE(preview_tab_);

    preview_ui_ = static_cast<PrintPreviewUI*>(
        preview_tab_->web_contents()->GetWebUI()->GetController());
    ASSERT_TRUE(preview_ui_);
  }

  void DeletePrintPreviewTab() {
    printing::BackgroundPrintingManager* bg_printing_manager =
        g_browser_process->background_printing_manager();
    ASSERT_TRUE(bg_printing_manager->HasPrintPreviewTab(preview_tab_));

    // Deleting TabContents* to avoid warings from pref_notifier_impl.cc
    // after the test ends.
    delete preview_tab_;
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
    base::JSONWriter::Write(&settings, &json_string);
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
    base::JSONWriter::Write(&settings, &json_string);
    ListValue args;
    args.Append(new base::StringValue(json_string));  // |args| takes ownership.
    preview_ui_->handler_->HandlePrint(&args);
  }

  PrintPreviewUI* preview_ui_;

 private:

  TabContents* preview_tab_;
};
