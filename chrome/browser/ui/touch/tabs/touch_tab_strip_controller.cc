// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/tabs/touch_tab_strip_controller.h"

#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/touch/tabs/touch_tab.h"
#include "chrome/browser/ui/touch/tabs/touch_tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

namespace {

void CalcTouchIconTargetSize(int* width, int* height) {
  if (*width > TouchTab::kTouchTargetIconSize ||
      *height > TouchTab::kTouchTargetIconSize) {
    // Too big, resize it maintaining the aspect ratio.
    float aspect_ratio = static_cast<float>(*width) /
                         static_cast<float>(*height);
    *height = TouchTab::kTouchTargetIconSize;
    *width = static_cast<int>(aspect_ratio * *height);
    if (*width > TouchTab::kTouchTargetIconSize) {
      *width = TouchTab::kTouchTargetIconSize;
      *height = static_cast<int>(*width / aspect_ratio);
    }
  }
}

GURL GetURLWithoutFragment(const GURL& gurl) {
  url_canon::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return gurl.ReplaceComponents(replacements);
}

}  // namespace

TouchTabStripController::TouchTabStripController(Browser* browser,
                                                 TabStripModel* model)
    : BrowserTabStripController(browser, model) {
}

TouchTabStripController::~TouchTabStripController() {
}

void TouchTabStripController::TabDetachedAt(TabContentsWrapper* contents,
                                            int model_index) {
  if (consumer_.HasPendingRequests()) {
    TouchTab* touch_tab =
        static_cast<TouchTab*>(tabstrip()->GetBaseTabAtModelIndex(model_index));
    consumer_.CancelAllRequestsForClientData(touch_tab);
  }
  BrowserTabStripController::TabDetachedAt(contents, model_index);
}

void TouchTabStripController::TabChangedAt(TabContentsWrapper* contents,
                                           int model_index,
                                           TabChangeType change_type) {
  // Clear the large icon if we are loading a different URL in the same tab.
  if (change_type == LOADING_ONLY) {
    TouchTab* touch_tab =
        static_cast<TouchTab*>(tabstrip()->GetBaseTabAtModelIndex(model_index));
      if (!touch_tab->touch_icon().isNull()) {
      GURL existing_tab_url = GetURLWithoutFragment(touch_tab->data().url);
      GURL page_url = GetURLWithoutFragment(contents->tab_contents()->GetURL());
      // Reset touch icon if the url are different.
      if (existing_tab_url != page_url) {
        touch_tab->set_touch_icon(SkBitmap());
        consumer_.CancelAllRequestsForClientData(touch_tab);
      }
    }
  }

  // Always call parent's method.
  BrowserTabStripController::TabChangedAt(contents, model_index, change_type);
}

void TouchTabStripController::SetTabRendererDataFromModel(
    TabContents* contents,
    int model_index,
    TabRendererData* data,
    TabStatus tab_status) {
  // Call parent first.
  BrowserTabStripController::SetTabRendererDataFromModel(contents, model_index,
                                                         data, tab_status);
  if (tab_status == NEW_TAB)
    return;

  // Use the touch icon if any.
  TouchTab* touch_tab =
      static_cast<TouchTab*>(tabstrip()->GetBaseTabAtModelIndex(model_index));
    if (!touch_tab->touch_icon().isNull()) {
    data->favicon = touch_tab->touch_icon();
    return;
  }

  // In the case where we do not have a touch icon we scale up the small
  // favicons (16x16) which originally are coming from NavigationEntry.
  if (data->favicon.width() == kFaviconSize &&
      data->favicon.height() == kFaviconSize) {
    data->favicon = skia::ImageOperations::Resize(data->favicon,
        skia::ImageOperations::RESIZE_BEST, TouchTab::kTouchTargetIconSize,
        TouchTab::kTouchTargetIconSize);
  }

  // Check if we have an outstanding request for this tab.
  if (consumer_.HasPendingRequests())
    consumer_.CancelAllRequestsForClientData(touch_tab);

  // Request touch icon.
  GURL page_url = GetURLWithoutFragment(contents->GetURL());
  FaviconService* favicon_service = profile()->GetFaviconService(
      Profile::EXPLICIT_ACCESS);
  if (favicon_service) {
    CancelableRequestProvider::Handle h =
        favicon_service->GetFaviconForURL(
            page_url,
            history::TOUCH_ICON | history::TOUCH_PRECOMPOSED_ICON,
            &consumer_,
            NewCallback(this, &TouchTabStripController::OnTouchIconAvailable));
    consumer_.SetClientData(favicon_service, h, touch_tab);
  }
}

const TouchTabStrip* TouchTabStripController::tabstrip() const {
  return static_cast<const TouchTabStrip*>(
      BrowserTabStripController::tabstrip());
}

void TouchTabStripController::OnTouchIconAvailable(
    FaviconService::Handle h,
    history::FaviconData favicon) {
  // Abandon the request when there is no valid favicon.
  if (!favicon.is_valid())
    return;

  // Retrieve the model_index from the TouchTab pointer received.
  TouchTab* touch_tab = consumer_.GetClientDataForCurrentRequest();
  int model_index = tabstrip()->GetModelIndexOfBaseTab(touch_tab);
  if (!IsValidIndex(model_index))
    return;

  // Try to decode the favicon, return on failure.
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(favicon.image_data->front(),
                        favicon.image_data->size(),
                        &bitmap);
  if (bitmap.isNull())
    return;

  // Rescale output, if needed, and assign to the TouchTab instance.
  int width = bitmap.width();
  int height = bitmap.height();
  if (width == TouchTab::kTouchTargetIconSize &&
      height == TouchTab::kTouchTargetIconSize) {
    touch_tab->set_touch_icon(bitmap);
  } else {
    CalcTouchIconTargetSize(&width, &height);
    touch_tab->set_touch_icon(skia::ImageOperations::Resize(bitmap,
        skia::ImageOperations::RESIZE_BEST, width, height));
  }

  // Refresh UI since favicon changed.
  browser()->GetTabContentsAt(model_index)->NotifyNavigationStateChanged(
      TabContents::INVALIDATE_TAB);
}
