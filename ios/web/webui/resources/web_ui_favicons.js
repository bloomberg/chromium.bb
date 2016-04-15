// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. In particular, members that are to
// be accessed externally should be specified in this['style'] as opposed to
// this.style because member identifiers are minified by default.
// See http://goo.gl/FwOgy

goog.require('__crWeb.webUIBase');

goog.provide('__crWeb.webUIFavicons');

/**
 * Sends message requesting favicon at the URL from imageSet for el. Sets
 * favicon-url attribute on el to the favicon URL.
 * @param {Element} el The DOM element to request the favicon for.
 * @param {string} imageSet The CSS -webkit-image-set.
 */
window['chrome']['requestFavicon'] = function(el, imageSet) {
  var cssUrls = imageSet.match(/url\([^\)]+\) \dx/g);
  // TODO(jyquinn): Review match above (crbug.com/528080).
  if (!cssUrls) {
    return;
  }
  // Extract url from CSS -webkit-image-set.
  var faviconUrl = '';
  for (var i = 0; i < cssUrls.length; ++i) {
    var scaleFactorExp = /(\d)x$/;
    var scaleFactor = getSupportedScaleFactors()[0];
    if (parseInt(scaleFactorExp.exec(cssUrls[i])[1], 10) === scaleFactor) {
      var urlExp = /url\(\"(.+)\"\)/;
      faviconUrl = urlExp.exec(cssUrls[i])[1];
      break;
    }
  }
  el.setAttribute('favicon-url', url(faviconUrl));
  chrome.send('webui.requestFavicon', [faviconUrl]);
};

/**
 * Called to set elements with favicon-url attribute equal to faviconUrl to
 * provided dataUrl.
 * @param {string} faviconUrl Favicon URL used to locate favicon element
 *     via the favicon-url attribute.
 * @param {string} dataUrl The data URL to assign to the favicon element's
 *    backgroundImage.
 */
window['chrome']['setFaviconBackground'] = function(faviconUrl, dataUrl) {
  var selector = "[favicon-url='" + url(faviconUrl) + "']";
  var elements = document.querySelectorAll(selector);
  for (var i = 0; i < elements.length; ++i) {
    elements[i].style.backgroundImage = url(dataUrl);
  }
};
