// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/permission_selector_row.h"

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "chrome/browser/ui/page_info/permission_menu_model.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/page_info/non_accessible_image_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/combobox_model.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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
  ~PermissionMenuButton() override;

  // Overridden from views::View.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

 private:
  // Overridden from views::MenuButtonListener.
  void OnMenuButtonClicked(views::MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override;

  PermissionMenuModel* menu_model_;  // Owned by |PermissionSelectorRow|.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  bool is_rtl_display_;

  DISALLOW_COPY_AND_ASSIGN(PermissionMenuButton);
};

///////////////////////////////////////////////////////////////////////////////
// PermissionMenuButton
///////////////////////////////////////////////////////////////////////////////

PermissionMenuButton::PermissionMenuButton(const base::string16& text,
                                           PermissionMenuModel* model,
                                           bool show_menu_marker)
    : MenuButton(text, this, show_menu_marker), menu_model_(model) {
  // Since PermissionMenuButtons are added to a GridLayout, they are not always
  // sized to their preferred size. Disclosure arrows are always right-aligned,
  // so if the text is not right-aligned, awkward space appears between the text
  // and the arrow.
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  // Update the themed border before the NativeTheme is applied. Usually this
  // happens in a call to LabelButton::OnNativeThemeChanged(). However, if
  // PermissionMenuButton called that from its override, the NativeTheme would
  // be available, and the button would get native GTK styling on Linux.
  UpdateThemedBorder();

  SetFocusForPlatform();
  set_request_focus_on_press(true);
  is_rtl_display_ =
      base::i18n::RIGHT_TO_LEFT == base::i18n::GetStringDirection(text);
}

PermissionMenuButton::~PermissionMenuButton() {}

void PermissionMenuButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  MenuButton::GetAccessibleNodeData(node_data);
  node_data->SetValue(GetText());
}

void PermissionMenuButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  SetTextColor(
      views::Button::STATE_NORMAL,
      theme->GetSystemColor(ui::NativeTheme::kColorId_LabelEnabledColor));
  SetTextColor(
      views::Button::STATE_HOVERED,
      theme->GetSystemColor(ui::NativeTheme::kColorId_LabelEnabledColor));
  SetTextColor(
      views::Button::STATE_DISABLED,
      theme->GetSystemColor(ui::NativeTheme::kColorId_LabelDisabledColor));
}

void PermissionMenuButton::OnMenuButtonClicked(views::MenuButton* source,
                                               const gfx::Point& point,
                                               const ui::Event* event) {
  menu_runner_.reset(
      new views::MenuRunner(menu_model_, views::MenuRunner::HAS_MNEMONICS));

  gfx::Point p(point);
  p.Offset(is_rtl_display_ ? source->width() : -source->width(), 0);
  menu_runner_->RunMenuAt(source->GetWidget()->GetTopLevelWidget(), this,
                          gfx::Rect(p, gfx::Size()), views::MENU_ANCHOR_TOPLEFT,
                          ui::MENU_SOURCE_NONE);
}

// This class adapts a |PermissionMenuModel| into a |ui::ComboboxModel| so that
// |PermissionCombobox| can use it.
class ComboboxModelAdapter : public ui::ComboboxModel {
 public:
  explicit ComboboxModelAdapter(PermissionMenuModel* model) : model_(model) {}
  ~ComboboxModelAdapter() override {}

  void OnPerformAction(int index);

  // Returns the checked index of the underlying PermissionMenuModel, of which
  // there must be exactly one. This is used to choose which index is selected
  // in the PermissionCombobox.
  int GetCheckedIndex();

  // ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;

 private:
  PermissionMenuModel* model_;
};

void ComboboxModelAdapter::OnPerformAction(int index) {
  model_->ExecuteCommand(index, 0);
}

int ComboboxModelAdapter::GetCheckedIndex() {
  int checked_index = -1;
  for (int i = 0; i < model_->GetItemCount(); ++i) {
    if (model_->IsCommandIdChecked(i)) {
      // This function keeps track of |checked_index| instead of returning early
      // here so that it can DCHECK that there's exactly one selected item,
      // which is not normally guaranteed by MenuModel, but *is* true of
      // PermissionMenuModel.
      DCHECK_EQ(checked_index, -1);
      checked_index = i;
    }
  }
  return checked_index;
}

