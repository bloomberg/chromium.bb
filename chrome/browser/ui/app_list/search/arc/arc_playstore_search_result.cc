// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_result.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_playstore_app_context_menu.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "chrome/grit/component_extension_resources.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/app.mojom.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/vector_icons/vector_icons.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"

using content::BrowserThread;

namespace {
bool disable_safe_decoding_for_testing = false;
// The id prefix to identify a Play Store search result.
constexpr char kPlayAppPrefix[] = "play://";
// Badge icon color, #000 at 54% opacity.
constexpr SkColor kBadgeColor = SkColorSetARGBMacro(0x8A, 0x00, 0x00, 0x00);

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

////////////////////////////////////////////////////////////////////////////////
// IconSource

class IconSource : public gfx::ImageSkiaSource {
 public:
  IconSource(const SkBitmap& decoded_bitmap, int resource_size_in_dip);
  explicit IconSource(int resource_size_in_dip);
  ~IconSource() override = default;

  void SetDecodedImage(const SkBitmap& decoded_bitmap);

 private:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  const int resource_size_in_dip_;
  gfx::ImageSkia decoded_icon_;

  DISALLOW_COPY_AND_ASSIGN(IconSource);
};

IconSource::IconSource(int resource_size_in_dip)
    : resource_size_in_dip_(resource_size_in_dip) {}

void IconSource::SetDecodedImage(const SkBitmap& decoded_bitmap) {
  decoded_icon_.AddRepresentation(gfx::ImageSkiaRep(
      decoded_bitmap, ui::GetScaleForScaleFactor(ui::SCALE_FACTOR_100P)));
}

gfx::ImageSkiaRep IconSource::GetImageForScale(float scale) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // We use the icon if it was decoded successfully, otherwise use the default
  // ARC icon.
  const gfx::ImageSkia* icon_to_scale;
  if (decoded_icon_.isNull()) {
    int resource_id =
        scale >= 1.5f ? IDR_ARC_SUPPORT_ICON_96 : IDR_ARC_SUPPORT_ICON_48;
    icon_to_scale =
        ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  } else {
    icon_to_scale = &decoded_icon_;
  }
  DCHECK(icon_to_scale);

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      *icon_to_scale, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(resource_size_in_dip_, resource_size_in_dip_));
  return resized_image.GetRepresentation(scale);
}

////////////////////////////////////////////////////////////////////////////////
// IconDecodeRequest

class ArcPlayStoreSearchResult::IconDecodeRequest
    : public ImageDecoder::ImageRequest {
 public:
  explicit IconDecodeRequest(ArcPlayStoreSearchResult* search_result)
      : search_result_(search_result) {}
  ~IconDecodeRequest() override = default;

  // ImageDecoder::ImageRequest overrides.
  void OnImageDecoded(const SkBitmap& bitmap) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    const gfx::Size resource_size(app_list::kGridIconDimension,
                                  app_list::kGridIconDimension);
    auto icon_source =
        base::MakeUnique<IconSource>(app_list::kGridIconDimension);
    icon_source->SetDecodedImage(bitmap);
    const gfx::ImageSkia icon =
        gfx::ImageSkia(std::move(icon_source), resource_size);
    icon.EnsureRepsForSupportedScales();

    search_result_->SetIcon(icon);
  }

  void OnDecodeImageFailed() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DLOG(ERROR) << "Failed to decode an app icon image.";

    const gfx::Size resource_size(app_list::kGridIconDimension,
                                  app_list::kGridIconDimension);
    auto icon_source =
        base::MakeUnique<IconSource>(app_list::kGridIconDimension);
    const gfx::ImageSkia icon =
        gfx::ImageSkia(std::move(icon_source), resource_size);
    icon.EnsureRepsForSupportedScales();

    search_result_->SetIcon(icon);
  }

 private:
  // ArcPlayStoreSearchResult owns IconDecodeRequest, so it will outlive this.
  ArcPlayStoreSearchResult* const search_result_;

  DISALLOW_COPY_AND_ASSIGN(IconDecodeRequest);
};

////////////////////////////////////////////////////////////////////////////////
// ArcPlayStoreSearchResult

// static
void ArcPlayStoreSearchResult::DisableSafeDecodingForTesting() {
  disable_safe_decoding_for_testing = true;
}

ArcPlayStoreSearchResult::ArcPlayStoreSearchResult(
    arc::mojom::AppDiscoveryResultPtr data,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : data_(std::move(data)),
      profile_(profile),
      list_controller_(list_controller) {
  set_title(base::UTF8ToUTF16(label().value()));
  set_id(kPlayAppPrefix +
         crx_file::id_util::GenerateId(install_intent_uri().value()));
  set_display_type(DISPLAY_TILE);
  SetBadgeIcon(gfx::CreateVectorIcon(
      is_instant_app() ? kIcBadgeInstantIcon : kIcBadgePlayIcon,
      kAppBadgeIconSize, kBadgeColor));
  SetFormattedPrice(base::UTF8ToUTF16(formatted_price().value()));
  SetRating(review_score());
  set_result_type(is_instant_app() ? RESULT_INSTANT_APP : RESULT_PLAYSTORE_APP);

  icon_decode_request_ = base::MakeUnique<IconDecodeRequest>(this);
  if (disable_safe_decoding_for_testing) {
    SkBitmap bitmap;
    if (!icon_png_data().empty() &&
        gfx::PNGCodec::Decode(
            reinterpret_cast<const unsigned char*>(icon_png_data().data()),
            icon_png_data().size(), &bitmap)) {
      icon_decode_request_->OnImageDecoded(bitmap);
    } else {
      icon_decode_request_->OnDecodeImageFailed();
    }
  } else {
    ImageDecoder::StartWithOptions(icon_decode_request_.get(), icon_png_data(),
                                   ImageDecoder::DEFAULT_CODEC, true,
                                   gfx::Size());
  }
}

ArcPlayStoreSearchResult::~ArcPlayStoreSearchResult() = default;

std::unique_ptr<SearchResult> ArcPlayStoreSearchResult::Duplicate() const {
  std::unique_ptr<ArcPlayStoreSearchResult> result =
      base::MakeUnique<ArcPlayStoreSearchResult>(data_.Clone(), profile_,
                                                 list_controller_);
  result->SetIcon(icon());
  return result;
}

void ArcPlayStoreSearchResult::Open(int event_flags) {
  if (!LaunchIntent(install_intent_uri().value(),
                    list_controller_->GetAppListDisplayId())) {
    return;
  }

  // Open the installing page of this result in Play Store.
  RecordHistogram(is_instant_app() ? PLAY_STORE_INSTANT_APP
                                   : PLAY_STORE_UNINSTALLED_APP);
  base::RecordAction(
      is_instant_app()
          ? base::UserMetricsAction(
                "Arc.Launcher.Search.OpenPlayStore.InstantApps")
          : base::UserMetricsAction(
                "Arc.Launcher.Search.OpenPlayStore.UninstalledApps"));
}

ui::MenuModel* ArcPlayStoreSearchResult::GetContextMenuModel() {
  context_menu_ = base::MakeUnique<ArcPlayStoreAppContextMenu>(
      this, profile_, list_controller_);
  // TODO(755701): Enable context menu once Play Store API starts returning both
  // install and launch intents.
  return nullptr;
}

void ArcPlayStoreSearchResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

}  // namespace app_list
