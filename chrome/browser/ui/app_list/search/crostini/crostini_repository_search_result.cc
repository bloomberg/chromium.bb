// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/crostini/crostini_repository_search_result.h"

#include <stddef.h>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {

namespace {

// TODO(https://crbug.com/921429): Need UX spec.
constexpr SkColor kListIconColor = SkColorSetARGB(0xDE, 0xAD, 0xAD, 0xAD);

}  // namespace

CrostiniRepositorySearchResult::CrostiniRepositorySearchResult(
    const std::string& app_name)
    : app_name_(app_name), weak_ptr_factory_(this) {
  // TODO(https://crbug.com/921429): Need UX spec.
  set_id("crostini:" + app_name_);
  SetResultType(ash::SearchResultType::kOmnibox);

  // TODO(https://crbug.com/921429): Need UX spec.
  const gfx::VectorIcon& icon = kFileDownloadIcon;
  SetIcon(gfx::CreateVectorIcon(
      icon, AppListConfig::instance().search_list_icon_dimension(),
      kListIconColor));
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_CROSTINI_REPOSITORY_SEARCH_RESULT_PLACEHOLDER_TEXT,
      base::UTF8ToUTF16(app_name_)));
}

CrostiniRepositorySearchResult::~CrostiniRepositorySearchResult() = default;

void CrostiniRepositorySearchResult::Open(int event_flags) {
  // TODO(https://crbug.com/921429): connect to logic in crostini_manager.
}

}  // namespace app_list
