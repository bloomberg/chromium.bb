// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();
var hasPDFPlugin = true;

// The total page count of the previewed document regardless of which pages the
// user has selected.
var totalPageCount = -1;

// The previously selected pages by the user. It is used in
// onPageSelectionMayHaveChanged() to make sure that a new preview is not
// requested more often than necessary.
var previouslySelectedPages = [];

// Timer id of the page range textfield. It is used to reset the timer whenever
// needed.
var timerId;

/**
 * Window onload handler, sets up the page and starts print preview by getting
 * the printer list.
 */
function onLoad() {
  initializeAnimation();

  $('printer-list').disabled = true;
  $('print-button').disabled = true;
  $('print-button').addEventListener('click', printFile);
  $('cancel-button').addEventListener('click', function(e) {
    window.close();
  });

  $('all-pages').addEventListener('click', onPageSelectionMayHaveChanged);
  $('copies').addEventListener('input', validateNumberOfCopies);
  $('copies').addEventListener('blur', handleCopiesFieldBlur);
  $('print-pages').addEventListener('click', handleIndividualPagesCheckbox);
  $('individual-pages').addEventListener('blur', handlePageRangesFieldBlur);
  $('individual-pages').addEventListener('focus', addTimerToPageRangeField);
  $('individual-pages').addEventListener('input', resetPageRangeFieldTimer);
  $('landscape').addEventListener('click', onLayoutModeToggle);
  $('portrait').addEventListener('click', onLayoutModeToggle);
  $('color').addEventListener('click', function() { setColor(true); });
  $('bw').addEventListener('click', function() { setColor(false); });
  $('printer-list').addEventListener(
      'change', updateControlsWithSelectedPrinterCapabilities);

  chrome.send('getPrinters');
}

/**
 * Gets the selected printer capabilities and updates the controls accordingly.
 */
function updateControlsWithSelectedPrinterCapabilities() {
  var printerList = $('printer-list');
  var selectedPrinter = printerList.selectedIndex;
  if (selectedPrinter < 0)
    return;

  var printerName = printerList.options[selectedPrinter].textContent;
  if (printerName == localStrings.getString('printToPDF')) {
    updateWithPrinterCapabilities({'disableColorOption': true,
                                   'setColorAsDefault': true});
  } else {
    // This message will call back to 'updateWithPrinterCapabilities'
    // function.
    chrome.send('getPrinterCapabilities', [printerName]);
  }
}

/**
 * Updates the controls with printer capabilities information.
 * @param {Object} settingInfo printer setting information.
 */
function updateWithPrinterCapabilities(settingInfo) {
  var disableColorOption = settingInfo.disableColorOption;
  var setColorAsDefault = settingInfo.setColorAsDefault;
  var colorOption = $('color');
  var bwOption = $('bw');

  if (disableColorOption != colorOption.disabled) {
    setControlAndLabelDisabled(colorOption, disableColorOption);
    setControlAndLabelDisabled(bwOption, disableColorOption);
  }

  if (colorOption.checked != setColorAsDefault) {
    colorOption.checked = setColorAsDefault;
    bwOption.checked = !setColorAsDefault;
    setColor(colorOption.checked);
  }
}

/**
 * Disables the input control element and its associated label.
 * @param {HTMLElement} controlElm An input control element.
 * @param {boolean} disable set to true to disable element and label.
 */
