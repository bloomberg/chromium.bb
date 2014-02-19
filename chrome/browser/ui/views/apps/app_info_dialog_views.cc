// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog_views.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

// Size of extension icon in top left of dialog.
const int kIconSize = 64;

void ShowChromeAppInfoDialog(gfx::NativeWindow parent_window,
                             Profile* profile,
                             const extensions::Extension* app,
                             const base::Closure& close_callback) {
  CreateBrowserModalDialogViews(new AppInfoView(profile, app, close_callback),
                                parent_window)->Show();
}

PermissionsScrollView::PermissionsScrollView(int min_height,
                                             int max_height,
                                             const extensions::Extension* app)
    : message_center::BoundedScrollView(min_height, max_height) {
  inner_scrollable_view = new views::View();
  this->SetContents(inner_scrollable_view);

  // Get the permission messages for the app
  std::vector<base::string16> permission_messages =
      extensions::PermissionMessageProvider::Get()->GetWarningMessages(
          app->GetActivePermissions(), app->GetType());

  // Create the layout
  views::GridLayout* layout =
      views::GridLayout::CreatePanel(inner_scrollable_view);
  inner_scrollable_view->SetLayoutManager(layout);

  // Create 2 columns: one for the bullet, one for the bullet text
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

  // Add permissions to scrollable view
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

AppInfoView::AppInfoView(Profile* profile,
                         const extensions::Extension* app,
                         const base::Closure& close_callback)
    : profile_(profile),
      app_name_label(NULL),
      app_description_label(NULL),
      app_(app),
      close_callback_(close_callback),
      weak_ptr_factory_(this) {
  // Create controls
  app_name_label = new views::Label(base::UTF8ToUTF16(app_->name()));
  app_name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  app_description_label =
      new views::Label(base::UTF8ToUTF16(app_->description()));
  app_description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  app_version_label =
      new views::Label(base::UTF8ToUTF16(app_->VersionString()));
  app_version_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  app_icon = new views::ImageView();
  app_icon->SetImageSize(gfx::Size(kIconSize, kIconSize));

  permission_list_heading = new views::Label(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_CAN_ACCESS));
  permission_list_heading->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  permissions_scroll_view = new PermissionsScrollView(0, 100, app);

  // Load the app icon asynchronously. For the response, check OnImageLoaded.
  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      app,
      extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER);
  int pixel_size =
      static_cast<int>(kIconSize * gfx::ImageSkia::GetMaxSupportedScale());
  extensions::ImageLoader::Get(profile)
      ->LoadImageAsync(app,
                       image,
                       gfx::Size(pixel_size, pixel_size),
                       base::Bind(&AppInfoView::OnAppImageLoaded, AsWeakPtr()));

  // Create the layout
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // Header column set with app icon and information
  static const int kHeaderColumnSetId = 0;
  views::ColumnSet* header_column_set =
      layout->AddColumnSet(kHeaderColumnSetId);
  header_column_set->AddColumn(views::GridLayout::FILL,
                               views::GridLayout::CENTER,
                               0,
                               views::GridLayout::FIXED,
                               kIconSize,
                               0);
  header_column_set->AddPaddingColumn(0,
                                      views::kRelatedControlHorizontalSpacing);
  header_column_set->AddColumn(views::GridLayout::FILL,
                               views::GridLayout::CENTER,
                               100.0f,
                               views::GridLayout::FIXED,
                               0,
                               0);

  // Column set with scrollable permissions
  static const int kPermissionsColumnSetId = 1;
  views::ColumnSet* permissions_column_set =
      layout->AddColumnSet(kPermissionsColumnSetId);
  permissions_column_set->AddColumn(views::GridLayout::FILL,
                                    views::GridLayout::CENTER,
                                    100.0f,
                                    views::GridLayout::FIXED,
                                    0,
                                    0);

  // The app icon takes up 3 rows
  layout->StartRow(0, kHeaderColumnSetId);
  layout->AddView(app_icon, 1, 3);

  // The app information fills up the right side of the icon
  layout->AddView(app_name_label);

  layout->StartRow(0, kHeaderColumnSetId);
  layout->SkipColumns(1);
  layout->AddView(app_version_label);

  layout->StartRow(0, kHeaderColumnSetId);
  layout->SkipColumns(1);
  layout->AddView(app_description_label);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, kPermissionsColumnSetId);
  layout->AddView(permission_list_heading);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, kPermissionsColumnSetId);
  layout->AddView(permissions_scroll_view);
}

AppInfoView::~AppInfoView() {}

bool AppInfoView::Cancel() {
  if (!close_callback_.is_null())
    close_callback_.Run();
  return true;
}

gfx::Size AppInfoView::GetPreferredSize() {
  static const int kDialogWidth = 360;
  int height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

base::string16 AppInfoView::GetDialogButtonLabel(ui::DialogButton button)
    const {
  if (button == ui::DIALOG_BUTTON_CANCEL) {
    return l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_CLOSE_BUTTON);
  }
  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

int AppInfoView::GetDialogButtons() const { return ui::DIALOG_BUTTON_CANCEL; }

bool AppInfoView::IsDialogButtonEnabled(ui::DialogButton button) const {
  return true;
}

ui::ModalType AppInfoView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 AppInfoView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_TITLE);
}

void AppInfoView::OnAppImageLoaded(const gfx::Image& image) {
  const SkBitmap* bitmap = image.ToSkBitmap();
  if (image.IsEmpty()) {
    bitmap = &extensions::IconsInfo::GetDefaultAppIcon()
                  .GetRepresentation(gfx::ImageSkia::GetMaxSupportedScale())
                  .sk_bitmap();
  }

  app_icon->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
}
