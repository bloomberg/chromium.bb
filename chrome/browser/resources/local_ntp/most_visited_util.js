// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Utilities for rendering most visited thumbnails and titles.
 */

<include src="instant_iframe_validation.js">


/**
 * Parses query parameters from Location.
 * @param {string} location The URL to generate the CSS url for.
 * @return {object} Dictionary containing name value pairs for URL
 */
function parseQueryParams(location) {
  var params = Object.create(null);
  var query = decodeURI(location.search.substring(1));
  var vars = query.split('&');
  for (var i = 0; i < vars.length; i++) {
    var pair = vars[i].split('=');
    var k = pair[0];
    if (k in params) {
      // Duplicate parameters are not allowed to prevent attackers who can
      // append things to |location| from getting their parameter values to
      // override legitimate ones.
      return Object.create(null);
    } else {
      params[k] = pair[1];
    }
  }
  return params;
}


/**
 * Creates a new most visited link element.
 * @param {Object} params URL parameters containing styles for the link.
 * @param {string} href The destination for the link.
 * @param {string} title The title for the link.
 * @param {string|undefined} text The text for the link or none.
 * @return {HTMLAnchorElement} A new link element.
 */
function createMostVisitedLink(params, href, title, text) {
  var styles = getMostVisitedStyles(params);
  var link = document.createElement('a');
  link.style.color = styles.color;
  link.style.fontSize = styles.fontSize + 'px';
  if (styles.fontFamily)
    link.style.fontFamily = styles.fontFamily;
  if (styles.textShadow)
    link.style.textShadow = styles.textShadow;
  link.href = href;
  link.title = title;
  link.target = '_top';
  if (text)
    link.textContent = text;
  return link;
}


/**
 * Decodes most visited styles from URL parameters.
 * - f: font-family
 * - fs: font-size as a number in pixels.
 * - c: A hexadecimal number interpreted as a hex color code.
 * - ts: Truthy iff text should be drawn with a shadow.
 * @param {Object.<string, string>} params URL parameters specifying style.
 * @return {Object} Styles suitable for CSS interpolation.
 */
function getMostVisitedStyles(params) {
  var styles = {
    color: '#777',
    fontFamily: '',
    fontSize: 11,
    textShadow: ''
  };
  if ('f' in params && /^[-0-9a-zA-Z ,]+$/.test(params.f))
    styles.fontFamily = params.f;
  if ('fs' in params && isFinite(parseInt(params.fs, 10)))
    styles.fontSize = parseInt(params.fs, 10);
  if ('c' in params)
    styles.color = convertColor(parseInt(params.c, 16)) || styles.color;
  if ('ts' in params && params.ts)
    styles.textShadow = 'black 0 1px 3px';
  return styles;
}


/**
 * @param {string} location A location containing URL parameters.
 * @param {function(Object, Object)} fill A function called with styles and
 *     data to fill.
 */
function fillMostVisited(location, fill) {
  var params = parseQueryParams(document.location);
  params.rid = parseInt(params.rid, 10);
  if (!isFinite(params.rid))
    return;
  var apiHandle = chrome.embeddedSearch.searchBox;
  var data = apiHandle.getMostVisitedItemData(params.rid);
  if (!data)
    return;
  if (/^javascript:/i.test(data.url))
    return;
  if (data.direction)
    document.body.dir = data.direction;
  fill(params, data);
}
