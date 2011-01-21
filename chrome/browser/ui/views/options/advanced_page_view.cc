// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/advanced_page_view.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/browser/ui/views/options/advanced_contents_view.h"
#include "chrome/browser/ui/views/options/managed_prefs_banner_view.h"
#include "chrome/common/chrome_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/message_box_flags.h"
#include "views/controls/message_box_view.h"
#include "views/controls/button/native_button.h"
#include "views/controls/scroll_view.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace {

// A dialog box that asks the user to confirm resetting settings.
class ResetDefaultsConfirmBox : public views::DialogDelegate {
 public:
  // This box is modal to |parent_hwnd|.
  static void ShowConfirmBox(HWND parent_hwnd, AdvancedPageView* page_view) {
    // When the window closes, it will delete itself.
    new ResetDefaultsConfirmBox(parent_hwnd, page_view);
  }

 protected:
  // views::DialogDelegate
  virtual std::wstring GetDialogButtonLabel(
      ui::MessageBoxFlags::DialogButton button) const {
    switch (button) {
      case ui::MessageBoxFlags::DIALOGBUTTON_OK:
        return UTF16ToWide(
            l10n_util::GetStringUTF16(IDS_OPTIONS_RESET_OKLABEL));
      case ui::MessageBoxFlags::DIALOGBUTTON_CANCEL:
        return UTF16ToWide(
            l10n_util::GetStringUTF16(IDS_OPTIONS_RESET_CANCELLABEL));
      default:
        break;
    }
    NOTREACHED();
    return std::wstring();
  }
  virtual std::wstring GetWindowTitle() const {
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  }
  virtual bool Accept() {
    advanced_page_view_->ResetToDefaults();
    return true;
  }
  // views::WindowDelegate
  virtual void DeleteDelegate() { delete this; }
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return message_box_view_; }

 private:
  ResetDefaultsConfirmBox(HWND parent_hwnd, AdvancedPageView* page_view)
      : advanced_page_view_(page_view) {
    int dialog_width = views::Window::GetLocalizedContentsWidth(
        IDS_OPTIONS_RESET_CONFIRM_BOX_WIDTH_CHARS);
    // Also deleted when the window closes.
    message_box_view_ = new MessageBoxView(
        ui::MessageBoxFlags::kFlagHasMessage |
            ui::MessageBoxFlags::kFlagHasOKButton,
        UTF16ToWide(
            l10n_util::GetStringUTF16(IDS_OPTIONS_RESET_MESSAGE)).c_str(),
        std::wstring(),
        dialog_width);
    views::Window::CreateChromeWindow(parent_hwnd, gfx::Rect(), this)->Show();
  }
  virtual ~ResetDefaultsConfirmBox() { }

  MessageBoxView* message_box_view_;
  AdvancedPageView* advanced_page_view_;

  DISALLOW_COPY_AND_ASSIGN(ResetDefaultsConfirmBox);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// AdvancedPageView, public:

AdvancedPageView::AdvancedPageView(Profile* profile)
    : advanced_scroll_view_(NULL),
      reset_to_default_button_(NULL),
      OptionsPageView(profile) {
}

AdvancedPageView::~AdvancedPageView() {
}

void AdvancedPageView::ResetToDefaults() {
  OptionsUtil::ResetToDefaults(profile());
}

///////////////////////////////////////////////////////////////////////////////
// AdvancedPageView, views::ButtonListener implementation:

void AdvancedPageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == reset_to_default_button_) {
     UserMetricsRecordAction(UserMetricsAction("Options_ResetToDefaults"),
                             NULL);
     ResetDefaultsConfirmBox::ShowConfirmBox(
         GetWindow()->GetNativeWindow(), this);
  }
}

///////////////////////////////////////////////////////////////////////////////
// AdvancedPageView, OptionsPageView implementation:

void AdvancedPageView::InitControlLayout() {
  reset_to_default_button_ = new views::NativeButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_RESET)));
  advanced_scroll_view_ = new AdvancedScrollViewContainer(profile());

  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(
      new ManagedPrefsBannerView(profile()->GetPrefs(), OPTIONS_PAGE_ADVANCED));

  layout->StartRow(1, single_column_view_set_id);
  layout->AddView(advanced_scroll_view_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(reset_to_default_button_, 1, 1,
                  GridLayout::TRAILING, GridLayout::CENTER);
}
