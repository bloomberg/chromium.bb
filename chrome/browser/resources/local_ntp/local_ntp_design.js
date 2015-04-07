// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Specifications for NTP design.
 */

 var THUMBNAIL_FALLBACK = {
   DOT: 'dot'  // Draw single dot.
 };


/**
 * Specifications for an NTP design (not comprehensive).
 *
 * fakeboxWingSize: Extra distance for fakebox to extend beyond beyond the list
 *   of tiles.
 * fontFamily: Font family to use for title and thumbnail iframes.
 * fontSize: Font size to use for the iframes, in px.
 * mainClass: Class applied to #ntp-contents to control CSS.
 * numTitleLines: Number of lines to display in titles.
 * showFavicon: Whether to show favicon.
 * thumbnailTextColor: The 4-component color that thumbnail iframe may use to
 *   display text message in place of missing thumbnail.
 * thumbnailFallback: (Optional) A value in THUMBNAIL_FALLBACK to specify the
 *   thumbnail fallback strategy. If unassigned, then the thumbnail.html
 *   iframe would handle the fallback.
 * tileWidth: The width of each suggestion tile, in px.
 * tileMargin: Spacing between successive tiles, in px.
 * titleColor: The 4-component color of title text.
 * titleColorAgainstDark: The 4-component color of title text against a dark
 *   theme.
 * titleTextAlign: (Optional) The alignment of title text. If unspecified, the
 *   default value is 'center'.
 * titleTextFade: (Optional) The number of pixels beyond which title
 *   text begins to fade. This overrides the default ellipsis style.
 *
 * @const {{
 *   fakeboxWingSize: number,
 *   fontFamily: string,
 *   fontSize: number,
 *   mainClass: string,
 *   numTitleLines: number,
 *   showFavicon: boolean,
 *   thumbnailTextColor: string,
 *   thumbnailFallback: string|null|undefined,
 *   tileWidth: number,
 *   tileMargin: number,
 *   titleColor: string,
 *   titleColorAgainstDark: string,
 *   titleTextAlign: string|null|undefined,
 *   titleTextFade: number|null|undefined
 * }}
 */
var NTP_DESIGN = {
  fakeboxWingSize: 0,
  fontFamily: 'arial, sans-serif',
  fontSize: 12,
  mainClass: 'thumb-ntp',
  numTitleLines: 1,
  showFavicon: true,
  thumbnailTextColor: [50, 50, 50, 255],
  thumbnailFallback: THUMBNAIL_FALLBACK.DOT,
  tileWidth: 156,
  tileMargin: 16,
  titleColor: [50, 50, 50, 255],
  titleColorAgainstDark: [210, 210, 210, 255],
  titleTextAlign: 'inherit',
  titleTextFade: 122 - 36  // 112px wide title with 32 pixel fade at end.
};


/**
 * Modifies NTP_DESIGN parameters for icon NTP.
 */
function modifyNtpDesignForIcons() {
  NTP_DESIGN.fakeboxWingSize = 132;
  NTP_DESIGN.mainClass = 'icon-ntp';
  NTP_DESIGN.numTitleLines = 2;
  NTP_DESIGN.showFavicon = false;
  NTP_DESIGN.thumbnailFallback = null;
  NTP_DESIGN.tileWidth = 48 + 2 * 18;
  NTP_DESIGN.tileMargin = 60 - 18 * 2;
  NTP_DESIGN.titleColor = [120, 120, 120, 255];
  NTP_DESIGN.titleColorAgainstDark = [210, 210, 210, 255];
  NTP_DESIGN.titleTextAlign = 'center';
  delete NTP_DESIGN.titleTextFade;
}