int ComboboxModelAdapter::GetItemCount() const {
  DCHECK(model_);
  return model_->GetItemCount();
}

base::string16 ComboboxModelAdapter::GetItemAt(int index) {
  return model_->GetLabelAt(index);
}

// The |PermissionCombobox| provides a combobox for selecting a permission type.
// This is only used on platforms where the permission dialog uses a combobox
// instead of a MenuButton (currently, Mac).
class PermissionCombobox : public views::Combobox,
                           public views::ComboboxListener {
 public:
  PermissionCombobox(ComboboxModelAdapter* model,
                     bool enabled,
                     bool use_default);
  ~PermissionCombobox() override;

  void UpdateSelectedIndex(bool use_default);

 private:
  // views::Combobox:
  void OnPaintBorder(gfx::Canvas* canvas) override;

  // views::ComboboxListener:
  void OnPerformAction(Combobox* combobox) override;

  ComboboxModelAdapter* model_;
};

PermissionCombobox::PermissionCombobox(ComboboxModelAdapter* model,
                                       bool enabled,
                                       bool use_default)
    : views::Combobox(model), model_(model) {
  set_listener(this);
  SetEnabled(enabled);
  UpdateSelectedIndex(use_default);
  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    set_size_to_largest_label(false);
    ModelChanged();
  }
}

PermissionCombobox::~PermissionCombobox() {}

void PermissionCombobox::UpdateSelectedIndex(bool use_default) {
  int index = model_->GetCheckedIndex();
  if (use_default && index == -1) {
    index = 0;
  }
  SetSelectedIndex(index);
}

void PermissionCombobox::OnPaintBorder(gfx::Canvas* canvas) {
  // No border except a focus indicator for MD mode.
  if (ui::MaterialDesignController::IsSecondaryUiMaterial() && !HasFocus()) {
    return;
  }
  Combobox::OnPaintBorder(canvas);
}

void PermissionCombobox::OnPerformAction(Combobox* combobox) {
  model_->OnPerformAction(combobox->selected_index());
}

}  // namespace internal

///////////////////////////////////////////////////////////////////////////////
// PermissionSelectorRow
///////////////////////////////////////////////////////////////////////////////

