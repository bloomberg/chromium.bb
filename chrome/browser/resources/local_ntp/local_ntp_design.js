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
 * fontFamily: Font family to use for title and thumbnail iframes.
 * fontSize: Font size to use for the iframes, in px.
 * tileWidth: The width of each suggestion tile, in px.
 * tileMargin: Spacing between successive tiles, in px.
 * titleColor: The RRGGBBAA color of title text.
 * titleColorAgainstDark: The RRGGBBAA color of title text against a dark theme.
 * titleTextAlign: (Optional) The alignment of title text. If unspecified, the
 *   default value is 'center'.
 * titleTextFade: (Optional) The number of pixels beyond which title
 *   text begins to fade. This overrides the default ellipsis style.
 * thumbnailTextColor: The RRGGBBAA color that thumbnail iframe may use to
 *   display text message in place of missing thumbnail.
 * thumbnailFallback: (Optional) A value in THUMBNAIL_FALLBACK to specify the
 *   thumbnail fallback strategy. If unassigned, then the thumbnail.html
 *   iframe would handle the fallback.
 *
 * @const {{
 *   fontFamily: string,
 *   fontSize: number,
 *   tileWidth: number,
 *   tileMargin: number,
 *   titleColor: string,
 *   titleColorAgainstDark: string,
 *   titleTextAlign: string|null|undefined,
 *   TODO(huangs): Clean-up certain parameters once previous design is no longer
 *       used on server.
 *   titleTextFade: number|null|undefined,
 *   thumbnailTextColor: string,
 *   thumbnailFallback: string|null|undefined
 * }}
 */
var NTP_DESIGN = {
  fontFamily: 'arial, sans-serif',
  fontSize: 12,
  tileWidth: 156,
  tileMargin: 16,
  titleColor: '323232ff',
  titleColorAgainstDark: 'd2d2d2ff',
  titleTextAlign: 'inherit',
  titleTextFade: 122 - 36,  // 112px wide title with 32 pixel fade at end.
  thumbnailTextColor: '323232ff',  // Unused.
  thumbnailFallback: THUMBNAIL_FALLBACK.DOT
};
