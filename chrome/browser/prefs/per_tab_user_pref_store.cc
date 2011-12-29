// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/per_tab_user_pref_store.h"
#include "chrome/common/pref_names.h"

PerTabUserPrefStore::PerTabUserPrefStore(PersistentPrefStore* underlay)
    : OverlayUserPrefStore(underlay) {
  RegisterOverlayProperty(
      prefs::kWebKitJavascriptEnabled,
      prefs::kWebKitGlobalJavascriptEnabled);
  RegisterOverlayProperty(
      prefs::kWebKitJavascriptCanOpenWindowsAutomatically,
      prefs::kWebKitGlobalJavascriptCanOpenWindowsAutomatically);
  RegisterOverlayProperty(
      prefs::kWebKitLoadsImagesAutomatically,
      prefs::kWebKitGlobalLoadsImagesAutomatically);
  RegisterOverlayProperty(
      prefs::kWebKitPluginsEnabled,
      prefs::kWebKitGlobalPluginsEnabled);
  RegisterOverlayProperty(
      prefs::kDefaultCharset,
      prefs::kGlobalDefaultCharset);
  RegisterOverlayProperty(
      prefs::kWebKitStandardFontFamily,
      prefs::kWebKitGlobalStandardFontFamily);
  RegisterOverlayProperty(
      prefs::kWebKitFixedFontFamily,
      prefs::kWebKitGlobalFixedFontFamily);
  RegisterOverlayProperty(
      prefs::kWebKitSerifFontFamily,
      prefs::kWebKitGlobalSerifFontFamily);
  RegisterOverlayProperty(
      prefs::kWebKitSansSerifFontFamily,
      prefs::kWebKitGlobalSansSerifFontFamily);
  RegisterOverlayProperty(
      prefs::kWebKitCursiveFontFamily,
      prefs::kWebKitGlobalCursiveFontFamily);
  RegisterOverlayProperty(
      prefs::kWebKitFantasyFontFamily,
      prefs::kWebKitGlobalFantasyFontFamily);
  RegisterOverlayProperty(
      prefs::kWebKitDefaultFontSize,
      prefs::kWebKitGlobalDefaultFontSize);
  RegisterOverlayProperty(
      prefs::kWebKitDefaultFixedFontSize,
      prefs::kWebKitGlobalDefaultFixedFontSize);
  RegisterOverlayProperty(
      prefs::kWebKitMinimumFontSize,
      prefs::kWebKitGlobalMinimumFontSize);
  RegisterOverlayProperty(
      prefs::kWebKitMinimumLogicalFontSize,
      prefs::kWebKitGlobalMinimumLogicalFontSize);
}
