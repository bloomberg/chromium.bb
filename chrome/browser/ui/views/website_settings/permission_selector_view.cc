// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/permission_selector_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/website_settings/permission_menu_model.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Left icon margin.
const int kPermissionIconMarginLeft = 6;
// The width of the column that contains the permissions icons.
const int kPermissionIconColumnWidth = 20;

}  // namespace

namespace internal {

// The |PermissionMenuButton| provides a menu for selecting a setting a
// permissions type.
class PermissionMenuButton : public views::MenuButton,
                             public views::MenuButtonListener {
 public:
  // Creates a new |PermissionMenuButton| with the passed |text|. The ownership
  // of the |model| remains with the caller and is not transfered to the
  // |PermissionMenuButton|. If the |show_menu_marker| flag is true, then a
  // small icon is be displayed next to the button |text|, indicating that the
  // button opens a drop down menu.
  PermissionMenuButton(const base::string16& text,
                       PermissionMenuModel* model,
                       bool show_menu_marker);
  virtual ~PermissionMenuButton();

  // Overridden from views::LabelButton.
  virtual void SetText(const base::string16& text) OVERRIDE;

  // Overridden from views::View.
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual void OnNativeThemeChanged(const ui::NativeTheme* theme) OVERRIDE;

 private:
  // Overridden from views::MenuButtonListener.
  virtual void OnMenuButtonClicked(View* source,
                                   const gfx::Point& point) OVERRIDE;

  PermissionMenuModel* menu_model_;  // Owned by |PermissionSelectorView|.
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(PermissionMenuButton);
};

///////////////////////////////////////////////////////////////////////////////
// PermissionMenuButton
///////////////////////////////////////////////////////////////////////////////

PermissionMenuButton::PermissionMenuButton(const base::string16& text,
                                           PermissionMenuModel* model,
                                           bool show_menu_marker)
    : MenuButton(NULL, text, this, show_menu_marker),
      menu_model_(model) {
}

PermissionMenuButton::~PermissionMenuButton() {
}

void PermissionMenuButton::SetText(const base::string16& text) {
  MenuButton::SetText(text);
  SizeToPreferredSize();
}

void PermissionMenuButton::GetAccessibleState(ui::AXViewState* state) {
  MenuButton::GetAccessibleState(state);
  state->value = GetText();
}

void PermissionMenuButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  SetTextColor(views::Button::STATE_NORMAL, GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelEnabledColor));
  SetTextColor(views::Button::STATE_HOVERED, GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelEnabledColor));
  SetTextColor(views::Button::STATE_DISABLED, GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelDisabledColor));
}

void PermissionMenuButton::OnMenuButtonClicked(View* source,
                                               const gfx::Point& point) {
  menu_runner_.reset(
      new views::MenuRunner(menu_model_, views::MenuRunner::HAS_MNEMONICS));

  gfx::Point p(point);
  p.Offset(-source->width(), 0);
  if (menu_runner_->RunMenuAt(source->GetWidget()->GetTopLevelWidget(),
                              this,
                              gfx::Rect(p, gfx::Size()),
                              views::MENU_ANCHOR_TOPLEFT,
                              ui::MENU_SOURCE_NONE) ==
      views::MenuRunner::MENU_DELETED) {
    return;
  }
}

}  // namespace internal

///////////////////////////////////////////////////////////////////////////////
// PermissionSelectorView
///////////////////////////////////////////////////////////////////////////////

PermissionSelectorView::PermissionSelectorView(
    const GURL& url,
    const WebsiteSettingsUI::PermissionInfo& permission)
    : icon_(NULL), menu_button_(NULL) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  const int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::FIXED,
                        kPermissionIconColumnWidth,
                        0);
  column_set->AddPaddingColumn(0, kPermissionIconMarginLeft);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  layout->StartRow(1, column_set_id);
  // Create the permission icon.
  icon_ = new views::ImageView();
  const gfx::Image& image = WebsiteSettingsUI::GetPermissionIcon(permission);
  icon_->SetImage(image.ToImageSkia());
  layout->AddView(icon_,
                  1,
                  1,
                  views::GridLayout::CENTER,
                  views::GridLayout::CENTER);
  // Create the label that displays the permission type.
  views::Label* label = new views::Label(l10n_util::GetStringFUTF16(
      IDS_WEBSITE_SETTINGS_PERMISSION_TYPE,
      WebsiteSettingsUI::PermissionTypeToUIString(permission.type)));
  layout->AddView(label,
                  1,
                  1,
                  views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  // Create the menu model.
  menu_model_.reset(new PermissionMenuModel(
      url,
      permission,
      base::Bind(&PermissionSelectorView::PermissionChanged,
                 base::Unretained(this))));
  // Create the permission menu button.
  bool button_enabled =
      permission.source == content_settings::SETTING_SOURCE_USER;
  menu_button_ = new internal::PermissionMenuButton(
      WebsiteSettingsUI::PermissionActionToUIString(
          permission.setting, permission.default_setting, permission.source),
      menu_model_.get(),
      button_enabled);
  menu_button_->SetEnabled(button_enabled);
  menu_button_->SetFocusable(button_enabled);
  menu_button_->SetAccessibleName(
      WebsiteSettingsUI::PermissionTypeToUIString(permission.type));
  layout->AddView(menu_button_);
}

void PermissionSelectorView::AddObserver(
    PermissionSelectorViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionSelectorView::ChildPreferredSizeChanged(View* child) {
  SizeToPreferredSize();
  // FIXME: The parent is only a plain |View| that is used as a
  // container/box/panel. The SizeToPreferredSize method of the parent is
  // called here directly in order not to implement a custom |View| class with
  // its own implementation of the ChildPreferredSizeChanged method.
  parent()->SizeToPreferredSize();
}

PermissionSelectorView::~PermissionSelectorView() {
}

void PermissionSelectorView::PermissionChanged(
    const WebsiteSettingsUI::PermissionInfo& permission) {
  // Change the permission icon to reflect the selected setting.
  const gfx::Image& image = WebsiteSettingsUI::GetPermissionIcon(permission);
  icon_->SetImage(image.ToImageSkia());

  // Update the menu button text to reflect the new setting.
  menu_button_->SetText(WebsiteSettingsUI::PermissionActionToUIString(
      permission.setting,
      permission.default_setting,
      content_settings::SETTING_SOURCE_USER));

  FOR_EACH_OBSERVER(PermissionSelectorViewObserver,
                    observer_list_,
                    OnPermissionChanged(permission));
}
