// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "net/base/url_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {

// Size of extension icon in top left of dialog.
const int kIconSize = 64;

// The horizontal spacing between the app's links in the header section.
const int kSpacingBetweenAppLinks = 3;

}  // namespace

AppInfoHeaderPanel::AppInfoHeaderPanel(Profile* profile,
                                       const extensions::Extension* app)
    : AppInfoPanel(profile, app),
      app_icon_(NULL),
      app_name_label_(NULL),
      view_in_store_link_(NULL),
      licenses_link_(NULL),
      weak_ptr_factory_(this) {
  CreateControls();

  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           views::kButtonHEdgeMargin,
                           views::kButtonVEdgeMargin,
                           views::kRelatedControlHorizontalSpacing));

  LayoutControls();
}

AppInfoHeaderPanel::~AppInfoHeaderPanel() {
}

void AppInfoHeaderPanel::CreateControls() {
  app_name_label_ =
      new views::Label(base::UTF8ToUTF16(app_->name()),
                       ui::ResourceBundle::GetSharedInstance().GetFontList(
                           ui::ResourceBundle::MediumFont));
  app_name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  app_icon_ = new views::ImageView();
  app_icon_->SetImageSize(gfx::Size(kIconSize, kIconSize));
  LoadAppImageAsync();

  if (CanShowAppInWebStore()) {
    view_in_store_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_WEB_STORE_LINK));
    view_in_store_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view_in_store_link_->set_listener(this);
  }

  if (CanDisplayLicenses()) {
    licenses_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_LICENSES_BUTTON_TEXT));
    licenses_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    licenses_link_->set_listener(this);
  }
}

void AppInfoHeaderPanel::LayoutControls() {
  AddChildView(app_icon_);
  if (!view_in_store_link_ && !licenses_link_) {
    // If there's no links, allow the app's name to take up multiple lines.
    // TODO(sashab): Limit the number of lines to 2.
    app_name_label_->SetMultiLine(true);
    AddChildView(app_name_label_);
  } else {
    // Create a vertical container to store the app's name and links.
    views::View* vertical_info_container = new views::View();
    views::BoxLayout* vertical_container_layout =
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
    vertical_container_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    vertical_info_container->SetLayoutManager(vertical_container_layout);

    vertical_info_container->AddChildView(app_name_label_);
    // Create a horizontal container to store the app's links.
    views::View* horizontal_links_container =
        CreateHorizontalStack(kSpacingBetweenAppLinks);
    if (view_in_store_link_)
      horizontal_links_container->AddChildView(view_in_store_link_);
    if (licenses_link_)
      horizontal_links_container->AddChildView(licenses_link_);
    vertical_info_container->AddChildView(horizontal_links_container);

    AddChildView(vertical_info_container);
  }
}
void AppInfoHeaderPanel::LinkClicked(views::Link* source, int event_flags) {
  if (source == view_in_store_link_) {
    ShowAppInWebStore();
  } else if (source == licenses_link_) {
    DisplayLicenses();
  } else {
    NOTREACHED();
  }
}

void AppInfoHeaderPanel::LoadAppImageAsync() {
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
      base::Bind(&AppInfoHeaderPanel::OnAppImageLoaded, AsWeakPtr()));
}

void AppInfoHeaderPanel::OnAppImageLoaded(const gfx::Image& image) {
  const SkBitmap* bitmap;
  if (image.IsEmpty()) {
    bitmap = &extensions::util::GetDefaultAppIcon()
                  .GetRepresentation(gfx::ImageSkia::GetMaxSupportedScale())
                  .sk_bitmap();
  } else {
    bitmap = image.ToSkBitmap();
  }

  app_icon_->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
}

void AppInfoHeaderPanel::ShowAppInWebStore() const {
  DCHECK(CanShowAppInWebStore());
  const GURL url = extensions::ManifestURL::GetDetailsURL(app_);
  DCHECK_NE(url, GURL::EmptyGURL());
  chrome::NavigateParams params(
      profile_,
      net::AppendQueryParameter(url,
                                extension_urls::kWebstoreSourceField,
                                extension_urls::kLaunchSourceAppListInfoDialog),
      ui::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

bool AppInfoHeaderPanel::CanShowAppInWebStore() const {
  return app_->from_webstore();
}

void AppInfoHeaderPanel::DisplayLicenses() {
  // Find the first shared module for this app, and display it's options page
  // as an 'about' link.
  // TODO(sashab): Revisit UI layout once shared module usage becomes more
  // common.
  DCHECK(CanDisplayLicenses());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(service);
  const std::vector<extensions::SharedModuleInfo::ImportInfo>& imports =
      extensions::SharedModuleInfo::GetImports(app_);
  const extensions::Extension* imported_module =
      service->GetExtensionById(imports[0].extension_id, true);
  DCHECK(imported_module);
  GURL about_page = extensions::ManifestURL::GetAboutPage(imported_module);
  DCHECK(about_page != GURL::EmptyGURL());
  chrome::NavigateParams params(
      profile_, about_page, ui::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

bool AppInfoHeaderPanel::CanDisplayLicenses() {
  if (!extensions::SharedModuleInfo::ImportsModules(app_))
    return false;
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(service);
  const std::vector<extensions::SharedModuleInfo::ImportInfo>& imports =
      extensions::SharedModuleInfo::GetImports(app_);
  const extensions::Extension* imported_module =
      service->GetExtensionById(imports[0].extension_id, true);
  DCHECK(imported_module);
  GURL about_page = extensions::ManifestURL::GetAboutPage(imported_module);
  if (about_page == GURL::EmptyGURL())
    return false;
  return true;
}
