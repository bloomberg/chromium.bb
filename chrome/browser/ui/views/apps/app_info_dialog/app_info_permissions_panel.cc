// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_panel.h"

#include <string>
#include <vector>

#include "apps/app_load_service.h"
#include "apps/saved_files_service.h"
#include "base/files/file_path.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

AppInfoPermissionsPanel::AppInfoPermissionsPanel(
    Profile* profile,
    const extensions::Extension* app)
    : AppInfoPanel(profile, app),
      active_permissions_heading_(NULL),
      active_permissions_list_(NULL),
      retained_files_heading_(NULL),
      retained_files_list_(NULL),
      revoke_file_permissions_button_(NULL) {
  // Create UI elements.
  CreateActivePermissionsControl();
  CreateRetainedFilesControl();

  // Layout elements.
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           0,
                           0,
                           views::kUnrelatedControlVerticalSpacing));

  LayoutActivePermissionsControl();
  LayoutRetainedFilesControl();
}

AppInfoPermissionsPanel::~AppInfoPermissionsPanel() {
  // Destroy view children before their models.
  RemoveAllChildViews(true);
}

// Given a list of strings, returns a view containing a list of these strings
// as bulleted items.
views::View* AppInfoPermissionsPanel::CreateBulletedListView(
    const std::vector<base::string16>& messages,
    bool allow_multiline,
    gfx::ElideBehavior elide_behavior) {
  const int kSpacingBetweenBulletAndStartOfText = 5;

  views::View* list_view = new views::View();
  views::GridLayout* layout = new views::GridLayout(list_view);
  list_view->SetLayoutManager(layout);

  // Create 2 columns: one for the bullet, one for the bullet text.
  static const int kColumnSetId = 1;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(0, kSpacingBetweenBulletAndStartOfText);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  for (std::vector<base::string16>::const_iterator it = messages.begin();
       it != messages.end();
       ++it) {
    views::Label* permission_label = new views::Label(*it);
    permission_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    if (allow_multiline)
      permission_label->SetMultiLine(true);
    else
      permission_label->SetElideBehavior(elide_behavior);

    // Extract only the bullet from the IDS_EXTENSION_PERMISSION_LINE text.
    views::Label* bullet_label = new views::Label(l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PERMISSION_LINE, base::string16()));

    // Add a padding row before every item except the first
    if (it != messages.begin())
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    layout->StartRow(1, kColumnSetId);
    layout->AddView(bullet_label);
    layout->AddView(permission_label);
  }

  return list_view;
}

void AppInfoPermissionsPanel::CreateActivePermissionsControl() {
  std::vector<base::string16> permission_strings =
      GetActivePermissionMessages();
  if (permission_strings.empty()) {
    views::Label* no_permissions_text = new views::Label(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_NO_PERMISSIONS_TEXT));
    no_permissions_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    active_permissions_list_ = no_permissions_text;
  } else {
    active_permissions_heading_ = CreateHeading(l10n_util::GetStringUTF16(
        IDS_APPLICATION_INFO_ACTIVE_PERMISSIONS_TEXT));
    active_permissions_list_ =
        CreateBulletedListView(permission_strings, true, gfx::NO_ELIDE);
  }
}

void AppInfoPermissionsPanel::CreateRetainedFilesControl() {
  const std::vector<base::string16> retained_file_permission_messages =
      GetRetainedFilePaths();

  if (!retained_file_permission_messages.empty()) {
    revoke_file_permissions_button_ = new views::LabelButton(
        this,
        l10n_util::GetStringUTF16(
            IDS_APPLICATION_INFO_REVOKE_RETAINED_FILE_PERMISSIONS_BUTTON_TEXT));
    revoke_file_permissions_button_->SetStyle(views::Button::STYLE_BUTTON);

    retained_files_heading_ = CreateHeading(l10n_util::GetStringUTF16(
        IDS_APPLICATION_INFO_RETAINED_FILE_PERMISSIONS_TEXT));
    retained_files_list_ = CreateBulletedListView(
        retained_file_permission_messages, false, gfx::ELIDE_MIDDLE);
  }
}

void AppInfoPermissionsPanel::LayoutActivePermissionsControl() {
  if (active_permissions_list_) {
    views::View* vertical_stack = CreateVerticalStack();
    if (active_permissions_heading_)
      vertical_stack->AddChildView(active_permissions_heading_);
    vertical_stack->AddChildView(active_permissions_list_);

    AddChildView(vertical_stack);
  }
}

void AppInfoPermissionsPanel::LayoutRetainedFilesControl() {
  if (retained_files_list_) {
    DCHECK(retained_files_heading_);
    DCHECK(revoke_file_permissions_button_);

    // Add a sub-view so the revoke button is right-aligned.
    views::View* right_aligned_button = new views::View();
    views::BoxLayout* right_aligned_horizontal_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    right_aligned_horizontal_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
    right_aligned_button->SetLayoutManager(right_aligned_horizontal_layout);
    right_aligned_button->AddChildView(revoke_file_permissions_button_);

    views::View* vertical_stack = CreateVerticalStack();
    vertical_stack->AddChildView(retained_files_heading_);
    vertical_stack->AddChildView(retained_files_list_);
    vertical_stack->AddChildView(right_aligned_button);

    AddChildView(vertical_stack);
  }
}

void AppInfoPermissionsPanel::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  if (sender == revoke_file_permissions_button_) {
    RevokeFilePermissions();
  } else {
    NOTREACHED();
  }
}

const std::vector<base::string16>
AppInfoPermissionsPanel::GetActivePermissionMessages() const {
  return app_->permissions_data()->GetPermissionMessageStrings();
}

const std::vector<base::string16>
AppInfoPermissionsPanel::GetRetainedFilePaths() const {
  std::vector<base::string16> retained_file_paths;
  if (app_->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kFileSystem)) {
    std::vector<apps::SavedFileEntry> retained_file_entries =
        apps::SavedFilesService::Get(profile_)->GetAllFileEntries(app_->id());
    for (std::vector<apps::SavedFileEntry>::const_iterator it =
             retained_file_entries.begin();
         it != retained_file_entries.end();
         ++it) {
      retained_file_paths.push_back(it->path.LossyDisplayName());
    }
  }
  return retained_file_paths;
}

void AppInfoPermissionsPanel::RevokeFilePermissions() {
  apps::SavedFilesService::Get(profile_)->ClearQueue(app_);
  apps::AppLoadService::Get(profile_)->RestartApplicationIfRunning(app_->id());

  GetWidget()->Close();
}
