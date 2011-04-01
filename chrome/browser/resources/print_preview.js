// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();
var hasPDFPlugin = true;
var expectedPageCount = 0;
var pageRangesInfo = [];
var printJobTitle = '';

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
  $('copies').addEventListener('input', validateNumberOfCopies);
  $('copies').addEventListener('blur', handleCopiesFieldBlur);
  $('layout').onchange = getPreview;
  $('color').onchange = getPreview;

  updateCollateCheckboxState();
  chrome.send('getPrinters');
};

/**
 * Parses the copies field text for validation and updates the state of print
 * button and collate checkbox. If the specified value is invalid, displays an
 * invalid warning icon on the text box and sets the error message as the title
 * message of text box.
 */
function validateNumberOfCopies() {
  var copiesField = $('copies');
  var message = '';
  if (!isNumberOfCopiesValid())
    message = localStrings.getString('invalidNumberOfCopiesTitleToolTip');
  copiesField.setCustomValidity(message);
  copiesField.title = message;
  updatePrintButtonState();
  updateCollateCheckboxState();
}

/**
 * Handles copies field blur event.
 */
function handleCopiesFieldBlur() {
  checkAndSetInputFieldDefaultValue($('copies'), 1);
}

/**
 * Updates the state of collate checkbox.
 *
 * Depending on the validity of 'copies' value, enables/disables the collate
 * checkbox.
 */
function updateCollateCheckboxState() {
  var copiesField = $('copies');
  var collateField = $('collate');
  collateField.disabled = !(copiesField.checkValidity() &&
                            copiesField.value > 1);
  if (collateField.disabled)
    $('collateOptionLabel').classList.add('disabled-label-text');
  else
    $('collateOptionLabel').classList.remove('disabled-label-text');
}

/**
 * Validates the copies text field value.
 * NOTE: An empty copies field text is considered valid because the blur event
 * listener of this field will set it back to a default value.
 * @return {boolean} true if the number of copies is valid else returns false.
 */
function isNumberOfCopiesValid() {
  var copiesFieldText = $('copies').value.replace(/\s/g, '');
  if (copiesFieldText == '')
    return true;

  var numericExp = /^[0-9]+$/;
  return (numericExp.test(copiesFieldText) && Number(copiesFieldText) > 0);
}

/**
 * Checks whether the input field text is empty or not. If the value is empty,
 * sets the input field default value.
 * @param {HTMLElement} inpElement An input element.
 * @param {string} defaultVal Input field default value.
 */
function checkAndSetInputFieldDefaultValue(inpElement, defaultVal) {
  var inpElementText = inpElement.value.replace(/\s/g, '');
  if (inpElementText == '')
    inpElement.value = defaultVal;
}

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

/**
 * Checks whether the preview layout setting is set to 'landscape' or not.
 *
 * @return {boolean} true if layout is 'landscape'.
 */
function isLandscape() {
  return ($('layout').options[$('layout').selectedIndex].value == '1');
}

/**
 * Checks whether the preview color setting is set to 'color' or not.
 *
 * @return {boolean} true if color is 'color'.
 */
function isColor() {
  return ($('color').options[$('color').selectedIndex].value == '1');
}

/**
 * Creates a JSON string based on the values in the printer settings.
 *
 * @return {string} JSON string with print job settings.
 */
function getSettingsJSON() {
  var selectedPrinter = $('printer-list').selectedIndex;
  var printerName = '';
  if (selectedPrinter >= 0)
    printerName = $('printer-list').options[selectedPrinter].textContent;
  var printAll = $('all-pages').checked;
  var twoSided = $('two-sided').checked;
  var copies = $('copies').value;
  var collate = $('collate').checked;
  var landscape = isLandscape();
  var color = isColor();
  var printToPDF = (printerName == localStrings.getString('printToPDF'));

  return JSON.stringify({'printerName': printerName,
                         'pageRange': pageRangesInfo,
                         'printAll': printAll,
                         'twoSided': twoSided,
                         'copies': copies,
                         'collate': collate,
                         'landscape': landscape,
                         'color': color,
                         'printToPDF': printToPDF});
}

