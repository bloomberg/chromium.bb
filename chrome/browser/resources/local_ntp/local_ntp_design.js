// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Specifications for the NTP design, and an accessor to presets.
 */


/**
 * Specifications for an NTP design (not comprehensive).
 *
 * name: A unique identifier for the style.
 *   appropriate CSS will take effect.
 * fontFamily: Font family to use for title and thumbnail <iframe>s.
 * fontSize: Font size to use for the <iframe>s, in px.
 * tileWidth: The width of each suggestion tile, in px.
 * tileMargin: Spacing between successive tiles, in px.
 * titleColor: The RRGGBB color of title text.
 * titleTextAlign: (Optional) The alignment of title text. If unspecified, the
 *   default value is 'center'.
 * titleTextFade: (Optional) The number of pixels beyond which title
 *   text begins to fade. This overrides the default ellipsis style.
 * thumbnailTextColor: The RRGGBB color that thumbnail <iframe> may use to
 *   display text message in place of missing thumbnail.
 *
 * @typedef {{
 *   name: string,
 *   fontFamily: string,
 *   fontSize: number,
 *   tileWidth: number,
 *   tileMargin: number,
 *   titleColor: string,
 *   titleTextAlign: string|null|undefined,
 *   titleTextFade: string|null|undefined,
 *   thumbnailTextColor: string
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
  // TODO(huangs): Add new style.
  return {
    name: 'classical',
    fontFamily: 'arial, sans-serif',
    fontSize: 11,
    tileWidth: 140,
    tileMargin: 20,
    titleColor: '777777',
    // No titleTextAlign: defaults to 'center'.
    // No titleTextFade: by default we have ellipsis.
    thumbnailTextColor: '777777'
  };
}