function setControlAndLabelDisabled(controlElm, disable) {
  controlElm.disabled = disable;
  var label = $(controlElm.getAttribute('label'));
  if (disable)
    label.classList.add('disabled-label-text');
  else
    label.classList.remove('disabled-label-text');
}

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
  onPageSelectionMayHaveChanged();
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
  var pageRanges = getSelectedPageRanges();
  var parsedPageRanges = '';
  var individualPagesField = $('individual-pages');

  for (var i = 0; i < pageRanges.length; ++i) {
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
  var printerList = $('printer-list')
  var selectedPrinter = printerList.selectedIndex;
  var printerName = '';
  if (selectedPrinter >= 0)
    printerName = printerList.options[selectedPrinter].textContent;
  var printAll = $('all-pages').checked;
  var printToPDF = (printerName == localStrings.getString('printToPDF'));

  return JSON.stringify({'printerName': printerName,
                         'pageRange': getSelectedPageRanges(),
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
 * Called from PrintPreviewHandler::SendPrinterList().
 * @param {Array} printers Array of printer names.
 * @param {number} defaultPrinterIndex The index of the default printer.
 */
function setPrinters(printers, defaultPrinterIndex) {
  var printerList = $('printer-list');
  for (var i = 0; i < printers.length; ++i) {
    var option = document.createElement('option');
    option.textContent = printers[i];
    printerList.add(option);
    if (i == defaultPrinterIndex)
      option.selected = true;
  }

  // Adding option for saving PDF to disk.
  var option = document.createElement('option');
  option.textContent = localStrings.getString('printToPDF');
  printerList.add(option);
  printerList.disabled = false;

  updateControlsWithSelectedPrinterCapabilities();

  // Once the printer list is populated, generate the initial preview.
  getPreview();
}

/**
 * Sets the color mode for the PDF plugin.
 * Called from PrintPreviewHandler::ProcessColorSetting().
 * @param {boolean} color is true if the PDF plugin should display in color.
 */
function setColor(color) {
  if (!hasPDFPlugin) {
    return;
  }
  $('pdf-viewer').grayscale(!color);
}

/**
 * Called when the PDF plugin loads its document.
 */
function onPDFLoad() {
  if (isLandscape())
    $('pdf-viewer').fitToWidth();
  else
    $('pdf-viewer').fitToHeight();
}

/**
 * Update the print preview when new preview data is available.
 * Create the PDF plugin as needed.
 * Called from PrintPreviewUI::PreviewDataIsAvailable().
 * @param {number} pageCount The expected total pages count.
 * @param {string} jobTitle The print job title.
 *
 */
function updatePrintPreview(pageCount, jobTitle) {
  // Initialize the expected page count.
  if (totalPageCount == -1)
    totalPageCount = pageCount;

  // Initialize the selected pages (defaults to all selected).
  if (previouslySelectedPages.length == 0)
    for (var i = 0; i < totalPageCount; i++)
      previouslySelectedPages.push(i+1);

  regeneratePreview = false;

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

  var pdfViewer = $('pdf-viewer');
  if (pdfViewer) {
    pdfViewer.reload();
    pdfViewer.grayscale(!isColor());
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

window.addEventListener('DOMContentLoaded', onLoad);

/**
 * Listener function that executes whenever any of the available settings
 * is changed.
 */
function printSettingChanged() {
  $('collate-option').hidden = getCopies() <= 1;
  updateSummary();
}

/**
 * Updates the print summary based on the currently selected user options.
 *
 */
function updateSummary() {
  var copies = getCopies();
  var printButton = $('print-button');
  var printSummary = $('print-summary');

  if (isNaN($('copies').value)) {
    printSummary.innerHTML =
        localStrings.getString('invalidNumberOfCopiesTitleToolTip');
    return;
  }

  var pageList = getSelectedPages();
  if (pageList.length <= 0) {
    printSummary.innerHTML =
        localStrings.getString('pageRangeInvalidTitleToolTip');
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

  var html = localStrings.getStringF('printPreviewSummaryFormat',
                                     pageList.length, pagesLabel,
                                     twoSidedLabel, timesSign, numOfCopies,
                                     copiesLabel, equalSign,
                                     '<strong>' + numOfSheets + '</strong>',
                                     '<strong>' + sheetsLabel + '</strong>');

  // Removing extra spaces from within the string.
  html.replace(/\s{2,}/g, ' ');
  printSummary.innerHTML = html;
}

/**
 * Handles a click event on the two-sided option.
 */
function handleTwoSidedClick(event) {
  handleZippyClickEl($('binding'));
  printSettingChanged(event);
}

/**
 * Gives focus to the individual pages textfield when 'print-pages' textbox is
 * clicked.
 */
function handleIndividualPagesCheckbox() {
  printSettingChanged();
  $('individual-pages').focus();
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
  totalPageCount = -1;
  previouslySelectedPages.length = 0;
  getPreview();
}

/**
 * Returns a list of all pages in the specified ranges. If the page ranges can't
 * be parsed an empty list is returned.
 *
 * @return {Array}
 */
function getSelectedPages() {
  var pageText = $('individual-pages').value;

  if ($('all-pages').checked || pageText == '')
    pageText = '1-' + totalPageCount;

  var pageList = [];
  var parts = pageText.split(/,/);

  for (var i = 0; i < parts.length; ++i) {
    var part = parts[i];
    var match = part.match(/([0-9]+)-([0-9]+)/);

    if (match && match[1] && match[2]) {
      var from = parseInt(match[1], 10);
      var to = parseInt(match[2], 10);

      if (from && to) {
        for (var j = from; j <= to; ++j)
          if (j <= totalPageCount)
            pageList.push(j);
      }
    } else if (parseInt(part, 10)) {
      if (parseInt(part, 10) <= totalPageCount)
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
function getSelectedPageRanges() {
  var pageList = getSelectedPages();
  var pageRanges = [];
  for (var i = 0; i < pageList.length; ++i) {
    tempFrom = pageList[i];
    while (i + 1 < pageList.length && pageList[i + 1] == pageList[i] + 1)
      ++i;
    tempTo = pageList[i];
    pageRanges.push({'from': tempFrom, 'to': tempTo});
  }
  return pageRanges;
}

/**
 * Whenever the page range textfield gains focus we add a timer to detect when
 * the user stops typing in order to update the print preview.
 */
function addTimerToPageRangeField() {
  timerId = window.setTimeout(onPageSelectionMayHaveChanged, 500);
}

/**
 * As the user types in the page range textfield, we need to reset this timer,
 * since the page ranges are still being edited.
 */
function resetPageRangeFieldTimer() {
  clearTimeout(timerId);
  addTimerToPageRangeField();
}

/**
 * When the user stops typing in the page range textfield or clicks on the
 * 'all-pages' checkbox, a new print preview is requested, only if
 * 1) The input is valid (it can be parsed, even only partially).
 * 2) The newly selected pages differ from the previously selected.
 */
function onPageSelectionMayHaveChanged() {
  var currentlySelectedPages = getSelectedPages();

  if (currentlySelectedPages.length == 0)
    return;
  if (areArraysEqual(previouslySelectedPages, currentlySelectedPages))
    return;

  previouslySelectedPages = currentlySelectedPages;
  getPreview();
}

/**
 * Returns true if the contents of the two arrays are equal.
 */
function areArraysEqual(array1, array2) {
  if (array1.length != array2.length)
    return false;
  for (var i = 0; i < array1.length; i++)
    if(array1[i] != array2[i])
      return false;
  return true;
}
