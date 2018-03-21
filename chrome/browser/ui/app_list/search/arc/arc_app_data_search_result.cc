// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_data_search_result.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/arc/icon_decode_request.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/path.h"

namespace app_list {

namespace {

constexpr char kAppDataSearchPrefix[] = "appdatasearch://";

constexpr int kAvatarSize = 40;

bool LaunchIntent(const std::string& intent_uri, int64_t display_id) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;

  auto* app = arc_service_manager->arc_bridge_service()->app();

  if (auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(app, LaunchIntent)) {
    app_instance->LaunchIntent(intent_uri, display_id);
    return true;
  }

  if (auto* app_instance =
          ARC_GET_INSTANCE_FOR_METHOD(app, LaunchIntentDeprecated)) {
    app_instance->LaunchIntentDeprecated(intent_uri, base::nullopt);
    return true;
  }

  return false;
}

// Provide circular avatar image source.
class AvatarImageSource : public gfx::CanvasImageSource {
 public:
  AvatarImageSource(gfx::ImageSkia avatar, int size)
      : CanvasImageSource(gfx::Size(size, size), false), radius_(size / 2) {
    avatar_ = gfx::ImageSkiaOperations::CreateResizedImage(
        avatar, skia::ImageOperations::RESIZE_BEST, gfx::Size(size, size));
  }
  ~AvatarImageSource() override = default;

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    gfx::Path circular_mask;
    circular_mask.addCircle(SkIntToScalar(radius_), SkIntToScalar(radius_),
                            SkIntToScalar(radius_));
    canvas->ClipPath(circular_mask, true);
    canvas->DrawImageInt(avatar_, 0, 0);
  }

  gfx::ImageSkia avatar_;
  const int radius_;

  DISALLOW_COPY_AND_ASSIGN(AvatarImageSource);
};

}  // namespace

ArcAppDataSearchResult::ArcAppDataSearchResult(
    arc::mojom::AppDataResultPtr data,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : data_(std::move(data)),
      profile_(profile),
      list_controller_(list_controller),
      weak_ptr_factory_(this) {
  set_title(base::UTF8ToUTF16(label()));
  set_id(kAppDataSearchPrefix + launch_intent_uri());
  set_display_type(ash::SearchResultDisplayType::kTile);

  icon_decode_request_ = std::make_unique<IconDecodeRequest>(
      base::BindOnce(&ArcAppDataSearchResult::SetIconToAvatarIcon,
                     weak_ptr_factory_.GetWeakPtr()));
  icon_decode_request_->StartWithOptions(icon_png_data());
}

ArcAppDataSearchResult::~ArcAppDataSearchResult() = default;

std::unique_ptr<SearchResult> ArcAppDataSearchResult::Duplicate() const {
  std::unique_ptr<ArcAppDataSearchResult> result =
      std::make_unique<ArcAppDataSearchResult>(data_.Clone(), profile_,
                                               list_controller_);
  result->SetIcon(icon());
  return result;
}

ui::MenuModel* ArcAppDataSearchResult::GetContextMenuModel() {
  // TODO(warx): Enable Context Menu.
  return nullptr;
}

void ArcAppDataSearchResult::Open(int event_flags) {
  LaunchIntent(launch_intent_uri(), list_controller_->GetAppListDisplayId());
}

void ArcAppDataSearchResult::SetIconToAvatarIcon(const gfx::ImageSkia& icon) {
  SetIcon(gfx::ImageSkia(std::make_unique<AvatarImageSource>(icon, kAvatarSize),
                         gfx::Size(kAvatarSize, kAvatarSize)));
}

}  // namespace app_list
