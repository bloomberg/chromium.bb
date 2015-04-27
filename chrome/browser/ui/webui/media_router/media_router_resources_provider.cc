// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_resources_provider.h"

#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

namespace {

void AddIcons(content::WebUIDataSource* html_source) {
  html_source->AddResourcePath("elements/icon/chromecast-icon.png",
                              IDR_MEDIA_ROUTER_CHROMECAST_ICON);
  html_source->AddResourcePath("elements/icon/chromecast-icon2x.png",
                              IDR_MEDIA_ROUTER_CHROMECAST_2X_ICON);
  html_source->AddResourcePath("elements/icon/close-gray.png",
                              IDR_CLOSE_GRAY_ICON);
  html_source->AddResourcePath("elements/icon/close-gray2x.png",
                              IDR_CLOSE_GRAY_2X_ICON);
  html_source->AddResourcePath("elements/icon/drop-down-arrow.png",
                              IDR_DROP_DOWN_ARROW_ICON);
  html_source->AddResourcePath("elements/icon/drop-down-arrow2x.png",
                              IDR_DROP_DOWN_ARROW_2X_ICON);
  html_source->AddResourcePath("elements/icon/drop-down-arrow-hover.png",
                              IDR_DROP_DOWN_ARROW_HOVER_ICON);
  html_source->AddResourcePath("elements/icon/drop-down-arrow-hover2x.png",
                              IDR_DROP_DOWN_ARROW_HOVER_2X_ICON);
  html_source->AddResourcePath("elements/icon/drop-down-arrow-showing.png",
                              IDR_DROP_DOWN_ARROW_SHOWING_ICON);
  html_source->AddResourcePath("elements/icon/drop-down-arrow-showing2x.png",
                              IDR_DROP_DOWN_ARROW_SHOWING_2X_ICON);
  html_source->AddResourcePath("elements/icon/generic-device.png",
                              IDR_MEDIA_ROUTER_GENERIC_DEVICE_2X_ICON);
  html_source->AddResourcePath("elements/icon/generic-device2x.png",
                              IDR_MEDIA_ROUTER_GENERIC_DEVICE_2X_ICON);
  html_source->AddResourcePath("elements/icon/hangouts-icon.png",
                              IDR_MEDIA_ROUTER_HANGOUTS_2X_ICON);
  html_source->AddResourcePath("elements/icon/hangouts-icon2x.png",
                              IDR_MEDIA_ROUTER_HANGOUTS_2X_ICON);
  html_source->AddResourcePath("elements/icon/sad-face.png",
                              IDR_SAD_FACE_ICON);
  html_source->AddResourcePath("elements/icon/sad-face2x.png",
                              IDR_SAD_FACE_2X_ICON);
}

void AddMainWebResources(content::WebUIDataSource* html_source) {
  // TODO(apacible): Add resources when they are available.
  html_source->AddResourcePath("media_router_data.js",
                               IDR_MEDIA_ROUTER_DATA_JS);
}

void AddPolymerElements(content::WebUIDataSource* html_source) {
  // TODO(apacible): Add resources when they are available.
  html_source->AddResourcePath(
      "elements/cast_mode_picker/cast_mode_picker.css",
      IDR_CAST_MODE_PICKER_CSS);
  html_source->AddResourcePath(
      "elements/cast_mode_picker/cast_mode_picker.html",
      IDR_CAST_MODE_PICKER_HTML);
  html_source->AddResourcePath(
      "elements/cast_mode_picker/cast_mode_picker.js",
      IDR_CAST_MODE_PICKER_JS);
  html_source->AddResourcePath(
      "elements/drop_down_button/drop_down_button.css",
      IDR_DROP_DOWN_BUTTON_CSS);
  html_source->AddResourcePath(
      "elements/drop_down_button/drop_down_button.html",
      IDR_DROP_DOWN_BUTTON_HTML);
  html_source->AddResourcePath(
      "elements/drop_down_button/drop_down_button.js",
      IDR_DROP_DOWN_BUTTON_JS);
  html_source->AddResourcePath(
      "elements/issue_banner/issue_banner.css",
      IDR_ISSUE_BANNER_CSS);
  html_source->AddResourcePath(
      "elements/issue_banner/issue_banner.html",
      IDR_ISSUE_BANNER_HTML);
  html_source->AddResourcePath(
      "elements/issue_banner/issue_banner.js",
      IDR_ISSUE_BANNER_JS);
}

}  // namespace

namespace media_router {

void AddMediaRouterUIResources(content::WebUIDataSource* html_source) {
  AddIcons(html_source);
  AddMainWebResources(html_source);
  AddPolymerElements(html_source);
  html_source->SetDefaultResource(IDR_MEDIA_ROUTER_HTML);
}

}  // namespace media_router
