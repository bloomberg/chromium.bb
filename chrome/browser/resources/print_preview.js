// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();
var hasPDFPlugin = true;
var expectedPageCount = 0;
var pageRangesInfo = [];

/**
 * Window onload handler, sets up the page.
 */
function load() {
  $('printer-list').disabled = true;
  $('print-button').disabled = true;

  $('print-button').addEventListener('click', printFile);

  $('cancel-button').addEventListener('click', function(e) {
    window.close();
  });

  $('pages').addEventListener('input', selectPageRange);
  $('pages').addEventListener('blur', validatePageRangeInfo);
  $('all-pages').addEventListener('click', handlePrintAllPages);
  $('print-pages').addEventListener('click', validatePageRangeInfo);

  chrome.send('getPrinters');
  chrome.send('getPreview');
};

/**
 * Validates the 'from' and 'to' values of page range.
 * @param {string} printFromText The 'from' value of page range.
 * @param {string} printToText The 'to' value of page range.
 * @return {boolean} true if the page range is valid else returns false.
 */
function isValidPageRange(printFromText, printToText) {
  var numericExp = /^[0-9]+$/;
  if (numericExp.test(printFromText) && numericExp.test(printToText)) {
    var printFrom = Number(printFromText);
    var printTo = Number(printToText);
    if (printFrom <= printTo && printFrom != 0 && printTo != 0 &&
        printTo <= expectedPageCount) {
      return true;
    }
  }
  return false;
}

/**
 * Parses the given page range text and populates |pageRangesInfo| array.
 *
 * If the page range text is valid, this function populates the |pageRangesInfo|
 * array with page range objects and returns true. Each page range object has
 * 'from' and 'to' fields with values.
 *
 * If the page range text is invalid, returns false.
 *
 * E.g.: If the page range text is specified as '1-3,7-9,8', create an array
 * with three objects [{from:1, to:3}, {from:7, to:9}, {from:8, to:8}].
 *
 * @return {boolean} true if page range text parsing is successful else false.
 */
function parsePageRanges() {
  var pageRangeText = $('pages').value;
  var pageRangeList = pageRangeText.replace(/\s/g, '').split(',');
  pageRangesInfo = [];
  for (var i = 0; i < pageRangeList.length; i++) {
    var tempRange = pageRangeList[i].split('-');
    var tempRangeLen = tempRange.length;
    var printFrom = tempRange[0];
    var printTo;
    if (tempRangeLen > 2)
      return false;  // Invalid page range (E.g.: 1-2-3).
    else if (tempRangeLen > 1)
      printTo = tempRange[1];
    else
      printTo = tempRange[0];

    // Validate the page range information.
    if (!isValidPageRange(printFrom, printTo))
      return false;

    pageRangesInfo.push({'from': parseInt(printFrom, 10),
                         'to': parseInt(printTo, 10)});
  }
  return true;
}

function printFile() {
  var selectedPrinter = $('printer-list').selectedIndex;
  var printerName = $('printer-list').options[selectedPrinter].textContent;
  var printAll = $('all-pages').checked;
  var twoSided = $('two-sided').checked;
  var copies = $('copies').value;
  var collate = $('collate').checked;
  var landscape = ($('layout').options[$('layout').selectedIndex].value == "1");
  var color = ($('color').options[$('color').selectedIndex].value == "1");

  var jobSettings = JSON.stringify({'printerName': printerName,
                                    'pageRange': pageRangesInfo,
                                    'printAll': printAll,
                                    'twoSided': twoSided,
                                    'copies': copies,
                                    'collate': collate,
                                    'landscape': landscape,
                                    'color': color});
  chrome.send('print', [jobSettings]);
}

/**
 * Fill the printer list drop down.
 * @param {array} printers Array of printer names.
 */
function setPrinters(printers) {
  if (printers.length > 0) {
    for (var i = 0; i < printers.length; ++i) {
      var option = document.createElement('option');
      option.textContent = printers[i];
      $('printer-list').add(option);
    }
    $('printer-list').disabled = false;
  } else {
    var option = document.createElement('option');
    option.textContent = localStrings.getString('noPrinter');
    $('printer-list').add(option);
  }
}

function onPDFLoad() {
  $('pdf-viewer').fitToHeight();
}

/**
 * Create the PDF plugin or reload the existing one.
 * @param {string} url The PdfPlugin data url.
 * @param {number} pagesCount The expected total pages count.
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
  if (!$('printer-list').disabled) {
    $('print-button').disabled = false;
  }

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

function selectPageRange() {
  $('print-pages').checked = true;
}

/**
 * Validates the page range text.
 *
 * If the page range text is an empty string, initializes the text value to
 * a default page range and updates the state of print button.
 *
 * If the page range text is not an empty string, parses the page range text
 * for validation. If the specified page range text is valid, |pageRangesInfo|
 * array will have the page ranges information.
 * If the specified page range is invalid, displays an invalid warning icon on
 * the text box and sets the error message as the title message of text box.
 */
function validatePageRangeInfo() {
  var pageRangeField = $('pages');
  var pageRangeText = pageRangeField.value.replace(/\s/g, '');

  if (pageRangeText == '')
    pageRangeField.value = '1-' + expectedPageCount;

  if ($('print-pages').checked) {
    var message = '';
    if (!parsePageRanges())
      message = localStrings.getString('pageRangeInvalidTitleToolTip');
    pageRangeField.setCustomValidity(message);
    pageRangeField.title = message;
  }
  updatePrintButtonState();
}

/**
 * Handles the 'All' pages option click event.
 */
function handlePrintAllPages() {
  pageRangesInfo = [];
  updatePrintButtonState();
}

/**
 * Updates the state of print button depending on the user selection.
 *
 * If the user has selected 'All' pages option, enables the print button.
 * If the user has selected a page range, depending on the validity of page
 * range text enables/disables the print button.
 */
function updatePrintButtonState() {
  $('print-button').disabled = !($('all-pages').checked ||
                                 $('pages').checkValidity());
}

window.addEventListener('DOMContentLoaded', load);
