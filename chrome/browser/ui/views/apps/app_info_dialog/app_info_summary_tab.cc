// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_summary_tab.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "url/gurl.h"

// Size of extension icon in top left of dialog.
const int kIconSize = 64;

AppInfoSummaryTab::AppInfoSummaryTab(gfx::NativeWindow parent_window,
                                     Profile* profile,
                                     const extensions::Extension* app,
                                     const base::Closure& close_callback)
    : AppInfoTab(parent_window, profile, app, close_callback),
      app_icon_(NULL),
      weak_ptr_factory_(this) {
  // Create UI elements.
  views::Label* app_name_label =
      new views::Label(base::UTF8ToUTF16(app_->name()));
  app_name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::Label* app_description_label =
      new views::Label(base::UTF8ToUTF16(app_->description()));
  app_description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::Label* app_version_label =
      new views::Label(base::UTF8ToUTF16(app_->VersionString()));
  app_version_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  app_icon_ = new views::ImageView();
  app_icon_->SetImageSize(gfx::Size(kIconSize, kIconSize));
  LoadAppImageAsync();

  // Create the layout.
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // Create a Header column set with app icon and information.
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

  // Create a main column set for the rest of the dialog content.
  static const int kMainColumnSetId = 1;
  views::ColumnSet* main_column_set = layout->AddColumnSet(kMainColumnSetId);
  main_column_set->AddColumn(views::GridLayout::FILL,
                             views::GridLayout::LEADING,
                             100.0f,
                             views::GridLayout::FIXED,
                             0,
                             0);

  // The app icon takes up 3 rows.
  layout->StartRow(0, kHeaderColumnSetId);
  layout->AddView(app_icon_, 1, 3);

  // The app information fills up the right side of the icon.
  layout->AddView(app_name_label);

  layout->StartRow(0, kHeaderColumnSetId);
  layout->SkipColumns(1);
  layout->AddView(app_version_label);

  layout->StartRow(0, kHeaderColumnSetId);
  layout->SkipColumns(1);
  layout->AddView(app_description_label);

  // Add a link to the web store for apps that have it.
  if (CanShowAppInWebStore()) {
    view_in_store_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_WEB_STORE_LINK));
    view_in_store_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view_in_store_link_->set_listener(this);

    layout->StartRow(0, kMainColumnSetId);
    layout->AddView(view_in_store_link_);
  }
}

AppInfoSummaryTab::~AppInfoSummaryTab() {}

void AppInfoSummaryTab::LinkClicked(views::Link* source, int event_flags) {
  if (source == view_in_store_link_) {
    ShowAppInWebStore();
  } else {
    NOTREACHED();
  }
}

void AppInfoSummaryTab::LoadAppImageAsync() {
  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      app_,
      extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER);
  int pixel_size =
      static_cast<int>(kIconSize * gfx::ImageSkia::GetMaxSupportedScale());
  extensions::ImageLoader::Get(profile_)->LoadImageAsync(
      app_,
      image,
      gfx::Size(pixel_size, pixel_size),
      base::Bind(&AppInfoSummaryTab::OnAppImageLoaded, AsWeakPtr()));
}

void AppInfoSummaryTab::OnAppImageLoaded(const gfx::Image& image) {
  const SkBitmap* bitmap;
  if (image.IsEmpty()) {
    bitmap = &extensions::IconsInfo::GetDefaultAppIcon()
                  .GetRepresentation(gfx::ImageSkia::GetMaxSupportedScale())
                  .sk_bitmap();
  } else {
    bitmap = image.ToSkBitmap();
  }

  app_icon_->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
}

bool AppInfoSummaryTab::CanShowAppInWebStore() const {
  return app_->from_webstore();
}

void AppInfoSummaryTab::ShowAppInWebStore() const {
  const GURL url = extensions::ManifestURL::GetDetailsURL(app_);
  DCHECK_NE(url, GURL::EmptyGURL());
  chrome::NavigateParams params(
      profile_,
      net::AppendQueryParameter(url,
                                extension_urls::kWebstoreSourceField,
                                extension_urls::kLaunchSourceAppListInfoDialog),
      content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}
