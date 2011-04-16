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
  initializeAnimation();

  updateSummary();

  $('printer-list').disabled = true;
  $('print-button').disabled = true;
  $('print-button').addEventListener('click', printFile);
  $('cancel-button').addEventListener('click', function(e) {
    window.close();
  });

  $('copies').addEventListener('input', validateNumberOfCopies);
  $('copies').addEventListener('blur', handleCopiesFieldBlur);
  $('individual-pages').addEventListener('blur', handlePageRangesFieldBlur);
  $('landscape').onclick = onLayoutModeToggle;
  $('portrait').onclick = onLayoutModeToggle;
  $('color').onclick = getPreview;
  $('bw').onclick = getPreview;

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
}

/**
 * Handles copies field blur event.
 */
function handleCopiesFieldBlur() {
  checkAndSetCopiesField();
  printSettingChanged();
}

/**
 * Handles page ranges field blur event.
 */
function handlePageRangesFieldBlur() {
  checkAndSetPageRangesField();
}

/**
 * Depending on 'copies' value, shows/hides the collate checkbox.
 */
function updateCollateCheckboxState() {
  $('collate-option').hidden = getCopies() <= 1;
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
 * Checks the value of the copies field. If it is a valid number it does
 * nothing. If it can only parse the first part of the string it replaces the
 * string with the first part. Example: '123abcd' becomes '123'.
 * If the string can't be parsed at all it replaces with 1.
 */
function checkAndSetCopiesField() {
  var copiesField = $('copies');
  var copies = parseInt(copiesField.value, 10);
  if (isNaN(copies))
    copies = 1;
  copiesField.value = copies;
  updateSummary();
}

/**
 * Checks the value of the page ranges text field. It parses the page ranges and
 * normalizes them. For example: '1,2,3,5,9-10' becomes '1-3, 5, 9-10'.
 * If it can't parse the whole string it will replace with the part it parsed.
 * For example: '1-6,9-10,sd343jf' becomes '1-6, 9-10'. If the specified page
 * range includes all pages it replaces it with the empty string (so that the
 * example text is automatically shown.
 *
 */
function checkAndSetPageRangesField() {
  var pageRanges = getPageRanges();
  var parsedPageRanges = '';
  var individualPagesField = $('individual-pages');

  if (pageRanges.length == 1 && pageRanges[0].from == 1 &&
          pageRanges[0].to == expectedPageCount) {
    individualPagesField.value = parsedPageRanges;
    return;
  }

  for (var i = 0; i < pageRanges.length; i++) {
    if (pageRanges[i].from == pageRanges[i].to)
      parsedPageRanges += pageRanges[i].from;
    else
      parsedPageRanges += pageRanges[i].from + '-' + pageRanges[i].to;
    if (i < pageRanges.length - 1)
      parsedPageRanges += ', ';
  }
  individualPagesField.value = parsedPageRanges;
  updateSummary();
}

/**
 * Checks whether the preview layout setting is set to 'landscape' or not.
 *
 * @return {boolean} true if layout is 'landscape'.
 */
function isLandscape() {
  return $('landscape').checked;
}

/**
 * Checks whether the preview color setting is set to 'color' or not.
 *
 * @return {boolean} true if color is 'color'.
 */
function isColor() {
  return $('color').checked;
}

/**
 * Checks whether the preview collate setting value is set or not.
 *
 * @return {boolean} true if collate setting is enabled and checked.
 */
function isCollated() {
  var collateField = $('collate');
  return !collateField.disabled && collateField.checked;
}

/**
 * Returns the number of copies currently indicated in the copies textfield. If
 * the contents of the textfield can not be converted to a number or if <1 it
 * returns 1.
 *
 * @return {number} number of copies.
 */
function getCopies() {
  var copies = parseInt($('copies').value, 10);
  if (!copies || copies <= 1)
    copies = 1;
  return copies;
}

/**
 * Checks whether the preview two-sided checkbox is checked.
 *
 * @return {boolean} true if two-sided is checked.
 */
function isTwoSided() {
  return $('two-sided').checked;
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
  var printToPDF = (printerName == localStrings.getString('printToPDF'));

  return JSON.stringify({'printerName': printerName,
                         'pageRange': getPageRanges(),
                         'printAll': printAll,
                         'twoSided': isTwoSided(),
                         'copies': getCopies(),
                         'collate': isCollated(),
                         'landscape': isLandscape(),
                         'color': isColor(),
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
 * @param {Array} printers Array of printer names.
 * @param {number} defaultPrinterIndex The index of the default printer.
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
  expectedPageCount = pageCount;

  // Set the print job title.
  printJobTitle = jobTitle;

  // Update the current tab title.
  document.title = localStrings.getStringF('printPreviewTitleFormat', jobTitle);

  createPDFPlugin();

  updateSummary();
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

  var pdfPlugin = document.createElement('embed');
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
                                  $('individual-pages').checkValidity()) ||
                                !$('copies').checkValidity());
}

window.addEventListener('DOMContentLoaded', load);

/**
 * Listener function that executes whenever any of the available settings
 * is changed.
 */
function printSettingChanged() {
  updateCollateCheckboxState();
  updateSummary();
}

/**
 * Updates the print summary based on the currently selected user options.
 *
 */
function updateSummary() {
  var html = '';
  var pageList = getPageList();
  var copies = getCopies();
  var printButton = $('print-button');
  var printSummary = $('print-summary');

  if (isNaN($('copies').value)) {
    html = localStrings.getString('invalidNumberOfCopiesTitleToolTip');
    printSummary.innerHTML = html;
    return;
  }

  if (pageList.length <= 0) {
    html = localStrings.getString('pageRangeInvalidTitleToolTip');
    printSummary.innerHTML = html;
    printButton.disabled = true;
    return;
  }

  var pagesLabel = localStrings.getString('printPreviewPageLabelSingular');
  var twoSidedLabel = '';
  var timesSign = '';
  var numOfCopies = '';
  var copiesLabel = '';
  var equalSign = '';
  var numOfSheets = '';
  var sheetsLabel = '';

  printButton.disabled = false;

  if (pageList.length > 1)
    pagesLabel = localStrings.getString('printPreviewPageLabelPlural');

  if (isTwoSided())
    twoSidedLabel = '('+localStrings.getString('optionTwoSided')+')';

  if (copies > 1) {
    timesSign = '×';
    numOfCopies = copies;
    copiesLabel = localStrings.getString('copiesLabel').toLowerCase();
  }

  if ((copies > 1) || (isTwoSided())) {
    numOfSheets = pageList.length;

    if (isTwoSided())
      numOfSheets = Math.ceil(numOfSheets / 2);

    equalSign = '=';
    numOfSheets *= copies;
    sheetsLabel = localStrings.getString('printPreviewSheetsLabel');
  }

  html = localStrings.getStringF('printPreviewSummaryFormat', pageList.length,
                                  pagesLabel, twoSidedLabel, timesSign,
                                  numOfCopies, copiesLabel, equalSign,
                                  '<strong>'+numOfSheets+'</strong>',
                                  '<strong>'+sheetsLabel+'</strong>');

  // Removing extra spaces from within the string.
  html.replace(/\s{2,}/g, ' ');
  printSummary.innerHTML = html;
}

/**
 * Handles a click event on the two-sided option.
 */
function handleTwoSidedClick(event) {
  var el = $('binding');
  handleZippyClickEl(el);
  printSettingChanged(event);
}

/**
 * When the user switches printing orientation mode the page field selection is
 * reset to "all pages selected". After the change the number of pages will be
 * different and currently selected page numbers might no longer be valid.
 * Even if they are still valid the content of these pages will be different.
 */
function onLayoutModeToggle() {
  $('individual-pages').value = '';
  $('all-pages').checked = true;
  getPreview();
}

/**
 * Returns a list of all pages in the specified ranges. If the page ranges can't
 * be parsed an empty list is returned.
 *
 * @return {Array}
 */
function getPageList() {
  var pageText = $('individual-pages').value;

  if ($('all-pages').checked || pageText == '')
    pageText = '1-' + expectedPageCount;

  var pageList = [];
  var parts = pageText.split(/,/);

  for (var i = 0; i < parts.length; i++) {
    var part = parts[i];
    var match = part.match(/([0-9]+)-([0-9]+)/);

    if (match && match[1] && match[2]) {
      var from = parseInt(match[1], 10);
      var to = parseInt(match[2], 10);

      if (from && to) {
        for (var j = from; j <= to; j++)
          if (j <= expectedPageCount)
            pageList.push(j);
      }
    } else if (parseInt(part, 10)) {
      if (parseInt(part, 10) <= expectedPageCount)
        pageList.push(parseInt(part, 10));
    }
  }
  return pageList;
}

/**
 * Parses the selected page ranges, processes them and returns the results.
 * It squashes whenever possible. Example '1-2,3,5-7' becomes 1-3,5-7
 *
 * @return {Array} an array of page range objects. A page range object has
 *     fields 'from' and 'to'.
 */
function getPageRanges() {
  var pageList = getPageList();
  var pageRanges = [];
  for (var i = 0; i < pageList.length; i++) {
    tempFrom = pageList[i];
    while (i + 1 < pageList.length && pageList[i + 1] == pageList[i] + 1)
      i++;
    tempTo = pageList[i];
    pageRanges.push({'from': tempFrom, 'to': tempTo});
  }
  return pageRanges;
}
