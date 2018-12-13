// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_localized_strings_provider.h"

#include "base/stl_util.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

// Note that media_router.html contains a <script> tag which imports a script
// of the following name. These names must be kept in sync.
const char kLocalizedStringsFile[] = "strings.js";

void AddMediaRouterStrings(content::WebUIDataSource* html_source) {
  static constexpr LocalizedString kLocalizedStrings[] = {
      {"mediaRouterTitle", IDS_MEDIA_ROUTER_TITLE},
      {"learnMoreText", IDS_LEARN_MORE},
      {"backButtonTitle", IDS_MEDIA_ROUTER_BACK_BUTTON_TITLE},
      {"closeButtonTitle", IDS_MEDIA_ROUTER_CLOSE_BUTTON_TITLE},
      {"searchButtonTitle", IDS_MEDIA_ROUTER_SEARCH_BUTTON_TITLE},
      {"viewCastModeListButtonTitle",
       IDS_MEDIA_ROUTER_VIEW_CAST_MODE_LIST_BUTTON_TITLE},
      {"viewDeviceListButtonTitle",
       IDS_MEDIA_ROUTER_VIEW_DEVICE_LIST_BUTTON_TITLE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings,
                          base::size(kLocalizedStrings));
}

void AddRouteDetailsStrings(content::WebUIDataSource* html_source) {
  static constexpr LocalizedString kLocalizedStrings[] = {
      {"castingActivityStatus", IDS_MEDIA_ROUTER_CASTING_ACTIVITY_STATUS},
      {"stopCastingButtonText", IDS_MEDIA_ROUTER_STOP_CASTING_BUTTON},
      {"startCastingButtonText", IDS_MEDIA_ROUTER_START_CASTING_BUTTON},
      {"playTitle", IDS_MEDIA_ROUTER_ROUTE_DETAILS_PLAY_TITLE},
      {"pauseTitle", IDS_MEDIA_ROUTER_ROUTE_DETAILS_PAUSE_TITLE},
      {"muteTitle", IDS_MEDIA_ROUTER_ROUTE_DETAILS_MUTE_TITLE},
      {"unmuteTitle", IDS_MEDIA_ROUTER_ROUTE_DETAILS_UNMUTE_TITLE},
      {"seekTitle", IDS_MEDIA_ROUTER_ROUTE_DETAILS_SEEK_TITLE},
      {"volumeTitle", IDS_MEDIA_ROUTER_ROUTE_DETAILS_VOLUME_TITLE},
      {"currentTimeLabel", IDS_MEDIA_ROUTER_ROUTE_DETAILS_CURRENT_TIME_LABEL},
      {"durationLabel", IDS_MEDIA_ROUTER_ROUTE_DETAILS_DURATION_LABEL},
      {"hangoutsLocalPresentTitle",
       IDS_MEDIA_ROUTER_ROUTE_DETAILS_HANGOUTS_LOCAL_PRESENT_TITLE},
      {"hangoutsLocalPresentSubtitle",
       IDS_MEDIA_ROUTER_ROUTE_DETAILS_HANGOUTS_LOCAL_PRESENT_SUBTITLE},
      {"alwaysUseMirroringTitle",
       IDS_MEDIA_ROUTER_ROUTE_DETAILS_ALWAYS_USE_MIRRORING_TITLE},
      {"fullscreenVideosDropdownTitle",
       IDS_MEDIA_ROUTER_ROUTE_DETAILS_FULLSCREEN_VIDEOS_DROPDOWN_TITLE},
      {"fullscreenVideosRemoteScreen",
       IDS_MEDIA_ROUTER_ROUTE_DETAILS_FULLSCREEN_VIDEOS_REMOTE_SCREEN},
      {"fullscreenVideosBothScreens",
       IDS_MEDIA_ROUTER_ROUTE_DETAILS_FULLSCREEN_VIDEOS_BOTH_SCREENS},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings,
                          base::size(kLocalizedStrings));
}

void AddIssuesStrings(content::WebUIDataSource* html_source) {
  static constexpr LocalizedString kLocalizedStrings[] = {
      {"dismissButton", IDS_MEDIA_ROUTER_DISMISS_BUTTON},
      {"issueHeaderText", IDS_MEDIA_ROUTER_ISSUE_HEADER},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings,
                          base::size(kLocalizedStrings));
}

void AddMediaRouterContainerStrings(content::WebUIDataSource* html_source) {
  static constexpr LocalizedString kLocalizedStrings[] = {
      {"castLocalMediaSubheadingText",
       IDS_MEDIA_ROUTER_CAST_LOCAL_MEDIA_SUBHEADING},
      {"castLocalMediaSelectedFileTitle",
       IDS_MEDIA_ROUTER_CAST_LOCAL_MEDIA_TITLE},
      {"firstRunFlowButtonText", IDS_MEDIA_ROUTER_FIRST_RUN_FLOW_BUTTON},
      {"firstRunFlowText", IDS_MEDIA_ROUTER_FIRST_RUN_FLOW_TEXT},
      {"firstRunFlowTitle", IDS_MEDIA_ROUTER_FIRST_RUN_FLOW_TITLE},
      {"firstRunFlowCloudPrefText",
       IDS_MEDIA_ROUTER_FIRST_RUN_FLOW_CLOUD_PREF_TEXT},
      {"autoCastMode", IDS_MEDIA_ROUTER_AUTO_CAST_MODE},
      {"destinationMissingText", IDS_MEDIA_ROUTER_DESTINATION_MISSING},
      {"searchInputLabel", IDS_MEDIA_ROUTER_SEARCH_LABEL},
      {"searchNoMatchesText", IDS_MEDIA_ROUTER_SEARCH_NO_MATCHES},
      {"selectCastModeHeaderText", IDS_MEDIA_ROUTER_SELECT_CAST_MODE_HEADER},
      {"shareYourScreenSubheadingText",
       IDS_MEDIA_ROUTER_SHARE_YOUR_SCREEN_SUBHEADING},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings,
                          base::size(kLocalizedStrings));
}

}  // namespace

namespace media_router {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  AddMediaRouterStrings(html_source);
  AddRouteDetailsStrings(html_source);
  AddIssuesStrings(html_source);
  AddMediaRouterContainerStrings(html_source);
  html_source->SetJsonPath(kLocalizedStringsFile);
}

}  // namespace media_router
