// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_tab.h"

#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/views/bounded_scroll_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

// A scrollable list of permissions for the given app.
class PermissionsScrollView : public message_center::BoundedScrollView {
 public:
  PermissionsScrollView(int min_height,
                        int max_height,
                        const extensions::Extension* app);

 private:
  virtual ~PermissionsScrollView();
};

PermissionsScrollView::PermissionsScrollView(int min_height,
                                             int max_height,
                                             const extensions::Extension* app)
    : message_center::BoundedScrollView(min_height, max_height) {
  views::View* inner_scrollable_view = new views::View();
  this->SetContents(inner_scrollable_view);

  // Get the permission messages for the app.
  std::vector<base::string16> permission_messages =
      extensions::PermissionMessageProvider::Get()->GetWarningMessages(
          app->GetActivePermissions(), app->GetType());

  // Create the layout.
  views::GridLayout* layout =
      views::GridLayout::CreatePanel(inner_scrollable_view);
  inner_scrollable_view->SetLayoutManager(layout);

  // Create 2 columns: one for the bullet, one for the bullet text.
  static const int kPermissionBulletsColumnSetId = 1;
  views::ColumnSet* permission_bullets_column_set =
      layout->AddColumnSet(kPermissionBulletsColumnSetId);
  permission_bullets_column_set->AddPaddingColumn(0, 10);
  permission_bullets_column_set->AddColumn(views::GridLayout::LEADING,
                                           views::GridLayout::LEADING,
                                           0,
                                           views::GridLayout::USE_PREF,
                                           0,  // no fixed width
                                           0);
  permission_bullets_column_set->AddPaddingColumn(0, 5);
  permission_bullets_column_set->AddColumn(views::GridLayout::LEADING,
                                           views::GridLayout::LEADING,
                                           0,
                                           views::GridLayout::USE_PREF,
                                           0,  // no fixed width
                                           0);

  // Add permissions to scrollable view.
  for (std::vector<base::string16>::const_iterator it =
           permission_messages.begin();
       it != permission_messages.end();
       ++it) {
    views::Label* permission_label = new views::Label(*it);

    permission_label->SetMultiLine(true);
    permission_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    permission_label->SizeToFit(250);

    layout->StartRow(0, kPermissionBulletsColumnSetId);
    // Extract only the bullet from the IDS_EXTENSION_PERMISSION_LINE text.
    layout->AddView(new views::Label(l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PERMISSION_LINE, base::string16())));
    // Place the text second, so multi-lined permissions line up below the
    // bullet.
    layout->AddView(permission_label);

    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }
}

PermissionsScrollView::~PermissionsScrollView() {}

AppInfoPermissionsTab::AppInfoPermissionsTab(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const extensions::Extension* app,
    const base::Closure& close_callback)
    : AppInfoTab(parent_window, profile, app, close_callback) {

  // Create the layout.
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  static const int kPermissionsColumnSetId = 0;
  views::ColumnSet* permissions_column_set =
      layout->AddColumnSet(kPermissionsColumnSetId);
  permissions_column_set->AddColumn(views::GridLayout::LEADING,
                                    views::GridLayout::LEADING,
                                    0,
                                    views::GridLayout::USE_PREF,
                                    0,  // no fixed width
                                    0);

  views::Label* required_permissions_heading = new views::Label(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_CAN_ACCESS));
  required_permissions_heading->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  layout->StartRow(0, kPermissionsColumnSetId);
  layout->AddView(required_permissions_heading);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  layout->StartRow(0, kPermissionsColumnSetId);
  layout->AddView(new PermissionsScrollView(0, 100, app));
  layout->AddPaddingRow(0, views::kUnrelatedControlHorizontalSpacing);
}

AppInfoPermissionsTab::~AppInfoPermissionsTab() {}
