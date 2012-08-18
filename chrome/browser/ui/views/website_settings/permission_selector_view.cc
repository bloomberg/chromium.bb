// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/permission_selector_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Left icon margin.
const int kPermissionIconMarginLeft = 6;
// The width of the column that contains the permissions icons.
const int kPermissionIconColumnWidth = 20;

// An array with |ContentSetting|s ordered by CommandID. The array is used to
// lookup a content setting for a given command id.
const ContentSetting kSettingsForCommandIDs[] = {
  CONTENT_SETTING_DEFAULT,  // COMMAND_SET_TO_DEFAULT
  CONTENT_SETTING_ALLOW,    // COMMAND_SET_TO_ALLOW
  CONTENT_SETTING_BLOCK,    // COMMAND_SET_TO_BLOCK
};

}  // namespace

namespace internal {

class PermissionMenuModel : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  PermissionMenuModel(ContentSettingsType site_permission,
                      ContentSetting default_setting,
                      ContentSetting current_setting,
                      PermissionSelectorView* selector);

  ContentSettingsType site_permission() const { return site_permission_; }
  ContentSetting default_setting() const { return default_setting_; }
  ContentSetting current_setting() const { return current_setting_; }

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  enum CommandID {
    COMMAND_SET_TO_DEFAULT,
    COMMAND_SET_TO_ALLOW,
    COMMAND_SET_TO_BLOCK,
  };

  // The site permission (the |ContentSettingsType|) for which the menu model
  // provides settings.
  ContentSettingsType site_permission_;

  // The global default setting for the |site_permission_|.
  ContentSetting default_setting_;

  // The currently active setting for the |site_permission|.
  ContentSetting current_setting_;

  // The |permission_selector_|s SelectionChanged method is called whenever the
  // |current_setting_| is changed.
  PermissionSelectorView* permission_selector_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(PermissionMenuModel);
};

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
  PermissionMenuButton(const string16& text,
                       PermissionMenuModel* model,
                       bool show_menu_marker);
  virtual ~PermissionMenuButton();

  // Overridden from views::MenuButton.
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::TextButton.
  virtual void SetText(const string16& text) OVERRIDE;

 private:
  // Overridden from views::MenuButtonListener.
  virtual void OnMenuButtonClicked(View* source,
                                   const gfx::Point& point) OVERRIDE;

  PermissionMenuModel* menu_model_;  // Owned by |PermissionSelectorView|.
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(PermissionMenuButton);
};

///////////////////////////////////////////////////////////////////////////////
// PermissionMenuModel
///////////////////////////////////////////////////////////////////////////////

PermissionMenuModel::PermissionMenuModel(
    ContentSettingsType site_permission,
    ContentSetting default_setting,
    ContentSetting current_setting,
    PermissionSelectorView* parent)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      site_permission_(site_permission),
      default_setting_(default_setting),
      current_setting_(current_setting),
      permission_selector_(parent) {
  string16 label;
  switch (default_setting_) {
    case CONTENT_SETTING_ALLOW:
      label = l10n_util::GetStringUTF16(
          IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_ALLOW);
      break;
    case CONTENT_SETTING_BLOCK:
      label = l10n_util::GetStringUTF16(
          IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_BLOCK);
      break;
    case CONTENT_SETTING_ASK:
      label = l10n_util::GetStringUTF16(
          IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_ASK);
      break;
    default:
      break;
  }
  AddCheckItem(COMMAND_SET_TO_DEFAULT, label);

  label = l10n_util::GetStringUTF16(
      IDS_WEBSITE_SETTINGS_MENU_ITEM_ALLOW);
  AddCheckItem(COMMAND_SET_TO_ALLOW, label);

  if (site_permission != CONTENT_SETTINGS_TYPE_FULLSCREEN) {
    label = l10n_util::GetStringUTF16(
        IDS_WEBSITE_SETTINGS_MENU_ITEM_BLOCK);
    AddCheckItem(COMMAND_SET_TO_BLOCK, label);
  }
}

bool PermissionMenuModel::IsCommandIdChecked(int command_id) const {
  return current_setting_ == kSettingsForCommandIDs[command_id];
}

bool PermissionMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool PermissionMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  // Accelerators are not supported.
  return false;
}

void PermissionMenuModel::ExecuteCommand(int command_id) {
  current_setting_ = kSettingsForCommandIDs[command_id];
  permission_selector_->SelectionChanged();
}

///////////////////////////////////////////////////////////////////////////////
// PermissionMenuButton
///////////////////////////////////////////////////////////////////////////////

