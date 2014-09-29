// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/polymer_resources_map.h"

#include "grit/browser_resources.h"

namespace chromeos {

std::vector<std::pair<std::string, int>> GetPolymerResourcesMap() {
  std::vector<std::pair<std::string, int>> result;
  result.push_back(
      std::make_pair("polymer/core-animated-pages/core-animated-pages.css",
                     IDR_POLYMER_CORE_ANIMATED_PAGES_CORE_ANIMATED_PAGES_CSS));
  result.push_back(std::make_pair(
      "polymer/core-animated-pages/core-animated-pages-extracted.js",
      IDR_POLYMER_CORE_ANIMATED_PAGES_CORE_ANIMATED_PAGES_EXTRACTED_JS));
  result.push_back(
      std::make_pair("polymer/core-animated-pages/core-animated-pages.html",
                     IDR_POLYMER_CORE_ANIMATED_PAGES_CORE_ANIMATED_PAGES_HTML));
  result.push_back(std::make_pair(
      "polymer/core-animated-pages/transitions/"
      "core-transition-pages-extracted.js",
      IDR_POLYMER_CORE_ANIMATED_PAGES_TRANSITIONS_CORE_TRANSITION_PAGES_EXTRACTED_JS));
  result.push_back(std::make_pair(
      "polymer/core-animated-pages/transitions/core-transition-pages.html",
      IDR_POLYMER_CORE_ANIMATED_PAGES_TRANSITIONS_CORE_TRANSITION_PAGES_HTML));
  result.push_back(std::make_pair(
      "polymer/core-animated-pages/transitions/cross-fade.html",
      IDR_POLYMER_CORE_ANIMATED_PAGES_TRANSITIONS_CROSS_FADE_HTML));
  result.push_back(std::make_pair(
      "polymer/core-animated-pages/transitions/hero-transition-extracted.js",
      IDR_POLYMER_CORE_ANIMATED_PAGES_TRANSITIONS_HERO_TRANSITION_EXTRACTED_JS));
  result.push_back(std::make_pair(
      "polymer/core-animated-pages/transitions/hero-transition.html",
      IDR_POLYMER_CORE_ANIMATED_PAGES_TRANSITIONS_HERO_TRANSITION_HTML));
  result.push_back(std::make_pair("polymer/core-icon/core-icon.css",
                                  IDR_POLYMER_CORE_ICON_CORE_ICON_CSS));
  result.push_back(
      std::make_pair("polymer/core-icon/core-icon-extracted.js",
                     IDR_POLYMER_CORE_ICON_CORE_ICON_EXTRACTED_JS));
  result.push_back(std::make_pair("polymer/core-icon/core-icon.html",
                                  IDR_POLYMER_CORE_ICON_CORE_ICON_HTML));
  result.push_back(
      std::make_pair("polymer/core-iconset/core-iconset-extracted.js",
                     IDR_POLYMER_CORE_ICONSET_CORE_ICONSET_EXTRACTED_JS));
  result.push_back(std::make_pair("polymer/core-iconset/core-iconset.html",
                                  IDR_POLYMER_CORE_ICONSET_CORE_ICONSET_HTML));
  result.push_back(std::make_pair(
      "polymer/core-iconset-svg/core-iconset-svg-extracted.js",
      IDR_POLYMER_CORE_ICONSET_SVG_CORE_ICONSET_SVG_EXTRACTED_JS));
  result.push_back(
      std::make_pair("polymer/core-iconset-svg/core-iconset-svg.html",
                     IDR_POLYMER_CORE_ICONSET_SVG_CORE_ICONSET_SVG_HTML));
  result.push_back(std::make_pair("polymer/core-item/core-item.css",
                                  IDR_POLYMER_CORE_ITEM_CORE_ITEM_CSS));
  result.push_back(
      std::make_pair("polymer/core-item/core-item-extracted.js",
                     IDR_POLYMER_CORE_ITEM_CORE_ITEM_EXTRACTED_JS));
  result.push_back(std::make_pair("polymer/core-item/core-item.html",
                                  IDR_POLYMER_CORE_ITEM_CORE_ITEM_HTML));
  result.push_back(
      std::make_pair("polymer/core-meta/core-meta-extracted.js",
                     IDR_POLYMER_CORE_META_CORE_META_EXTRACTED_JS));
  result.push_back(std::make_pair("polymer/core-meta/core-meta.html",
                                  IDR_POLYMER_CORE_META_CORE_META_HTML));
  result.push_back(
      std::make_pair("polymer/core-range/core-range-extracted.js",
                     IDR_POLYMER_CORE_RANGE_CORE_RANGE_EXTRACTED_JS));
  result.push_back(std::make_pair("polymer/core-range/core-range.html",
                                  IDR_POLYMER_CORE_RANGE_CORE_RANGE_HTML));
  result.push_back(
      std::make_pair("polymer/core-selection/core-selection-extracted.js",
                     IDR_POLYMER_CORE_SELECTION_CORE_SELECTION_EXTRACTED_JS));
  result.push_back(
      std::make_pair("polymer/core-selection/core-selection.html",
                     IDR_POLYMER_CORE_SELECTION_CORE_SELECTION_HTML));
  result.push_back(
      std::make_pair("polymer/core-selector/core-selector-extracted.js",
                     IDR_POLYMER_CORE_SELECTOR_CORE_SELECTOR_EXTRACTED_JS));
  result.push_back(
      std::make_pair("polymer/core-selector/core-selector.html",
                     IDR_POLYMER_CORE_SELECTOR_CORE_SELECTOR_HTML));
  result.push_back(
      std::make_pair("polymer/core-style/core-style-extracted.js",
                     IDR_POLYMER_CORE_STYLE_CORE_STYLE_EXTRACTED_JS));
  result.push_back(std::make_pair("polymer/core-style/core-style.html",
                                  IDR_POLYMER_CORE_STYLE_CORE_STYLE_HTML));
  result.push_back(
      std::make_pair("polymer/core-transition/core-transition-extracted.js",
                     IDR_POLYMER_CORE_TRANSITION_CORE_TRANSITION_EXTRACTED_JS));
  result.push_back(
      std::make_pair("polymer/core-transition/core-transition.html",
                     IDR_POLYMER_CORE_TRANSITION_CORE_TRANSITION_HTML));
  result.push_back(std::make_pair("polymer/paper-button/paper-button.css",
                                  IDR_POLYMER_PAPER_BUTTON_PAPER_BUTTON_CSS));
  result.push_back(
      std::make_pair("polymer/paper-button/paper-button-extracted.js",
                     IDR_POLYMER_PAPER_BUTTON_PAPER_BUTTON_EXTRACTED_JS));
  result.push_back(std::make_pair("polymer/paper-button/paper-button.html",
                                  IDR_POLYMER_PAPER_BUTTON_PAPER_BUTTON_HTML));
  result.push_back(
      std::make_pair("polymer/paper-focusable/paper-focusable-extracted.js",
                     IDR_POLYMER_PAPER_FOCUSABLE_PAPER_FOCUSABLE_EXTRACTED_JS));
  result.push_back(
      std::make_pair("polymer/paper-focusable/paper-focusable.html",
                     IDR_POLYMER_PAPER_FOCUSABLE_PAPER_FOCUSABLE_HTML));
  result.push_back(
      std::make_pair("polymer/paper-progress/paper-progress.css",
                     IDR_POLYMER_PAPER_PROGRESS_PAPER_PROGRESS_CSS));
  result.push_back(
      std::make_pair("polymer/paper-progress/paper-progress-extracted.js",
                     IDR_POLYMER_PAPER_PROGRESS_PAPER_PROGRESS_EXTRACTED_JS));
  result.push_back(
      std::make_pair("polymer/paper-progress/paper-progress.html",
                     IDR_POLYMER_PAPER_PROGRESS_PAPER_PROGRESS_HTML));
  result.push_back(
      std::make_pair("polymer/paper-ripple/paper-ripple-extracted.js",
                     IDR_POLYMER_PAPER_RIPPLE_PAPER_RIPPLE_EXTRACTED_JS));
  result.push_back(std::make_pair("polymer/paper-ripple/paper-ripple.html",
                                  IDR_POLYMER_PAPER_RIPPLE_PAPER_RIPPLE_HTML));
  result.push_back(std::make_pair("polymer/paper-shadow/paper-shadow.css",
                                  IDR_POLYMER_PAPER_SHADOW_PAPER_SHADOW_CSS));
  result.push_back(
      std::make_pair("polymer/paper-shadow/paper-shadow-extracted.js",
                     IDR_POLYMER_PAPER_SHADOW_PAPER_SHADOW_EXTRACTED_JS));
  result.push_back(std::make_pair("polymer/paper-shadow/paper-shadow.html",
                                  IDR_POLYMER_PAPER_SHADOW_PAPER_SHADOW_HTML));
  result.push_back(std::make_pair("polymer/platform/platform.js",
                                  IDR_POLYMER_PLATFORM_PLATFORM_JS));
  result.push_back(std::make_pair("polymer/polymer/layout.html",
                                  IDR_POLYMER_POLYMER_LAYOUT_HTML));
  result.push_back(std::make_pair("polymer/polymer/polymer.html",
                                  IDR_POLYMER_POLYMER_POLYMER_HTML));
  result.push_back(std::make_pair("polymer/polymer/polymer.js",
                                  IDR_POLYMER_POLYMER_POLYMER_JS));
  return result;
}

}  // namespace chromeos
