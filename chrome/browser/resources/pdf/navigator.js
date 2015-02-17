// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Creates a new Navigator for navigating to links inside or outside the PDF.
 * @param {string} originalUrl The original page URL.
 * @param {Object} viewport The viewport info of the page.
 * @param {Object} paramsParser The object for URL parsing.
 * @param {Function} navigateInCurrentTabCallback The Callback function that
 *    gets called when navigation happens in the current tab.
 * @param {Function} navigateInNewTabCallback The Callback function that gets
 *    called when navigation happens in the new tab.
 */
function Navigator(originalUrl,
                   viewport,
                   paramsParser,
                   navigateInCurrentTabCallback,
                   navigateInNewTabCallback) {
  this.originalUrl_ = originalUrl;
  this.viewport_ = viewport;
  this.paramsParser_ = paramsParser;
  this.navigateInCurrentTabCallback_ = navigateInCurrentTabCallback;
  this.navigateInNewTabCallback_ = navigateInNewTabCallback;
}

Navigator.prototype = {
   /**
   * @private
   * Function to navigate to the given URL. This might involve navigating
   * within the PDF page or opening a new url (in the same tab or a new tab).
   * @param {string} url The URL to navigate to.
   * @param {boolean} newTab Whether to perform the navigation in a new tab or
   *    in the current tab.
   */
  navigate: function(url, newTab) {
    if (url.length == 0)
      return;
    // If |urlFragment| starts with '#', then it's for the same URL with a
    // different URL fragment.
    if (url.charAt(0) == '#') {
      // if '#' is already present in |originalUrl| then remove old fragment
      // and add new url fragment.
      var hashIndex = this.originalUrl_.search('#');
      if (hashIndex != -1)
        url = this.originalUrl_.substring(0, hashIndex) + url;
      else
        url = this.originalUrl_ + url;
    }

    // If there's no scheme, add http.
    if (url.indexOf('://') == -1 && url.indexOf('mailto:') == -1)
      url = 'http://' + url;

    // Make sure inputURL starts with a valid scheme.
    if (url.indexOf('http://') != 0 &&
        url.indexOf('https://') != 0 &&
        url.indexOf('ftp://') != 0 &&
        url.indexOf('file://') != 0 &&
        url.indexOf('mailto:') != 0) {
      return;
    }
    // Make sure inputURL is not only a scheme.
    if (url == 'http://' ||
        url == 'https://' ||
        url == 'ftp://' ||
        url == 'file://' ||
        url == 'mailto:') {
      return;
    }

    if (newTab) {
      this.navigateInNewTabCallback_(url);
    } else {
      this.paramsParser_.getViewportFromUrlParams(
          url, this.onViewportReceived_.bind(this));
    }
  },

   /**
   * @private
   * Called when the viewport position is received.
   * @param {Object} viewportPosition Dictionary containing the viewport
   *    position.
   */
  onViewportReceived_: function(viewportPosition) {
    var pageNumber = viewportPosition.page;
    if (pageNumber != undefined)
      this.viewport_.goToPage(pageNumber);
    else
      this.navigateInCurrentTabCallback_(viewportPosition['url']);
  }
};
