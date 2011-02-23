// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();
var hasPDFPlugin = true;
var expectedPageCount = 0;

/**
 * Window onload handler, sets up the page.
 */
function load() {
  $('print-button').addEventListener('click', function(e) {
    chrome.send('print');
  });
  $('cancel-button').addEventListener('click', function(e) {
    window.close();
  });

  chrome.send('getPrinters');
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
function createPDFPlugin(url, pagesCount) {
  if (!hasPDFPlugin) {
    return;
  }
  // Set the expected pages count.
  if (expectedPageCount != pagesCount) {
    expectedPageCount = pagesCount;
    // Set the initial page range text.
    $('pages').value = '1-' + expectedPageCount;
  }

  // Enable the print button.
  $('print-button').disabled = false;

  if ($('pdf-viewer')) {
    $('pdf-viewer').reload();
    return;
  }

  var loadingElement = $('loading');
  loadingElement.classList.add('hidden');
  var mainView = loadingElement.parentNode;

  var pdfPlugin = document.createElement('object');
  pdfPlugin.setAttribute('id', 'pdf-viewer');
  pdfPlugin.setAttribute('type', 'application/pdf');
  pdfPlugin.setAttribute('src', url);
  mainView.appendChild(pdfPlugin);
  if (!pdfPlugin.onload) {
    hasPDFPlugin = false;
    mainView.removeChild(pdfPlugin);
    $('no-plugin').classList.remove('hidden');
    return;
  }
  pdfPlugin.onload('onPDFLoad()');
}

window.addEventListener('DOMContentLoaded', load);
