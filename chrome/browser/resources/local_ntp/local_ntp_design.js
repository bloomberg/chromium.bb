// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Specifications for NTP design, and an accessor to presets.
 */

 var THUMBNAIL_FALLBACK = {
   DOT: 'dot'  // Draw single dot.
 };

/**
 * Specifications for an NTP design (not comprehensive).
 *
 * name: A unique identifier for the style.
 * fontFamily: Font family to use for title and thumbnail <iframe>s.
 * fontSize: Font size to use for the <iframe>s, in px.
 * tileWidth: The width of each suggestion tile, in px.
 * tileMargin: Spacing between successive tiles, in px.
 * titleColor: The RRGGBBAA color of title text.
 * titleColorAgainstDark: The RRGGBBAA color of title text against a dark theme.
 * titleTextAlign: (Optional) The alignment of title text. If unspecified, the
 *   default value is 'center'.
 * titleTextFade: (Optional) The number of pixels beyond which title
 *   text begins to fade. This overrides the default ellipsis style.
 * thumbnailTextColor: The RRGGBBAA color that thumbnail <iframe> may use to
 *   display text message in place of missing thumbnail.
 * thumbnailFallback: (Optional) A value in THUMBNAIL_FALLBACK to specify the
 *   thumbnail fallback strategy. If unassigned, then the thumbnail.html
 *   <iframe> would handle the fallback.
 * showFakeboxHint: Whether to display text in the fakebox.
 *
 * @typedef {{
 *   name: string,
 *   fontFamily: string,
 *   fontSize: number,
 *   tileWidth: number,
 *   tileMargin: number,
 *   titleColor: string,
 *   titleColorAgainstDark: string,
 *   titleTextAlign: string|null|undefined,
 *   titleTextFade: string|null|undefined,
 *   thumbnailTextColor: string,
 *   thumbnailFallback: string|null|undefined
 *   showFakeboxHint: string|null|undefined
 * }}
 */
var NtpDesign;

/**
 * Returns an NTP design corresponding to the given name.
 * @param {string|undefined} opt_name The name of the design. If undefined, then
 *   the default design is specified.
 * @return {NtpDesign} The NTP design corresponding to name.
 */
function getNtpDesign(opt_name) {
  var ntpDesign = null;

  if (opt_name === 'md') {
    ntpDesign = {
      name: opt_name,
      fontFamily: 'arial, sans-serif',
      fontSize: 12,
      tileWidth: 156,
      tileMargin: 16,
      titleColor: '323232ff',
      titleColorAgainstDark: 'd2d2d2ff',
      titleTextAlign: 'inherit',
      titleTextFade: 122 - 36,  // 112px wide title with 32 pixel fade at end.
      thumbnailTextColor: '323232ff',  // Unused.
      thumbnailFallback: THUMBNAIL_FALLBACK.DOT,
      showFakeboxHint: true
    };
  } else {
    ntpDesign = {
      name: 'classical',
      fontFamily: 'arial, sans-serif',
      fontSize: 11,
      tileWidth: 140,
      tileMargin: 20,
      titleColor: '777777ff',
      titleColorAgainstDark: '777777ff',
      titleTextAlign: 'center',
      titleTextFade: null,  // Default to ellipsis.
      thumbnailTextColor: '777777ff',
      thumbnailFallback: null,  // Default to false.
      showFakeboxHint: false
    };
  }
  return ntpDesign;
}