PermissionSelectorRow::PermissionSelectorRow(
    Profile* profile,
    const GURL& url,
    const PageInfoUI::PermissionInfo& permission,
    views::GridLayout* layout)
    : profile_(profile), icon_(NULL), menu_button_(NULL), combobox_(NULL) {
  // Create the permission icon.
  icon_ = new NonAccessibleImageView();
  const gfx::Image& image = PageInfoUI::GetPermissionIcon(permission);
  icon_->SetImage(image.ToImageSkia());
  layout->AddView(icon_, 1, 1, views::GridLayout::CENTER,
                  views::GridLayout::CENTER);
  // Create the label that displays the permission type.
  label_ =
      new views::Label(PageInfoUI::PermissionTypeToUIString(permission.type),
                       CONTEXT_BODY_TEXT_LARGE);
  layout->AddView(label_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  // Create the menu model.
  menu_model_.reset(new PermissionMenuModel(
      profile, url, permission,
      base::Bind(&PermissionSelectorRow::PermissionChanged,
                 base::Unretained(this))));

// Create the permission menu button.
#if defined(OS_MACOSX)
  bool use_real_combobox = true;
#else
  bool use_real_combobox =
      ui::MaterialDesignController::IsSecondaryUiMaterial();
#endif
  if (use_real_combobox) {
    InitializeComboboxView(layout, permission);
  } else {
    InitializeMenuButtonView(layout, permission);
  }

  // Show the permission decision reason, if it was not the user.
  base::string16 reason =
      PageInfoUI::PermissionDecisionReasonToUIString(profile, permission, url);
  if (!reason.empty()) {
    layout->StartRow(1, 1);
    layout->SkipColumns(1);
    views::Label* permission_decision_reason = new views::Label(reason);
    permission_decision_reason->SetEnabledColor(
        PageInfoUI::GetPermissionDecisionTextColor());
    // Long labels should span the remaining width of the row.
    views::ColumnSet* column_set = layout->GetColumnSet(1);
    DCHECK(column_set);
    layout->AddView(permission_decision_reason, column_set->num_columns() - 2,
                    1, views::GridLayout::LEADING, views::GridLayout::CENTER);
  }
}

void PermissionSelectorRow::AddObserver(
    PermissionSelectorRowObserver* observer) {
  observer_list_.AddObserver(observer);
}

PermissionSelectorRow::~PermissionSelectorRow() {
  // Gross. On paper the Combobox and the ComboboxModelAdapter are both owned by
  // this class, but actually, the Combobox is owned by View and will be
  // destroyed in ~View(), which runs *after* ~PermissionSelectorRow() is done,
  // which means the Combobox gets destroyed after its ComboboxModel, which
  // causes an explosion when the Combobox attempts to stop observing the
  // ComboboxModel. This hack ensures the Combobox is deleted before its
  // ComboboxModel.
  //
  // Technically, the MenuButton has the same problem, but MenuButton doesn't
  // use its model in its destructor.
  if (combobox_) {
    combobox_->parent()->RemoveChildView(combobox_);
  }
}

void PermissionSelectorRow::InitializeMenuButtonView(
    views::GridLayout* layout,
    const PageInfoUI::PermissionInfo& permission) {
  bool button_enabled =
      permission.source == content_settings::SETTING_SOURCE_USER;
  menu_button_ = new internal::PermissionMenuButton(
      PageInfoUI::PermissionActionToUIString(
          profile_, permission.type, permission.setting,
          permission.default_setting, permission.source),
      menu_model_.get(), button_enabled);
  menu_button_->SetEnabled(button_enabled);
  menu_button_->SetAccessibleName(
      PageInfoUI::PermissionTypeToUIString(permission.type));
  layout->AddView(menu_button_);
}

void PermissionSelectorRow::InitializeComboboxView(
    views::GridLayout* layout,
    const PageInfoUI::PermissionInfo& permission) {
  bool button_enabled =
      permission.source == content_settings::SETTING_SOURCE_USER;
  combobox_model_adapter_.reset(
      new internal::ComboboxModelAdapter(menu_model_.get()));
  combobox_ = new internal::PermissionCombobox(combobox_model_adapter_.get(),
                                               button_enabled, true);
  combobox_->SetEnabled(button_enabled);
  combobox_->SetAccessibleName(
      PageInfoUI::PermissionTypeToUIString(permission.type));
  layout->AddView(combobox_);
}

void PermissionSelectorRow::PermissionChanged(
    const PageInfoUI::PermissionInfo& permission) {
  // Change the permission icon to reflect the selected setting.
  const gfx::Image& image = PageInfoUI::GetPermissionIcon(permission);
  icon_->SetImage(image.ToImageSkia());

  // Update the menu button text to reflect the new setting.
  if (menu_button_) {
    menu_button_->SetText(PageInfoUI::PermissionActionToUIString(
        profile_, permission.type, permission.setting,
        permission.default_setting, content_settings::SETTING_SOURCE_USER));
    menu_button_->SizeToPreferredSize();
    // Re-layout will be done at the |PageInfoBubbleView| level, since
    // that view may need to resize itself to accomodate the new sizes of its
    // contents.
    menu_button_->InvalidateLayout();
  } else if (combobox_) {
    bool use_default = permission.setting == CONTENT_SETTING_DEFAULT;
    combobox_->UpdateSelectedIndex(use_default);
  }

  for (PermissionSelectorRowObserver& observer : observer_list_) {
    observer.OnPermissionChanged(permission);
  }
}

views::View* PermissionSelectorRow::button() {
  // These casts are required because the two arms of a ?: cannot have different
  // types T1 and T2, even if the resulting value of the ?: is about to be a T
  // and T1 and T2 are both subtypes of T.
  return menu_button_ ? static_cast<views::View*>(menu_button_)
                      : static_cast<views::View*>(combobox_);
}
