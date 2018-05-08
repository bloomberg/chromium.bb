// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_playstore_app_context_menu.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/app.mojom.h"
#include "components/crx_file/id_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// The id prefix to identify a Play Store search result.
constexpr char kPlayAppPrefix[] = "play://";
// Badge icon color, #000 at 54% opacity.
constexpr SkColor kBadgeColor = SkColorSetARGB(0x8A, 0x00, 0x00, 0x00);

bool LaunchIntent(const std::string& intent_uri, int64_t display_id) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;

  auto* arc_bridge = arc_service_manager->arc_bridge_service();

  if (auto* app_instance =
          ARC_GET_INSTANCE_FOR_METHOD(arc_bridge->app(), LaunchIntent)) {
    app_instance->LaunchIntent(intent_uri, display_id);
    return true;
  }

  if (auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge->app(), LaunchIntentDeprecated)) {
    app_instance->LaunchIntentDeprecated(intent_uri, base::nullopt);
    return true;
  }

  return false;
}

}  // namespace

namespace app_list {

ArcPlayStoreSearchResult::ArcPlayStoreSearchResult(
    arc::mojom::AppDiscoveryResultPtr data,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : data_(std::move(data)),
      profile_(profile),
      list_controller_(list_controller),
      weak_ptr_factory_(this) {
  SetTitle(base::UTF8ToUTF16(label().value()));
  set_id(kPlayAppPrefix +
         crx_file::id_util::GenerateId(install_intent_uri().value()));
  SetDisplayType(ash::SearchResultDisplayType::kTile);
  SetBadgeIcon(gfx::CreateVectorIcon(
      is_instant_app() ? kIcBadgeInstantIcon : kIcBadgePlayIcon,
      kAppBadgeIconSize, kBadgeColor));
  SetFormattedPrice(base::UTF8ToUTF16(formatted_price().value()));
  SetRating(review_score());
  SetResultType(is_instant_app() ? ash::SearchResultType::kInstantApp
                                 : ash::SearchResultType::kPlayStoreApp);

  icon_decode_request_ = std::make_unique<arc::IconDecodeRequest>(
      base::BindOnce(&ArcPlayStoreSearchResult::SetIcon,
                     weak_ptr_factory_.GetWeakPtr()),
      kGridIconDimension);
  icon_decode_request_->StartWithOptions(icon_png_data());
}

ArcPlayStoreSearchResult::~ArcPlayStoreSearchResult() = default;

void ArcPlayStoreSearchResult::Open(int event_flags) {
  if (!LaunchIntent(install_intent_uri().value(),
                    list_controller_->GetAppListDisplayId())) {
    return;
  }

  // Open the installing page of this result in Play Store.
  RecordHistogram(is_instant_app() ? PLAY_STORE_INSTANT_APP
                                   : PLAY_STORE_UNINSTALLED_APP);
}

void ArcPlayStoreSearchResult::GetContextMenuModel(
    GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<ArcPlayStoreAppContextMenu>(
      this, profile_, list_controller_);
  // TODO(755701): Enable context menu once Play Store API starts returning both
  // install and launch intents.
  std::move(callback).Run(nullptr);
}

void ArcPlayStoreSearchResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

AppContextMenu* ArcPlayStoreSearchResult::GetAppContextMenu() {
  return context_menu_.get();
}

}  // namespace app_list
