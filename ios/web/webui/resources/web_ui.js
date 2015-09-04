// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. In particular, members that are to
// be accessed externally should be specified in this['style'] as opposed to
// this.style because member identifiers are minified by default.
// See http://goo.gl/FwOgy

window['chrome'] = window ['chrome'] || {};

window['chrome']['send'] = function(message, args) {
  __gCrWeb.message.invokeOnHost({'command': 'chrome.send',
                                 'message': '' + message,
                                 'arguments': args || []});
};

// Chrome defines bind on all functions, so this is expected to exist by
// webui's scripts.
Function.prototype.bind = function(context) {
  // Reference to the Function instance.
  var self = this;
  // Reference to the current arguments.
  var curriedArguments = [];
  for (var i = 1; i < arguments.length; i++)
    curriedArguments.push(arguments[i]);
  return function() {
    var finalArguments = [];
    for (var i = 0; i < curriedArguments.length; i++)
      finalArguments.push(curriedArguments[i]);
    for (var i = 0; i < arguments.length; i++)
      finalArguments.push(arguments[i]);
    return self.apply(context, finalArguments);
  }
};

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
