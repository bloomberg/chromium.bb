// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_result.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/launcher_search_provider/service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "extensions/common/manifest_handlers/icons_handler.h"

namespace {

const char kResultIdDelimiter = ':';

}  // namespace

namespace app_list {

LauncherSearchResult::LauncherSearchResult(
    const std::string& item_id,
    const GURL& icon_url,
    const int discrete_value_relevance,
    Profile* profile,
    const extensions::Extension* extension)
    : item_id_(item_id),
      discrete_value_relevance_(discrete_value_relevance),
      profile_(profile),
      extension_(extension) {
  DCHECK_GE(discrete_value_relevance, 0);
  DCHECK_LE(discrete_value_relevance,
            chromeos::launcher_search_provider::kMaxSearchResultScore);

  // TODO(yawano) Decode passed icon url and show it badged with extension
  // icon.
  extension_icon_image_.reset(new extensions::IconImage(
      profile, extension, extensions::IconsInfo::GetIcons(extension),
      GetPreferredIconDimension(), extensions::util::GetDefaultExtensionIcon(),
      nullptr));

  Initialize();
}

LauncherSearchResult::~LauncherSearchResult() {
  if (extension_icon_image_ != nullptr)
    extension_icon_image_->RemoveObserver(this);
}

scoped_ptr<SearchResult> LauncherSearchResult::Duplicate() const {
  LauncherSearchResult* duplicated_result =
      new LauncherSearchResult(item_id_, discrete_value_relevance_, profile_,
                               extension_, extension_icon_image_);
  duplicated_result->set_title(title());
  return make_scoped_ptr(duplicated_result);
}

void LauncherSearchResult::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  DCHECK_EQ(image, extension_icon_image_.get());
  UpdateIcon();
}

LauncherSearchResult::LauncherSearchResult(
    const std::string& item_id,
    const int discrete_value_relevance,
    Profile* profile,
    const extensions::Extension* extension,
    const linked_ptr<extensions::IconImage>& extension_icon_image)
    : item_id_(item_id),
      discrete_value_relevance_(discrete_value_relevance),
      profile_(profile),
      extension_(extension),
      extension_icon_image_(extension_icon_image) {
  DCHECK(extension_icon_image_ != nullptr);
  Initialize();
}

void LauncherSearchResult::Initialize() {
  set_id(GetSearchResultId());
  set_relevance(static_cast<double>(discrete_value_relevance_) /
                static_cast<double>(
                    chromeos::launcher_search_provider::kMaxSearchResultScore));
  set_details(base::UTF8ToUTF16(extension_->name()));

  extension_icon_image_->AddObserver(this);
  UpdateIcon();
}

void LauncherSearchResult::UpdateIcon() {
  if (!extension_icon_image_->image_skia().isNull())
    SetIcon(extension_icon_image_->image_skia());
}

std::string LauncherSearchResult::GetSearchResultId() {
  return extension_->id() + kResultIdDelimiter + item_id_;
}

}  // namespace app_list
