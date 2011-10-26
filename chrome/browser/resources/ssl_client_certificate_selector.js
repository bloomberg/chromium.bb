// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('sslClientCertificateSelector', function() {
  'use strict';

  /**
   * Disables the controls while the dialog is busy.
   */
  function disableControls() {
    $('info').disabled = true;
    $('cancel').disabled = true;
    $('ok').disabled = true;
  }

  /**
   * Closes the dialog and passes a result value to the dialog close handler.
   * @param {number|null} result The value to pass to the dialog close handler,
   *     representing the index of the certificate to use or null if it is a
   *     Cancel operation.
   */
  function closeWithResult(result) {
    disableControls();
    var json = JSON.stringify([result]);
    chrome.send('DialogClose', [json]);
  }

  var details;

  /**
   * Updates the details area using the currently selected certificate.
   */
  function updateDetails() {
    var index = $('certificates').value;
    $('details').textContent = details[index];
  }

  /**
   * Gets the selected certificate index.
   * @return {number} The index of the selected certificate.
   */
  function selectedCertificateIndex() {
    return Number($('certificates').value);
  }

  /**
   * Shows the certificate viewer for the selected certificate.
   */
  function viewCertificate() {
    chrome.send('viewCertificate', [selectedCertificateIndex()]);
  }

  /**
   * Inserts translated strings on loading.
   */
  function initialize() {
    i18nTemplate.process(document, templateData);
    $('info').onclick = viewCertificate;
    $('cancel').onclick = function() {
      closeWithResult();  // No arguments means cancel.
    };
    $('ok').onclick = function() {
      closeWithResult(selectedCertificateIndex());
    };
    $('certificates').onchange = updateDetails;
    chrome.send('requestDetails');
  }

  /**
   * Adds elements to the DOM to populate the tab contents list box area of the
   * dialog. Substitutes the favicon source and title text from the details
   * using a template mechanism (clones hidden parts of the dialog DOM).
   * @param {{site: string, certificates: List<string>, details:
   *     List<string>}} dict Specifies the site (that is requesting a
   *     certificate), the display names of the certificates, and the details of
   *     the certificates.
   */
  function setDetails(dict) {
    if (dict.site) {
      $('site').textContent = dict.site;
    }
    var certificates = dict.certificates;
    if (certificates) {
      var numCertificates = certificates.length;
      var selectElement = $('certificates');
      for (var i = 0; i < numCertificates; i++) {
        var certificate = certificates[i];
        var optionElement = document.createElement('option');
        optionElement.value = i;
        optionElement.textContent = certificate;
        selectElement.appendChild(optionElement);
      }
    }
    if (dict.details) {
      details = dict.details;
    }
    updateDetails();
  }

  return {
    initialize: initialize,
    setDetails: setDetails,
  };
});

document.addEventListener('DOMContentLoaded',
                          sslClientCertificateSelector.initialize);
