// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Creates a new OpenPDFParamsParser. This parses the open pdf parameters
 * passed in the url to set initial viewport settings for opening the pdf.
 * @param {string} url to be parsed.
 */
function OpenPDFParamsParser(url) {
  this.url_ = url;
  this.urlParams = {};
  this.parseOpenPDFParams_();
}

OpenPDFParamsParser.prototype = {
  /**
   * @private
   * Parse zoom parameter of open PDF parameters. If this
   * parameter is passed while opening PDF then PDF should be opened
   * at the specified zoom level.
   * @param {number} zoom value.
   */
  parseZoomParam_: function(paramValue) {
    var paramValueSplit = paramValue.split(',');
    if ((paramValueSplit.length != 1) && (paramValueSplit.length != 3))
      return;

    // User scale of 100 means zoom value of 100% i.e. zoom factor of 1.0.
    var zoomFactor = parseFloat(paramValueSplit[0]) / 100;
    if (isNaN(zoomFactor))
      return;

    // Handle #zoom=scale.
    if (paramValueSplit.length == 1) {
      this.urlParams['zoom'] = zoomFactor;
      return;
    }

    // Handle #zoom=scale,left,top.
    var position = {x: parseFloat(paramValueSplit[1]),
                    y: parseFloat(paramValueSplit[2])};
    this.urlParams['position'] = position;
    this.urlParams['zoom'] = zoomFactor;
  },

  /**
   * @private
   * Parse open PDF parameters. These parameters are mentioned in the url
   * and specify actions to be performed when opening pdf files.
   * See http://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/
   * pdfs/pdf_open_parameters.pdf for details.
   */
  parseOpenPDFParams_: function() {
    var originalUrl = this.url_;
    var paramIndex = originalUrl.search('#');
    if (paramIndex == -1)
      return;

    var paramTokens = originalUrl.substring(paramIndex + 1).split('&');
    var paramsDictionary = {};
    for (var i = 0; i < paramTokens.length; ++i) {
      var keyValueSplit = paramTokens[i].split('=');
      if (keyValueSplit.length != 2)
        continue;
      paramsDictionary[keyValueSplit[0]] = keyValueSplit[1];
    }

    if ('page' in paramsDictionary) {
      // |pageNumber| is 1-based, but goToPage() take a zero-based page number.
      var pageNumber = parseInt(paramsDictionary['page']);
      if (!isNaN(pageNumber))
        this.urlParams['page'] = pageNumber - 1;
    }

    if ('zoom' in paramsDictionary)
      this.parseZoomParam_(paramsDictionary['zoom']);
  }
};
