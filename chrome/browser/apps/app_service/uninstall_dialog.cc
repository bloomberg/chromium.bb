// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/uninstall_dialog.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/public/cpp/icon_loader.h"

namespace {

constexpr int32_t kUninstallIconSize = 32;
constexpr int32_t kArcUninstallIconSize = 48;

}  // namespace

namespace apps {

UninstallDialog::UninstallDialog(Profile* profile,
                                 apps::mojom::AppType app_type,
                                 const std::string& app_id,
                                 const std::string& app_name,
                                 apps::mojom::IconKeyPtr icon_key,
                                 apps::IconLoader* icon_loader,
                                 UninstallCallback uninstall_callback)
    : profile_(profile),
      app_type_(app_type),
      app_id_(app_id),
      app_name_(app_name),
      uninstall_callback_(std::move(uninstall_callback)) {
  int32_t size_hint_in_dip;
  switch (app_type) {
    case apps::mojom::AppType::kCrostini:
      // Crostini uninstall dialog doesn't show the icon.
      UiBase::Create(profile_, app_type_, app_id_, app_name, gfx::ImageSkia(),
                     this);
      return;
    case apps::mojom::AppType::kArc:
      // Currently ARC apps only support 48*48 native icon.
      size_hint_in_dip = kArcUninstallIconSize;
      break;
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
      size_hint_in_dip = kUninstallIconSize;
      break;
    default:
      NOTREACHED();
      return;
  }
  constexpr bool kAllowPlaceholderIcon = false;
  icon_loader->LoadIconFromIconKey(
      app_type, app_id, std::move(icon_key),
      apps::mojom::IconCompression::kUncompressed, size_hint_in_dip,
      kAllowPlaceholderIcon,
      base::BindOnce(&UninstallDialog::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr()));
}

UninstallDialog::~UninstallDialog() = default;

void UninstallDialog::OnDialogClosed(bool uninstall,
                                     bool clear_site_data,
                                     bool report_abuse) {
  std::move(uninstall_callback_)
      .Run(uninstall, clear_site_data, report_abuse, this);
}

void UninstallDialog::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    return;
  }

  UiBase::Create(profile_, app_type_, app_id_, app_name_,
                 icon_value->uncompressed, this);
}

}  // namespace apps
