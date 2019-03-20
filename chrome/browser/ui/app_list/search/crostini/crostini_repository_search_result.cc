// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/crostini/crostini_repository_search_result.h"

#include <stddef.h>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/crostini/crostini_package_service.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {

namespace {

// TODO(https://crbug.com/921429): Need UX spec.
constexpr SkColor kListIconColor = SkColorSetARGB(0xDE, 0x00, 0x00, 0x00);

}  // namespace

CrostiniRepositorySearchResult::CrostiniRepositorySearchResult(
    Profile* profile,
    const std::string& app_name)
    : profile_(profile), app_name_(app_name), weak_ptr_factory_(this) {
  // TODO(https://crbug.com/921429): Need UX spec.
  set_id("crostini:" + app_name_);
  SetResultType(ash::SearchResultType::kOmnibox);

  // TODO(https://crbug.com/921429): Need UX spec.
  const gfx::VectorIcon& icon = kFileDownloadIcon;
  SetIcon(gfx::CreateVectorIcon(
      icon, AppListConfig::instance().search_list_icon_dimension(),
      kListIconColor));
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_CROSTINI_REPOSITORY_SEARCH_RESULT_TITLE_PLACEHOLDER_TEXT,
      base::UTF8ToUTF16(app_name_)));
  SetDetails(l10n_util::GetStringUTF16(
      IDS_CROSTINI_REPOSITORY_SEARCH_RESULT_DETAILS_PLACEHOLDER_TEXT));
}

CrostiniRepositorySearchResult::~CrostiniRepositorySearchResult() = default;

void CrostiniRepositorySearchResult::OnOpen(
    const crostini::LinuxPackageInfo& package_info) {
  // TODO(https://crbug.com/921429): Handle when |package_info| fails.
  if (package_info.success) {
    crostini::ShowCrostiniAppInstallerView(profile_, package_info);
  }
}

void CrostiniRepositorySearchResult::Open(int event_flags) {
  crostini::CrostiniManager::GetForProfile(profile_)
      ->GetLinuxPackageInfoFromApt(
          crostini::kCrostiniDefaultVmName,
          crostini::kCrostiniDefaultContainerName, app_name_,
          base::BindOnce(&CrostiniRepositorySearchResult::OnOpen,
                         weak_ptr_factory_.GetWeakPtr()));
}

SearchResultType CrostiniRepositorySearchResult::GetSearchResultType() const {
  return CROSTINI_APP;
}

}  // namespace app_list
