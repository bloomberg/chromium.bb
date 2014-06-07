// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/autofill/test_popup_controller_common.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "grit/component_scaled_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/text_utils.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::NiceMock;
using base::ASCIIToUTF16;
using base::WeakPtr;

namespace autofill {
namespace {

class MockAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  MockAutofillExternalDelegate(AutofillManager* autofill_manager,
                               AutofillDriver* autofill_driver)
      : AutofillExternalDelegate(autofill_manager, autofill_driver) {}
  virtual ~MockAutofillExternalDelegate() {}

  virtual void DidSelectSuggestion(const base::string16& value,
                                   int identifier) OVERRIDE {}
  virtual void RemoveSuggestion(const base::string16& value,
                                int identifier) OVERRIDE {}
  virtual void ClearPreviewedForm() OVERRIDE {}
  base::WeakPtr<AutofillExternalDelegate> GetWeakPtr() {
    return AutofillExternalDelegate::GetWeakPtr();
  }
};

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MockAutofillClient() : prefs_(autofill::test::PrefServiceForTesting()) {}
  virtual ~MockAutofillClient() {}

  virtual PrefService* GetPrefs() OVERRIDE { return prefs_.get(); }

 private:
  scoped_ptr<PrefService> prefs_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

class TestAutofillPopupController : public AutofillPopupControllerImpl {
 public:
  explicit TestAutofillPopupController(
      base::WeakPtr<AutofillExternalDelegate> external_delegate,
      const gfx::RectF& element_bounds)
      : AutofillPopupControllerImpl(
            external_delegate, NULL, NULL, element_bounds,
            base::i18n::UNKNOWN_DIRECTION),
        test_controller_common_(new TestPopupControllerCommon(element_bounds)) {
    controller_common_.reset(test_controller_common_);
  }
  virtual ~TestAutofillPopupController() {}

  void set_display(const gfx::Display& display) {
    test_controller_common_->set_display(display);
  }

  // Making protected functions public for testing
  using AutofillPopupControllerImpl::SetPopupBounds;
  using AutofillPopupControllerImpl::names;
  using AutofillPopupControllerImpl::subtexts;
  using AutofillPopupControllerImpl::identifiers;
  using AutofillPopupControllerImpl::selected_line;
  using AutofillPopupControllerImpl::SetSelectedLine;
  using AutofillPopupControllerImpl::SelectNextLine;
  using AutofillPopupControllerImpl::SelectPreviousLine;
  using AutofillPopupControllerImpl::RemoveSelectedLine;
  using AutofillPopupControllerImpl::popup_bounds;
  using AutofillPopupControllerImpl::element_bounds;
#if !defined(OS_ANDROID)
  using AutofillPopupControllerImpl::GetNameFontListForRow;
  using AutofillPopupControllerImpl::subtext_font_list;
  using AutofillPopupControllerImpl::RowWidthWithoutText;
#endif
  using AutofillPopupControllerImpl::SetValues;
  using AutofillPopupControllerImpl::GetDesiredPopupWidth;
  using AutofillPopupControllerImpl::GetDesiredPopupHeight;
  using AutofillPopupControllerImpl::GetWeakPtr;
  MOCK_METHOD1(InvalidateRow, void(size_t));
  MOCK_METHOD0(UpdateBoundsAndRedrawPopup, void());
  MOCK_METHOD0(Hide, void());

  void DoHide() {
    AutofillPopupControllerImpl::Hide();
  }

 private:
  virtual void ShowView() OVERRIDE {}

  TestPopupControllerCommon* test_controller_common_;
};

}  // namespace

class AutofillPopupControllerUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillPopupControllerUnitTest()
      : autofill_client_(new MockAutofillClient()),
        autofill_popup_controller_(NULL) {}
  virtual ~AutofillPopupControllerUnitTest() {}

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();

    ContentAutofillDriver::CreateForWebContentsAndDelegate(
        web_contents(),
        autofill_client_.get(),
        "en-US",
        AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);
    ContentAutofillDriver* driver =
        ContentAutofillDriver::FromWebContents(web_contents());
    external_delegate_.reset(
        new NiceMock<MockAutofillExternalDelegate>(
            driver->autofill_manager(),
            driver));

    autofill_popup_controller_ =
        new testing::NiceMock<TestAutofillPopupController>(
            external_delegate_->GetWeakPtr(),gfx::Rect());
  }

  virtual void TearDown() OVERRIDE {
    // This will make sure the controller and the view (if any) are both
    // cleaned up.
    if (autofill_popup_controller_)
      autofill_popup_controller_->DoHide();

    external_delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestAutofillPopupController* popup_controller() {
    return autofill_popup_controller_;
  }

  MockAutofillExternalDelegate* delegate() {
    return external_delegate_.get();
  }

 protected:
  scoped_ptr<MockAutofillClient> autofill_client_;
  scoped_ptr<NiceMock<MockAutofillExternalDelegate> > external_delegate_;
  testing::NiceMock<TestAutofillPopupController>* autofill_popup_controller_;
};

TEST_F(AutofillPopupControllerUnitTest, SetBounds) {
  // Ensure the popup size can be set and causes a redraw.
  gfx::Rect popup_bounds(10, 10, 100, 100);

  EXPECT_CALL(*autofill_popup_controller_,
              UpdateBoundsAndRedrawPopup());

  popup_controller()->SetPopupBounds(popup_bounds);

  EXPECT_EQ(popup_bounds, popup_controller()->popup_bounds());
}

TEST_F(AutofillPopupControllerUnitTest, ChangeSelectedLine) {
  // Set up the popup.
  std::vector<base::string16> names(2, base::string16());
  std::vector<int> autofill_ids(2, 0);
  autofill_popup_controller_->Show(names, names, names, autofill_ids);

  EXPECT_LT(autofill_popup_controller_->selected_line(), 0);
  // Check that there are at least 2 values so that the first and last selection
  // are different.
  EXPECT_GE(2,
      static_cast<int>(autofill_popup_controller_->subtexts().size()));

  // Test wrapping before the front.
  autofill_popup_controller_->SelectPreviousLine();
  EXPECT_EQ(static_cast<int>(
      autofill_popup_controller_->subtexts().size() - 1),
      autofill_popup_controller_->selected_line());

  // Test wrapping after the end.
  autofill_popup_controller_->SelectNextLine();
  EXPECT_EQ(0, autofill_popup_controller_->selected_line());
}

TEST_F(AutofillPopupControllerUnitTest, RedrawSelectedLine) {
  // Set up the popup.
  std::vector<base::string16> names(2, base::string16());
  std::vector<int> autofill_ids(2, 0);
  autofill_popup_controller_->Show(names, names, names, autofill_ids);

  // Make sure that when a new line is selected, it is invalidated so it can
  // be updated to show it is selected.
  int selected_line = 0;
  EXPECT_CALL(*autofill_popup_controller_, InvalidateRow(selected_line));
  autofill_popup_controller_->SetSelectedLine(selected_line);

  // Ensure that the row isn't invalidated if it didn't change.
  EXPECT_CALL(*autofill_popup_controller_,
              InvalidateRow(selected_line)).Times(0);
  autofill_popup_controller_->SetSelectedLine(selected_line);

  // Change back to no selection.
  EXPECT_CALL(*autofill_popup_controller_, InvalidateRow(selected_line));
  autofill_popup_controller_->SetSelectedLine(-1);
}

