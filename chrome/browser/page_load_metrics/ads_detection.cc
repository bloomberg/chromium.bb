// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/ads_detection.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

bool IsGoogleAd(content::NavigationHandle* navigation_handle) {
  // Because sub-resource filtering isn't always enabled, and doesn't work
  // well in monitoring mode (no CSS enforcement), it's difficult to identify
  // ads. Google ads are prevalent and easy to track, so we'll start by
  // tracking those. Note that the frame name can be very large, so be careful
  // to avoid full string searches if possible.
  // TODO(jkarlin): Track other ad networks that are easy to identify.

  // In case the navigation aborted, look up the RFH by the Frame Tree Node
  // ID. It returns the committed frame host or the initial frame host for the
  // frame if no committed host exists. Using a previous host is fine because
  // once a frame has an ad we always consider it to have an ad.
  // We use the unsafe method of FindFrameByFrameTreeNodeId because we're not
  // concerned with which process the frame lives on (we're just measuring
  // bytes and not granting security privileges).
  content::RenderFrameHost* current_frame_host =
      navigation_handle->GetWebContents()->UnsafeFindFrameByFrameTreeNodeId(
          navigation_handle->GetFrameTreeNodeId());
  if (current_frame_host) {
    const std::string& frame_name = current_frame_host->GetFrameName();
    if (base::StartsWith(frame_name, "google_ads_iframe",
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(frame_name, "google_ads_frame",
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  const GURL& frame_url = navigation_handle->GetURL();
  if (frame_url.host_piece() == "tpc.googlesyndication.com" &&
      base::StartsWith(frame_url.path_piece(), "/safeframe",
                       base::CompareCase::SENSITIVE)) {
    return true;
  }

  return false;
}

// Associates the detected AdTypes for a navigation with its NavigationHandle.
class NavigationHandleAdsData : public base::SupportsUserData::Data {
 public:
  static NavigationHandleAdsData* GetOrCreate(
      content::NavigationHandle* navigation_handle) {
    DCHECK(navigation_handle);
    NavigationHandleAdsData* ads_data = static_cast<NavigationHandleAdsData*>(
        navigation_handle->GetUserData(kUserDataKey));
    if (!ads_data) {
      std::unique_ptr<NavigationHandleAdsData> new_ads_data =
          base::MakeUnique<NavigationHandleAdsData>();

      // It is safe to retain |ads_data| raw pointer, despite passing an
      // ownership of |new_ads_data| to SetUserData, because |navigation_handle|
      // will keep the NavigationHandleAdsData instance alive until the
      // |navigation_handle| is destroyed.
      ads_data = new_ads_data.get();

      navigation_handle->SetUserData(kUserDataKey, std::move(new_ads_data));
    }

    return ads_data;
  }

  NavigationHandleAdsData() = default;
  ~NavigationHandleAdsData() override {}

  AdTypes& ad_types() { return ad_types_; }
  const AdTypes& ad_types() const { return ad_types_; }

  void CalculateGoogleAdsIfNecessary(
      content::NavigationHandle* navigation_handle) {
    if (has_calculated_google_ads_)
      return;
    has_calculated_google_ads_ = true;

    if (IsGoogleAd(navigation_handle))
      ad_types_.set(AD_TYPE_GOOGLE);
  }

 private:
  AdTypes ad_types_;
  bool has_calculated_google_ads_ = false;

  static const char kUserDataKey[];

  DISALLOW_COPY_AND_ASSIGN(NavigationHandleAdsData);
};

const char NavigationHandleAdsData::kUserDataKey[] = "AdsData";

}  // namespace

const AdTypes& GetDetectedAdTypes(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);

  // TODO(lukasza): https://crbug.com/746638: Add a DCHECK that verifies that
  // GetDetectedAdTypes is called not earlier than when the |navigation_handle|
  // is ready to commit.

  NavigationHandleAdsData* ads_data =
      NavigationHandleAdsData::GetOrCreate(navigation_handle);
  ads_data->CalculateGoogleAdsIfNecessary(navigation_handle);
  return ads_data->ad_types();
}

void SetDetectedAdType(content::NavigationHandle* navigation_handle,
                       AdType type) {
  DCHECK(navigation_handle);
  NavigationHandleAdsData::GetOrCreate(navigation_handle)->ad_types().set(type);
}

}  // namespace page_load_metrics
