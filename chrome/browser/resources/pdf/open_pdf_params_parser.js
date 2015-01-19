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
  // A dictionary of all the named destinations in the PDF.
  this.namedDestinations = {};
}

OpenPDFParamsParser.prototype = {
  /**
   * @private
   * Parse zoom parameter of open PDF parameters. If this
   * parameter is passed while opening PDF then PDF should be opened
   * at the specified zoom level.
   * @param {number} zoom value.
   * @param {Object} viewportPosition to store zoom and position value.
   */
  parseZoomParam_: function(paramValue, viewportPosition) {
    var paramValueSplit = paramValue.split(',');
    if ((paramValueSplit.length != 1) && (paramValueSplit.length != 3))
      return;

    // User scale of 100 means zoom value of 100% i.e. zoom factor of 1.0.
    var zoomFactor = parseFloat(paramValueSplit[0]) / 100;
    if (isNaN(zoomFactor))
      return;

    // Handle #zoom=scale.
    if (paramValueSplit.length == 1) {
      viewportPosition['zoom'] = zoomFactor;
      return;
    }

    // Handle #zoom=scale,left,top.
    var position = {x: parseFloat(paramValueSplit[1]),
                    y: parseFloat(paramValueSplit[2])};
    viewportPosition['position'] = position;
    viewportPosition['zoom'] = zoomFactor;
  },

  /**
   * @private
   * Parse PDF url parameters. These parameters are mentioned in the url
   * and specify actions to be performed when opening pdf files.
   * See http://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/
   * pdfs/pdf_open_parameters.pdf for details.
   * @param {string} url that needs to be parsed.
   * @return {Object} A dictionary containing the viewport which should be
   * displayed based on the URL.
   */
  getViewportFromUrlParams: function(url) {
    var viewportPosition = {};
    var paramIndex = url.search('#');
    if (paramIndex == -1)
      return viewportPosition;

    var paramTokens = url.substring(paramIndex + 1).split('&');
    if ((paramTokens.length == 1) && (paramTokens[0].search('=') == -1)) {
      // Handle the case of http://foo.com/bar#NAMEDDEST. This is not
      // explicitlymentioned except by example in the Adobe
      // "PDF Open Parameters" document.
      viewportPosition['page'] = this.namedDestinations[paramTokens[0]];
      return viewportPosition;
    }

    var paramsDictionary = {};
    for (var i = 0; i < paramTokens.length; ++i) {
      var keyValueSplit = paramTokens[i].split('=');
      if (keyValueSplit.length != 2)
        continue;
      paramsDictionary[keyValueSplit[0]] = keyValueSplit[1];
    }

    if ('nameddest' in paramsDictionary) {
      var page = this.namedDestinations[paramsDictionary['nameddest']];
      if (page != undefined)
        viewportPosition['page'] = page;
    }

    if ('page' in paramsDictionary) {
      // |pageNumber| is 1-based, but goToPage() take a zero-based page number.
      var pageNumber = parseInt(paramsDictionary['page']);
      if (!isNaN(pageNumber) && pageNumber > 0)
        viewportPosition['page'] = pageNumber - 1;
    }

    if ('zoom' in paramsDictionary)
      this.parseZoomParam_(paramsDictionary['zoom'], viewportPosition);

    return viewportPosition;
  }
};