PermissionMenuButton::PermissionMenuButton(const string16& text,
                                           PermissionMenuModel* model,
                                           bool show_menu_marker)
    : ALLOW_THIS_IN_INITIALIZER_LIST(MenuButton(NULL, text, this,
                                                show_menu_marker)),
      menu_model_(model) {
}

PermissionMenuButton::~PermissionMenuButton() {
}

gfx::Size PermissionMenuButton::GetPreferredSize() {
  gfx::Insets insets = GetInsets();
  // Scale the button to the current text size.
  gfx::Size prefsize(text_size_.width() + insets.width(),
                     text_size_.height() + insets.height());
  if (max_width_ > 0)
    prefsize.set_width(std::min(max_width_, prefsize.width()));
  if (show_menu_marker()) {
    prefsize.Enlarge(menu_marker()->width() +
                         views::MenuButton::kMenuMarkerPaddingLeft +
                         views::MenuButton::kMenuMarkerPaddingRight,
                     0);
  }
  return prefsize;
}

void PermissionMenuButton::SetText(const string16& text) {
  MenuButton::SetText(text);
  SizeToPreferredSize();
}

void PermissionMenuButton::OnMenuButtonClicked(View* source,
                                               const gfx::Point& point) {
  views::MenuModelAdapter menu_model_adapter(menu_model_);
  menu_runner_.reset(new views::MenuRunner(menu_model_adapter.CreateMenu()));

  gfx::Point p(point);
  p.Offset(-source->width(), 0);
  if (menu_runner_->RunMenuAt(
          source->GetWidget()->GetTopLevelWidget(),
          this,
          gfx::Rect(p, gfx::Size()),
          views::MenuItemView::TOPLEFT,
          views::MenuRunner::HAS_MNEMONICS) == views::MenuRunner::MENU_DELETED)
    return;
}

}  // namespace internal

///////////////////////////////////////////////////////////////////////////////
// PermissionSelectorView
///////////////////////////////////////////////////////////////////////////////

PermissionSelectorView::PermissionSelectorView(
    ContentSettingsType type,
    ContentSetting default_setting,
    ContentSetting current_setting,
    content_settings::SettingSource source)
    : icon_(NULL),
      menu_button_(NULL) {
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
  ContentSetting setting = current_setting;
  if (setting == CONTENT_SETTING_DEFAULT)
    setting = default_setting;
  const gfx::Image& image = WebsiteSettingsUI::GetPermissionIcon(type, setting);
  icon_->SetImage(image.ToImageSkia());
  layout->AddView(icon_,
                  1,
                  1,
                  views::GridLayout::CENTER,
                  views::GridLayout::CENTER);
  // Create the label that displays the permission type.
  views::Label* label = new views::Label(
      l10n_util::GetStringFUTF16(
          IDS_WEBSITE_SETTINGS_PERMISSION_TYPE,
          WebsiteSettingsUI::PermissionTypeToUIString(type)));
  layout->AddView(label,
                  1,
                  1,
                  views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  // Create the permission menu button.
  menu_button_model_.reset(new internal::PermissionMenuModel(
      type, default_setting, current_setting, this));
  bool button_enabled = source == content_settings::SETTING_SOURCE_USER;
  menu_button_ = new internal::PermissionMenuButton(
      WebsiteSettingsUI::PermissionActionToUIString(current_setting,
                                                    default_setting,
                                                    source),
      menu_button_model_.get(),
      button_enabled);
  menu_button_->SetEnabled(button_enabled);
  layout->AddView(menu_button_);
}

void PermissionSelectorView::AddObserver(
    PermissionSelectorViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionSelectorView::SelectionChanged() {
  // Update the icon to reflect the new setting.
  ContentSetting effective_setting = menu_button_model_->current_setting();
  if (effective_setting == CONTENT_SETTING_DEFAULT)
    effective_setting = menu_button_model_->default_setting();
  const gfx::Image& image = WebsiteSettingsUI::GetPermissionIcon(
      menu_button_model_->site_permission(), effective_setting);
  icon_->SetImage(image.ToImageSkia());

  // Update the menu button text to reflect the new setting.
  menu_button_->SetText(WebsiteSettingsUI::PermissionActionToUIString(
      menu_button_model_->current_setting(),
      menu_button_model_->default_setting(),
      content_settings::SETTING_SOURCE_USER));

  FOR_EACH_OBSERVER(PermissionSelectorViewObserver,
                    observer_list_,
                    OnPermissionChanged(this));
}

ContentSetting PermissionSelectorView::GetSelectedSetting() const {
  return menu_button_model_->current_setting();
}

ContentSettingsType PermissionSelectorView::GetPermissionType() const {
  return menu_button_model_->site_permission();
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
