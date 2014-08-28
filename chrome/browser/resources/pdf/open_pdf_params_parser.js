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
  }
};
