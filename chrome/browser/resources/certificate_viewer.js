// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cert_viewer', function() {
  'use strict';

  /**
   * Initialize the certificate viewer dialog by wiring up the close button,
   * substituting in translated strings and requesting certificate details.
   */
  function initialize() {
    $('close').onclick = function() {
      window.close();
    }
    var args = JSON.parse(chrome.dialogArguments);
    i18nTemplate.process(document, templateData);
    chrome.send('requestCertificateInfo', [args.cert]);
  }

  /**
   * This function is called from certificate_viewer_ui.cc with the certificate
   * information. Display all returned information to the user.
   * @param {Object} certInfo Certificate information in named fields.
   */
  function getCertificateInfo(certInfo) {
    for (var key in certInfo) {
      $(key).textContent = certInfo[key];
    }
  }

  return {
    initialize: initialize,
    getCertificateInfo: getCertificateInfo,
  };
});

document.addEventListener('DOMContentLoaded', cert_viewer.initialize);
