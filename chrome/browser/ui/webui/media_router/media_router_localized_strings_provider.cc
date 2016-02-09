// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_localized_strings_provider.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

// Note that media_router.html contains a <script> tag which imports a script
// of the following name. These names must be kept in sync.
const char kLocalizedStringsFile[] = "strings.js";

void AddMediaRouterStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("mediaRouterTitle", IDS_MEDIA_ROUTER_TITLE);
}

void AddRouteDetailsStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("castingActivityStatus",
      IDS_MEDIA_ROUTER_CASTING_ACTIVITY_STATUS);
  html_source->AddLocalizedString("stopCastingButton",
      IDS_MEDIA_ROUTER_STOP_CASTING_BUTTON);
  html_source->AddLocalizedString("startCastingButton",
                                  IDS_MEDIA_ROUTER_START_CASTING_BUTTON);
}

void AddIssuesStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("dismissButton",
                                  IDS_MEDIA_ROUTER_DISMISS_BUTTON);
  html_source->AddLocalizedString("issueHeader",
                                  IDS_MEDIA_ROUTER_ISSUE_HEADER);
  html_source->AddLocalizedString("learnMoreButton",
                                  IDS_MEDIA_ROUTER_LEARN_MORE_BUTTON);
}

void AddMediaRouterContainerStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("firstRunFlowButton",
                                  IDS_MEDIA_ROUTER_FIRST_RUN_FLOW_BUTTON);
  html_source->AddLocalizedString("firstRunFlowLearnMore",
                                  IDS_MEDIA_ROUTER_FIRST_RUN_FLOW_LEARN_MORE);
  html_source->AddLocalizedString("firstRunFlowText",
                                  IDS_MEDIA_ROUTER_FIRST_RUN_FLOW_TEXT);
  html_source->AddLocalizedString("firstRunFlowTitle",
                                  IDS_MEDIA_ROUTER_FIRST_RUN_FLOW_TITLE);
#if defined(GOOGLE_CHROME_BUILD)
  html_source->AddLocalizedString("firstRunFlowCloudPrefText",
      IDS_MEDIA_ROUTER_FIRST_RUN_FLOW_CLOUD_PREF_TEXT);
#endif  // defined(GOOGLE_CHROME_BUILD)
  html_source->AddLocalizedString("autoCastMode",
                                  IDS_MEDIA_ROUTER_AUTO_CAST_MODE);
  html_source->AddLocalizedString("deviceMissing",
                                  IDS_MEDIA_ROUTER_DEVICE_MISSING);
  html_source->AddLocalizedString("selectCastModeHeader",
      IDS_MEDIA_ROUTER_SELECT_CAST_MODE_HEADER);
  html_source->AddLocalizedString("shareYourScreenSubheading",
      IDS_MEDIA_ROUTER_SHARE_YOUR_SCREEN_SUBHEADING);
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
