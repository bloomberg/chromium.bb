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
// Left margin of the label that displays the permission types.
const int kPermissionsRowLabelMarginLeft = 8;

string16 GetPermissionTypeLabel(ContentSettingsType type) {
  return l10n_util::GetStringUTF16(
      WebsiteSettingsUI::PermissionTypeToUIStringID(type)) + ASCIIToUTF16(":");
}

string16 PermissionValueToString(ContentSetting value) {
  return l10n_util::GetStringUTF16(
      WebsiteSettingsUI::PermissionValueToUIStringID(value));
}

string16 GetPermissionActionDisplayString(ContentSetting value) {
  return l10n_util::GetStringUTF16(
      WebsiteSettingsUI::PermissionActionUIStringID(value));
}

string16 GetPermissionDisplayString(ContentSetting setting,
                                    ContentSetting default_setting) {
  if (setting == CONTENT_SETTING_DEFAULT) {
    return l10n_util::GetStringFUTF16(
        IDS_WEBSITE_SETTINGS_DEFAULT_SETTING,
        GetPermissionActionDisplayString(default_setting));
  }
  return l10n_util::GetStringFUTF16(IDS_WEBSITE_SETTINGS_USER_SETTING,
                                    GetPermissionActionDisplayString(setting));
}

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
  // |PermissionMenuButton|.
  PermissionMenuButton(const string16& text,
                       PermissionMenuModel* model);
  virtual ~PermissionMenuButton();

 private:
  // Overridden from views::MenuButtonListener:
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
  AddCheckItem(COMMAND_SET_TO_DEFAULT,
               l10n_util::GetStringFUTF16(
                   IDS_WEBSITE_SETTINGS_DEFAULT_PERMISSION_LABEL,
                   PermissionValueToString(default_setting_)));
  AddCheckItem(COMMAND_SET_TO_ALLOW,
               l10n_util::GetStringFUTF16(
                   IDS_WEBSITE_SETTINGS_PERMISSION_LABEL,
                   PermissionValueToString(CONTENT_SETTING_ALLOW)));
  if (site_permission != CONTENT_SETTINGS_TYPE_FULLSCREEN) {
    AddCheckItem(COMMAND_SET_TO_BLOCK,
                 l10n_util::GetStringFUTF16(
                     IDS_WEBSITE_SETTINGS_PERMISSION_LABEL,
                     PermissionValueToString(CONTENT_SETTING_BLOCK)));
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
// PermissionMenuModel
///////////////////////////////////////////////////////////////////////////////

PermissionMenuButton::PermissionMenuButton(const string16& text,
                                           PermissionMenuModel* model)
    : ALLOW_THIS_IN_INITIALIZER_LIST(MenuButton(NULL, text, this, true)),
      menu_model_(model) {
}

PermissionMenuButton::~PermissionMenuButton() {
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

PermissionSelectorView::PermissionSelectorView(ContentSettingsType type,
                                               ContentSetting default_setting,
                                               ContentSetting current_setting)
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
  column_set->AddPaddingColumn(0, kPermissionsRowLabelMarginLeft);
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
  views::Label* label = new views::Label(GetPermissionTypeLabel(type));
  layout->AddView(label,
                  1,
                  1,
                  views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  // Create the permission menu button.
  menu_button_model_.reset(new internal::PermissionMenuModel(
      type, default_setting, current_setting, this));
  menu_button_ = new internal::PermissionMenuButton(
      GetPermissionDisplayString(current_setting, default_setting),
      menu_button_model_.get());
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
  menu_button_->SetText(GetPermissionDisplayString(
        menu_button_model_->current_setting(),
        menu_button_model_->default_setting()));

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

PermissionSelectorView::~PermissionSelectorView() {
}
