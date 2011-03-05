// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();
var hasPDFPlugin = true;
var expectedPageCount = 0;

/**
 * Window onload handler, sets up the page.
 */
function load() {
  $('print-button').addEventListener('click', printFile);

  $('cancel-button').addEventListener('click', function(e) {
    window.close();
  });

  chrome.send('getPrinters');
};

/**
 * Page range text validation.
 * Returns true if |printFromText| and |printToText| are valid page numbers.
 * TODO (kmadhusu): Get the expected page count and validate the page range
 * with total number of pages.
 */
function isValidPageRange(printFromText, printToText) {
  var numericExp = /^[0-9]+$/;
  if (numericExp.test(printFromText) && numericExp.test(printToText)) {
    var printFrom = Number(printFromText);
    var printTo = Number(printToText);
    if (printFrom <= printTo && printFrom != 0 && printTo != 0)
      return true;
  }
  return false;
}

/**
 * Parse page range text.
 * Eg: If page range is specified as '1-3,7-9,8'. Create an array with three
 * elements. Each array element contains the range information.
 * [{from:1, to:3}, {from:7, to:9}, {from:8, to:8}]
 * TODO (kmadhusu): Handle invalid characters.
 */
function getPageRanges() {
  var pageRangesInfo = [];
  var pageRangeText = $('pages').value;
  var pageRangeList = pageRangeText.replace(/\s/g, '').split(',');
  for (var i = 0; i < pageRangeList.length; i++) {
    var tempRange = pageRangeList[i].split('-');
    var printFrom = tempRange[0];
    var printTo;
    if (tempRange.length > 1)
      printTo = tempRange[1];
    else
      printTo = tempRange[0];
    // Validate the page range information.
    if (isValidPageRange(printFrom, printTo)) {
      pageRangesInfo.push({'from': parseInt(printFrom, 10),
                           'to': parseInt(printTo, 10)});
    }
  }
  return pageRangesInfo;
}

function printFile() {
  var selectedPrinter = $('printer-list').selectedIndex;
  var printerName = $('printer-list').options[selectedPrinter].textContent;
  var pageRanges = getPageRanges();
  var printAll = $('all-pages').checked;
  var twoSided = $('two-sided').checked;
  var copies = $('copies').value;
  var collate = $('collate').checked;
  var layout = $('layout').options[$('layout').selectedIndex].value;
  var color = $('color').options[$('color').selectedIndex].value;

  var jobSettings = JSON.stringify({'printerName': printerName,
                                    'pageRange': pageRanges,
                                    'printAll': printAll,
                                    'twoSided': twoSided,
                                    'copies': copies,
                                    'collate': collate,
                                    'layout': layout,
                                    'color': color});
  chrome.send('print', [jobSettings]);
}

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