TEST_F(AutofillPopupControllerUnitTest, RemoveLine) {
  // Set up the popup.
  std::vector<base::string16> names(3, base::string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(1);
  autofill_ids.push_back(1);
  autofill_ids.push_back(POPUP_ITEM_ID_AUTOFILL_OPTIONS);
  autofill_popup_controller_->Show(names, names, names, autofill_ids);

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  autofill::GenerateTestAutofillPopup(external_delegate_.get());

  // No line is selected so the removal should fail.
  EXPECT_FALSE(autofill_popup_controller_->RemoveSelectedLine());

  // Try to remove the last entry and ensure it fails (it is an option).
  autofill_popup_controller_->SetSelectedLine(
      autofill_popup_controller_->subtexts().size() - 1);
  EXPECT_FALSE(autofill_popup_controller_->RemoveSelectedLine());
  EXPECT_LE(0, autofill_popup_controller_->selected_line());

  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(*autofill_popup_controller_, UpdateBoundsAndRedrawPopup());
  autofill_popup_controller_->SetSelectedLine(0);
  EXPECT_TRUE(autofill_popup_controller_->RemoveSelectedLine());

  // Remove the last entry. The popup should then be hidden since there are
  // no Autofill entries left.
  EXPECT_CALL(*autofill_popup_controller_, Hide());
  autofill_popup_controller_->SetSelectedLine(0);
  EXPECT_TRUE(autofill_popup_controller_->RemoveSelectedLine());
}

TEST_F(AutofillPopupControllerUnitTest, RemoveOnlyLine) {
  // Set up the popup.
  std::vector<base::string16> names(1, base::string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(1);
  autofill_popup_controller_->Show(names, names, names, autofill_ids);

  // Generate a popup.
  autofill::GenerateTestAutofillPopup(external_delegate_.get());

  // Select the only line.
  autofill_popup_controller_->SetSelectedLine(0);

  // Remove the only line. There should be no row invalidation and the popup
  // should then be hidden since there are no Autofill entries left.
  EXPECT_CALL(*autofill_popup_controller_, Hide());
  EXPECT_CALL(*autofill_popup_controller_, InvalidateRow(_)).Times(0);
  EXPECT_TRUE(autofill_popup_controller_->RemoveSelectedLine());
}

TEST_F(AutofillPopupControllerUnitTest, SkipSeparator) {
  // Set up the popup.
  std::vector<base::string16> names(3, base::string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(1);
  autofill_ids.push_back(POPUP_ITEM_ID_SEPARATOR);
  autofill_ids.push_back(POPUP_ITEM_ID_AUTOFILL_OPTIONS);
  autofill_popup_controller_->Show(names, names, names, autofill_ids);

  autofill_popup_controller_->SetSelectedLine(0);

  // Make sure next skips the unselectable separator.
  autofill_popup_controller_->SelectNextLine();
  EXPECT_EQ(2, autofill_popup_controller_->selected_line());

  // Make sure previous skips the unselectable separator.
  autofill_popup_controller_->SelectPreviousLine();
  EXPECT_EQ(0, autofill_popup_controller_->selected_line());
}

TEST_F(AutofillPopupControllerUnitTest, RowWidthWithoutText) {
  std::vector<base::string16> names(4);
  std::vector<base::string16> subtexts(4);
  std::vector<base::string16> icons(4);
  std::vector<int> ids(4);

  // Set up some visible display so the text values are kept.
  gfx::Display display(0, gfx::Rect(0, 0, 100, 100));
  autofill_popup_controller_->set_display(display);

  // Give elements 1 and 3 subtexts and elements 2 and 3 icons, to ensure
  // all combinations of subtexts and icons.
  subtexts[1] = ASCIIToUTF16("x");
  subtexts[3] = ASCIIToUTF16("x");
  icons[2] = ASCIIToUTF16("americanExpressCC");
  icons[3] = ASCIIToUTF16("genericCC");
  autofill_popup_controller_->Show(names, subtexts, icons, ids);

  int base_size =
      AutofillPopupView::kEndPadding * 2 +
      kPopupBorderThickness * 2;
  int subtext_increase = AutofillPopupView::kNamePadding;

  EXPECT_EQ(base_size, autofill_popup_controller_->RowWidthWithoutText(0));
  EXPECT_EQ(base_size + subtext_increase,
            autofill_popup_controller_->RowWidthWithoutText(1));
  EXPECT_EQ(base_size + AutofillPopupView::kIconPadding +
                ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                    IDR_AUTOFILL_CC_AMEX).Width(),
            autofill_popup_controller_->RowWidthWithoutText(2));
  EXPECT_EQ(base_size + subtext_increase + AutofillPopupView::kIconPadding +
                ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                    IDR_AUTOFILL_CC_GENERIC).Width(),
            autofill_popup_controller_->RowWidthWithoutText(3));
}

TEST_F(AutofillPopupControllerUnitTest, UpdateDataListValues) {
  std::vector<base::string16> items;
  items.push_back(base::string16());
  std::vector<int> ids;
  ids.push_back(1);

  autofill_popup_controller_->Show(items, items, items, ids);

  EXPECT_EQ(items, autofill_popup_controller_->names());
  EXPECT_EQ(ids, autofill_popup_controller_->identifiers());

  // Add one data list entry.
  std::vector<base::string16> data_list_values;
  data_list_values.push_back(ASCIIToUTF16("data list value 1"));

  autofill_popup_controller_->UpdateDataListValues(data_list_values,
                                                   data_list_values);

  // Update the expected values.
  items.insert(items.begin(), data_list_values[0]);
  items.insert(items.begin() + 1, base::string16());
  ids.insert(ids.begin(), POPUP_ITEM_ID_DATALIST_ENTRY);
  ids.insert(ids.begin() + 1, POPUP_ITEM_ID_SEPARATOR);

  EXPECT_EQ(items, autofill_popup_controller_->names());
  EXPECT_EQ(ids, autofill_popup_controller_->identifiers());

  // Add two data list entries (which should replace the current one).
  data_list_values.push_back(ASCIIToUTF16("data list value 2"));

  autofill_popup_controller_->UpdateDataListValues(data_list_values,
                                                   data_list_values);

  // Update the expected values.
  items.insert(items.begin() + 1, data_list_values[1]);
  ids.insert(ids.begin(), POPUP_ITEM_ID_DATALIST_ENTRY);

  EXPECT_EQ(items, autofill_popup_controller_->names());
  EXPECT_EQ(ids, autofill_popup_controller_->identifiers());

  // Clear all data list values.
  data_list_values.clear();

  autofill_popup_controller_->UpdateDataListValues(data_list_values,
                                                   data_list_values);

  items.clear();
  items.push_back(base::string16());
  ids.clear();
  ids.push_back(1);

  EXPECT_EQ(items, autofill_popup_controller_->names());
  EXPECT_EQ(ids, autofill_popup_controller_->identifiers());
}

TEST_F(AutofillPopupControllerUnitTest, PopupsWithOnlyDataLists) {
  // Create the popup with a single datalist element.
  std::vector<base::string16> items;
  items.push_back(base::string16());
  std::vector<int> ids;
  ids.push_back(POPUP_ITEM_ID_DATALIST_ENTRY);

  autofill_popup_controller_->Show(items, items, items, ids);

  EXPECT_EQ(items, autofill_popup_controller_->names());
  EXPECT_EQ(ids, autofill_popup_controller_->identifiers());

  // Replace the datalist element with a new one.
  std::vector<base::string16> data_list_values;
  data_list_values.push_back(ASCIIToUTF16("data list value 1"));

  autofill_popup_controller_->UpdateDataListValues(data_list_values,
                                                   data_list_values);

  EXPECT_EQ(data_list_values, autofill_popup_controller_->names());
  // The id value should stay the same.
  EXPECT_EQ(ids, autofill_popup_controller_->identifiers());

  // Clear datalist values and check that the popup becomes hidden.
  EXPECT_CALL(*autofill_popup_controller_, Hide());
  data_list_values.clear();
  autofill_popup_controller_->UpdateDataListValues(data_list_values,
                                                   data_list_values);
}

TEST_F(AutofillPopupControllerUnitTest, GetOrCreate) {
  ContentAutofillDriver* driver =
      ContentAutofillDriver::FromWebContents(web_contents());
  MockAutofillExternalDelegate delegate(driver->autofill_manager(), driver);

  WeakPtr<AutofillPopupControllerImpl> controller =
      AutofillPopupControllerImpl::GetOrCreate(
          WeakPtr<AutofillPopupControllerImpl>(), delegate.GetWeakPtr(),
          NULL, NULL, gfx::Rect(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_TRUE(controller.get());

  controller->Hide();

  controller = AutofillPopupControllerImpl::GetOrCreate(
      WeakPtr<AutofillPopupControllerImpl>(), delegate.GetWeakPtr(),
      NULL, NULL, gfx::Rect(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_TRUE(controller.get());

  WeakPtr<AutofillPopupControllerImpl> controller2 =
      AutofillPopupControllerImpl::GetOrCreate(controller,
                                               delegate.GetWeakPtr(),
                                               NULL,
                                               NULL,
                                               gfx::Rect(),
                                               base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(controller.get(), controller2.get());
  controller->Hide();

  testing::NiceMock<TestAutofillPopupController>* test_controller =
      new testing::NiceMock<TestAutofillPopupController>(delegate.GetWeakPtr(),
                                                         gfx::Rect());
  EXPECT_CALL(*test_controller, Hide());

  gfx::RectF bounds(0.f, 0.f, 1.f, 2.f);
  base::WeakPtr<AutofillPopupControllerImpl> controller3 =
      AutofillPopupControllerImpl::GetOrCreate(
          test_controller->GetWeakPtr(),
          delegate.GetWeakPtr(),
          NULL,
          NULL,
          bounds,
          base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(
      bounds,
      static_cast<AutofillPopupController*>(controller3.get())->
          element_bounds());
  controller3->Hide();

  // Hide the test_controller to delete it.
  test_controller->DoHide();
}

TEST_F(AutofillPopupControllerUnitTest, ProperlyResetController) {
  std::vector<base::string16> names(2);
  std::vector<int> ids(2);
  popup_controller()->SetValues(names, names, names, ids);
  popup_controller()->SetSelectedLine(0);

  // Now show a new popup with the same controller, but with fewer items.
  WeakPtr<AutofillPopupControllerImpl> controller =
      AutofillPopupControllerImpl::GetOrCreate(
          popup_controller()->GetWeakPtr(),
          delegate()->GetWeakPtr(),
          NULL,
          NULL,
          gfx::Rect(),
          base::i18n::UNKNOWN_DIRECTION);
  EXPECT_NE(0, controller->selected_line());
  EXPECT_TRUE(controller->names().empty());
}

#if !defined(OS_ANDROID)
TEST_F(AutofillPopupControllerUnitTest, ElideText) {
  std::vector<base::string16> names;
  names.push_back(ASCIIToUTF16("Text that will need to be trimmed"));
  names.push_back(ASCIIToUTF16("Untrimmed"));

  std::vector<base::string16> subtexts;
  subtexts.push_back(ASCIIToUTF16("Label that will be trimmed"));
  subtexts.push_back(ASCIIToUTF16("Untrimmed"));

  std::vector<base::string16> icons(2, ASCIIToUTF16("genericCC"));
  std::vector<int> autofill_ids(2, 0);

  // Show the popup once so we can easily generate the size it needs.
  autofill_popup_controller_->Show(names, subtexts, icons, autofill_ids);

  // Ensure the popup will be too small to display all of the first row.
  int popup_max_width =
      gfx::GetStringWidth(
          names[0], autofill_popup_controller_->GetNameFontListForRow(0)) +
      gfx::GetStringWidth(
          subtexts[0], autofill_popup_controller_->subtext_font_list()) - 25;
  gfx::Rect popup_bounds = gfx::Rect(0, 0, popup_max_width, 0);
  autofill_popup_controller_->set_display(gfx::Display(0, popup_bounds));

  autofill_popup_controller_->Show(names, subtexts, icons, autofill_ids);

  // The first element was long so it should have been trimmed.
  EXPECT_NE(names[0], autofill_popup_controller_->names()[0]);
  EXPECT_NE(subtexts[0], autofill_popup_controller_->subtexts()[0]);

  // The second element was shorter so it should be unchanged.
  EXPECT_EQ(names[1], autofill_popup_controller_->names()[1]);
  EXPECT_EQ(subtexts[1], autofill_popup_controller_->subtexts()[1]);
}
#endif

}  // namespace autofill
