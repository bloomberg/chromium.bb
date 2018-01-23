// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/browser/ui/views/passwords/credentials_selection_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_items_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_pending_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_sign_in_promo_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/desktop_ios_promotion/desktop_ios_promotion_bubble_view.h"
#endif

// Construct an appropriate ColumnSet for the given |type|, and add it
// to |layout|.
void ManagePasswordsBubbleView::BuildColumnSet(views::GridLayout* layout,
                                               ColumnSetType type) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  int full_width = ManagePasswordsBubbleView::kDesiredBubbleWidth;
  const int button_divider = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  const int column_divider = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  switch (type) {
    case SINGLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            0,
                            views::GridLayout::FIXED,
                            full_width,
                            0);
      break;
    case DOUBLE_VIEW_COLUMN_SET_USERNAME:
    case DOUBLE_VIEW_COLUMN_SET_PASSWORD:
      column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                            0, views::GridLayout::USE_PREF, 0, 0);
      column_set->AddPaddingColumn(0, column_divider);
      column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                            views::GridLayout::USE_PREF, 0, 0);
      break;
    case TRIPLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                            0, views::GridLayout::USE_PREF, 0, 0);
      column_set->AddPaddingColumn(0, column_divider);
      column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                            views::GridLayout::USE_PREF, 0, 0);
      column_set->AddPaddingColumn(0, column_divider);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::FILL, 0,
                            views::GridLayout::USE_PREF, 0, 0);
      break;
    case DOUBLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, button_divider);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case LINK_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::LEADING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, button_divider);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case SINGLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case TRIPLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::LEADING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, button_divider);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, button_divider);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
  }
}

// Builds a credential row, adds the given elements to the layout.
// |password_view_button| is an optional field. If it is a nullptr, a
// DOUBLE_VIEW_COLUMN_SET_PASSWORD will be used for password row instead of
// TRIPLE_VIEW_COLUMN_SET.
// TODO(pbos): Merge into PendingView when UpdatePendingView and PendingView
// dialogs merge.
void ManagePasswordsBubbleView::BuildCredentialRows(
    views::GridLayout* layout,
    views::View* username_field,
    views::View* password_field,
    views::ToggleImageButton* password_view_button,
    bool show_password_label) {
  // Username row.
  BuildColumnSet(layout, DOUBLE_VIEW_COLUMN_SET_USERNAME);
  layout->StartRow(0, DOUBLE_VIEW_COLUMN_SET_USERNAME);
  std::unique_ptr<views::Label> username_label(new views::Label(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USERNAME_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  username_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT);
  std::unique_ptr<views::Label> password_label(new views::Label(
      show_password_label
          ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSWORD_LABEL)
          : base::string16(),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  password_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT);
  int labels_width = std::max(username_label->GetPreferredSize().width(),
                              password_label->GetPreferredSize().width());

  layout->AddView(username_label.release(), 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::FILL, labels_width, 0);
  layout->AddView(username_field, 1, 1, views::GridLayout::FILL,
                  views::GridLayout::FILL, 0, 0);

  layout->AddPaddingRow(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_CONTROL_LIST_VERTICAL));

  // Password row.
  ColumnSetType type = password_view_button ? TRIPLE_VIEW_COLUMN_SET
                                            : DOUBLE_VIEW_COLUMN_SET_PASSWORD;
  BuildColumnSet(layout, type);
  layout->StartRow(0, type);
  layout->AddView(password_label.release(), 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::FILL, labels_width, 0);
  layout->AddView(password_field, 1, 1, views::GridLayout::FILL,
                  views::GridLayout::FILL, 0, 0);
  // The eye icon is also added to the layout if it was passed.
  if (password_view_button) {
    layout->AddView(password_view_button);
  }
}

// ManagePasswordsBubbleView --------------------------------------------------
ManagePasswordsBubbleView::ManagePasswordsBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    DisplayReason reason)
    : ManagePasswordsBubbleDelegateViewBase(web_contents,
                                            anchor_view,
                                            anchor_point,
                                            reason),
      initially_focused_view_(nullptr) {
  set_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::MANAGE_PASSWORDS);
}

ManagePasswordsBubbleView::~ManagePasswordsBubbleView() = default;

bool ManagePasswordsBubbleView::ShouldSnapFrameWidth() const {
  return ChromeLayoutProvider::Get()->IsHarmonyMode();
}

int ManagePasswordsBubbleView::GetDialogButtons() const {
  // TODO(tapted): DialogClientView should manage buttons.
  return ui::DIALOG_BUTTON_NONE;
}

views::View* ManagePasswordsBubbleView::GetInitiallyFocusedView() {
  return initially_focused_view_;
}

void ManagePasswordsBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  CreateChild();
}

void ManagePasswordsBubbleView::AddedToWidget() {
  auto title_view =
      std::make_unique<views::StyledLabel>(base::string16(), this);
  title_view->SetTextContext(views::style::CONTEXT_DIALOG_TITLE);
  UpdateTitleText(title_view.get());
  GetBubbleFrameView()->SetTitleView(std::move(title_view));
}

void ManagePasswordsBubbleView::UpdateTitleText(
    views::StyledLabel* title_view) {
  title_view->SetText(GetWindowTitle());
  if (!model()->title_brand_link_range().is_empty()) {
    auto link_style = views::StyledLabel::RangeStyleInfo::CreateForLink();
    link_style.disable_line_wrapping = false;
    title_view->AddStyleRange(model()->title_brand_link_range(), link_style);
  }
}

gfx::ImageSkia ManagePasswordsBubbleView::GetWindowIcon() {
#if defined(OS_WIN)
  if (model()->state() ==
      password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE) {
    return desktop_ios_promotion::GetPromoImage(
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_TextfieldDefaultColor));
  }
#endif
  return gfx::ImageSkia();
}

bool ManagePasswordsBubbleView::ShouldShowWindowIcon() const {
  return model()->state() ==
         password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE;
}

bool ManagePasswordsBubbleView::ShouldShowCloseButton() const {
  return model()->state() == password_manager::ui::PENDING_PASSWORD_STATE ||
         model()->state() == password_manager::ui::CHROME_SIGN_IN_PROMO_STATE ||
         model()->state() ==
             password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE;
}

void ManagePasswordsBubbleView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  DCHECK_EQ(model()->title_brand_link_range(), range);
  model()->OnBrandLinkClicked();
}

void ManagePasswordsBubbleView::CreateChild() {
  // TODO(pbos): This file is being removed (and static code moved). It was only
  // kept to make the diff easier for the last CL. This should now be unused.
  NOTREACHED();
}