/**
 * Asks the browser to print the preview PDF based on current print settings.
 */
function printFile() {
  chrome.send('print', [getSettingsJSON()]);
}

/**
 * Asks the browser to generate a preview PDF based on current print settings.
 */
function getPreview() {
  chrome.send('getPreview', [getSettingsJSON()]);
}

/**
 * Fill the printer list drop down.
 * @param {array} printers Array of printer names.
 * @param {int} defaultPrinterIndex The index of the default printer.
 */
function setPrinters(printers, defaultPrinterIndex) {
  if (printers.length > 0) {
    for (var i = 0; i < printers.length; ++i) {
      var option = document.createElement('option');
      option.textContent = printers[i];
      $('printer-list').add(option);
      if (i == defaultPrinterIndex)
        option.selected = true;
    }
  } else {
    var option = document.createElement('option');
    option.textContent = localStrings.getString('noPrinter');
    $('printer-list').add(option);
  }

  // Adding option for saving PDF to disk.
  var option = document.createElement('option');
  option.textContent = localStrings.getString('printToPDF');
  $('printer-list').add(option);
  $('printer-list').disabled = false;

  // Once the printer list is populated, generate the initial preview.
  getPreview();
}

/**
 * Sets the color mode for the PDF plugin.
 * @param {boolean} color is true if the PDF plugin should display in color.
 */
function setColor(color) {
  if (!hasPDFPlugin) {
    return;
  }
  $('pdf-viewer').grayscale(!color);
}

function onPDFLoad() {
  if (isLandscape())
    $('pdf-viewer').fitToWidth();
  else
    $('pdf-viewer').fitToHeight();
}

/**
 * Update the print preview when new preview data is available.
 * Create the PDF plugin as needed.
 * @param {number} pageCount The expected total pages count.
 * @param {string} jobTitle The print job title.
 *
 */
function updatePrintPreview(pageCount, jobTitle) {
  // Set the expected page count.
  if (expectedPageCount != pageCount) {
    expectedPageCount = pageCount;
    // Set the initial page range text.
    $('pages').value = '1-' + expectedPageCount;
  }

  // Set the print job title.
  printJobTitle = jobTitle;

  // Update the current tab title.
  document.title = localStrings.getStringF('printPreviewTitleFormat', jobTitle);

  createPDFPlugin();
}

/**
 * Create the PDF plugin or reload the existing one.
 */
function createPDFPlugin() {
  if (!hasPDFPlugin) {
    return;
  }

  // Enable the print button.
  if (!$('printer-list').disabled) {
    $('print-button').disabled = false;
  }

  if ($('pdf-viewer')) {
    $('pdf-viewer').reload();
    $('pdf-viewer').grayscale(!isColor());
    return;
  }

  var loadingElement = $('loading');
  loadingElement.classList.add('hidden');
  var mainView = loadingElement.parentNode;

  var pdfPlugin = document.createElement('object');
  pdfPlugin.setAttribute('id', 'pdf-viewer');
  pdfPlugin.setAttribute('type', 'application/pdf');
  pdfPlugin.setAttribute('src', 'chrome://print/print.pdf');
  mainView.appendChild(pdfPlugin);
  if (!pdfPlugin.onload) {
    hasPDFPlugin = false;
    mainView.removeChild(pdfPlugin);
    $('no-plugin').classList.remove('hidden');
    return;
  }
  pdfPlugin.grayscale(true);
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

  checkAndSetInputFieldDefaultValue(pageRangeField, ('1-' + expectedPageCount));

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
 * Depending on the validity of 'copies' value, enables/disables the print
 * button.
 */
function updatePrintButtonState() {
  $('print-button').disabled = (!($('all-pages').checked ||
                                  $('pages').checkValidity()) ||
                                !$('copies').checkValidity());
}

window.addEventListener('DOMContentLoaded', load);
