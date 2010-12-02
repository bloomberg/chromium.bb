// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();

/**
 * Window onload handler, sets up the page.
 */
function load() {
  $('cancel-button').addEventListener('click', function(e) {
    window.close();
  });

  chrome.send('getPrinters');
  chrome.send('getPreview');
};

/**
 * Fill the printer list drop down.
 */
function setPrinters(printers) {
  if (printers.length > 0) {
    for (var i = 0; i < printers.length; ++i) {
      var option = document.createElement('option');
      option.textContent = printers[i];
      $('printer-list').add(option);
    }
  } else {
    var option = document.createElement('option');
    option.textContent = localStrings.getString('noPrinter');
    $('printer-list').add(option);
    $('printer-list').disabled = true;
    $('print-button').disabled = true;
  }
}

function onPDFLoad() {
  $('pdf-viewer').fitToHeight();
}

/**
 * Create the PDF plugin or reload the existing one.
 */
function createPDFPlugin(url) {
  if ($('pdf-viewer')) {
    pdfPlugin.reload();
    return;
  }

  var loadingElement = $('loading');
  var mainView = loadingElement.parentNode;
  mainView.removeChild(loadingElement);

  pdfPlugin = document.createElement('object');
  pdfPlugin.setAttribute('id', 'pdf-viewer');
  pdfPlugin.setAttribute('type', 'application/pdf');
  pdfPlugin.setAttribute('src', url);
  mainView.appendChild(pdfPlugin);
  pdfPlugin.onload('onPDFLoad()');
}

window.addEventListener('DOMContentLoaded', load);
