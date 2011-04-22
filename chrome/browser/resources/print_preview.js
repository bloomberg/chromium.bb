// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();

// Whether or not the PDF plugin supports all the capabilities needed for
// print preview.
var hasCompatiblePDFPlugin = true;

// The total page count of the previewed document regardless of which pages the
// user has selected.
var totalPageCount = -1;

// The previously selected pages by the user. It is used in
// onPageSelectionMayHaveChanged() to make sure that a new preview is not
// requested more often than necessary.
var previouslySelectedPages = [];

// The previously selected layout mode. It is used in order to prevent the
// preview from updating when the user clicks on the already selected layout
// mode.
var previouslySelectedLayout = null;

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
  $('copies').addEventListener('input', copiesFieldChanged);
  $('copies').addEventListener('blur', handleCopiesFieldBlur);
  $('print-pages').addEventListener('click', handleIndividualPagesCheckbox);
  $('individual-pages').addEventListener('blur', handlePageRangesFieldBlur);
  $('individual-pages').addEventListener('focus', addTimerToPageRangeField);
  $('individual-pages').addEventListener('input', function() {
    resetPageRangeFieldTimer();
    updatePrintButtonState();
    updatePrintSummary();
  });

  $('two-sided').addEventListener('click', handleTwoSidedClick)
  $('landscape').addEventListener('click', onLayoutModeToggle);
  $('portrait').addEventListener('click', onLayoutModeToggle);
  $('color').addEventListener('click', function() { setColor(true); });
  $('bw').addEventListener('click', function() { setColor(false); });
  $('printer-list').addEventListener(
      'change', updateControlsWithSelectedPrinterCapabilities);

  chrome.send('getPrinters');
}

/**
 * Disables the controls which need the initiator tab to generate preview
 * data. This function is called when the initiator tab is closed.
 */
function disablePreviewControls() {
  var controlIDs = ['landscape', 'portrait', 'all-pages', 'print-pages',
                    'individual-pages'];
  var controlCount = controlIDs.length;
  for (var i = 0; i < controlCount; i++)
    setControlAndLabelDisabled($(controlIDs[i]), true);
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
  if (label == undefined)
    return;

  if (disable)
    label.classList.add('disabled-label-text');
  else
    label.classList.remove('disabled-label-text');
}

/**
 * Handles copies field blur event.
 */
function handleCopiesFieldBlur() {
  checkAndSetCopiesField();
  copiesFieldChanged();
}

/**
 * Handles page ranges field blur event.
 */
function handlePageRangesFieldBlur() {
  checkAndSetPageRangesField();
  updatePrintButtonState();
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
  updatePrintSummary();
}

/**
 * Checks the value of the page ranges text field. It parses the page ranges and
 * normalizes them. For example: '1-3,2,4,8,9-10,4,4' becomes '1-4, 8-10'.
 * If it can't parse the whole string it will replace with the part it parsed.
 * For example: '1-3,2,4,8,abcdef,9-10,4,4' becomes '1-4, 8-10'.
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
  updatePrintSummary();
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
 * Gets the duplex mode for printing.
 * @return {number} duplex mode.
 */
function getDuplexMode() {
  // Constants values matches printing::PrintingContext::DuplexMode enum.
  const SIMPLEX = 0;
  const LONG_EDGE = 1;
  const SHORT_EDGE = 2;

  if (!isTwoSided())
    return SIMPLEX;

  return $('long-edge').checked ? LONG_EDGE : SHORT_EDGE;
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
                         'duplex': getDuplexMode(),
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
function requestPrintPreview() {
  $('dancing-dots').classList.remove('invisible');
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
  requestPrintPreview();
}

/**
 * Sets the color mode for the PDF plugin.
 * Called from PrintPreviewHandler::ProcessColorSetting().
 * @param {boolean} color is true if the PDF plugin should display in color.
 */
function setColor(color) {
  if (!hasCompatiblePDFPlugin) {
    return;
  }
  var pdfViewer = $('pdf-viewer');
  if (!pdfViewer) {
    return;
  }
  pdfViewer.grayscale(!color);
}

/**
 * Display an error message when print preview fails.
 * Called from PrintPreviewMessageHandler::OnPrintPreviewFailed().
 */
function printPreviewFailed() {
  $('preview-failed').classList.remove('hidden');

  var pdfViewer = $('pdf-viewer');
  if (pdfViewer)
    $('mainview').removeChild(pdfViewer);
}

/**
 * Called when the PDF plugin loads its document.
 */
function onPDFLoad() {
  if (isLandscape())
    $('pdf-viewer').fitToWidth();
  else
    $('pdf-viewer').fitToHeight();

  $('dancing-dots').classList.add('invisible');
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
  if (totalPageCount == -1)
    totalPageCount = pageCount;

  if (previouslySelectedPages.length == 0)
    for (var i = 0; i < totalPageCount; i++)
      previouslySelectedPages.push(i+1);

  if (previouslySelectedLayout == null)
    previouslySelectedLayout = $('portrait');

  regeneratePreview = false;

  // Update the current tab title.
  document.title = localStrings.getStringF('printPreviewTitleFormat', jobTitle);

  createPDFPlugin();

  updatePrintSummary();
}

/**
 * Create the PDF plugin or reload the existing one.
 */
function createPDFPlugin() {
  if (!hasCompatiblePDFPlugin) {
    return;
  }

  // Enable the print button.
  if (!$('printer-list').disabled) {
    $('print-button').disabled = false;
  }

  $('preview-failed').classList.add('hidden');

  var pdfViewer = $('pdf-viewer');
  if (pdfViewer) {
    pdfViewer.reload();
    pdfViewer.grayscale(!isColor());
    return;
  }

  var pdfPlugin = document.createElement('embed');
  pdfPlugin.setAttribute('id', 'pdf-viewer');
  pdfPlugin.setAttribute('type', 'application/pdf');
  pdfPlugin.setAttribute('src', 'chrome://print/print.pdf');
  var mainView = $('mainview');
  mainView.appendChild(pdfPlugin);

  // Check to see if the PDF plugin is our PDF plugin. (or compatible)
  if (!pdfPlugin.onload) {
    hasCompatiblePDFPlugin = false;
    mainView.removeChild(pdfPlugin);
    $('no-plugin').classList.remove('hidden');
    return;
  }
  pdfPlugin.onload('onPDFLoad()');

  // Older version of the PDF plugin may not have this method.
  // TODO(thestig) Eventually remove this.
  if (pdfPlugin.removePrintButton) {
    pdfPlugin.removePrintButton();
  }

  pdfPlugin.grayscale(true);
}

/**
 * Updates the state of print button depending on the user selection.
 * The button is enabled only when the following conditions are true.
 * 1) The selected page ranges are valid.
 * 2) The number of copies is valid.
 */
function updatePrintButtonState() {
  $('print-button').disabled = (!isNumberOfCopiesValid() ||
                                getSelectedPagesSet().length == 0);
}

window.addEventListener('DOMContentLoaded', onLoad);

/**
 * Listener function that executes whenever a change occurs in the 'copies'
 * field.
 */
function copiesFieldChanged() {
  updatePrintButtonState();
  $('collate-option').hidden = getCopies() <= 1;
  updatePrintSummary();
}

/**
 * Updates the print summary based on the currently selected user options.
 *
 */
function updatePrintSummary() {
  var copies = getCopies();
  var printSummary = $('print-summary');

  if (!isNumberOfCopiesValid()) {
    printSummary.innerHTML = localStrings.getString('invalidNumberOfCopies');
    return;
  }

  var pageList = getSelectedPagesSet();
  if (pageList.length <= 0) {
    printSummary.innerHTML = localStrings.getString('invalidPageRange');
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
function handleTwoSidedClick() {
  handleZippyClickEl($('binding'));
  updatePrintSummary();
}

/**
 * Gives focus to the individual pages textfield when 'print-pages' textbox is
 * clicked.
 */
function handleIndividualPagesCheckbox() {
  onPageSelectionMayHaveChanged();
  $('individual-pages').focus();
}

/**
 * When the user switches printing orientation mode the page field selection is
 * reset to "all pages selected". After the change the number of pages will be
 * different and currently selected page numbers might no longer be valid.
 * Even if they are still valid the content of these pages will be different.
 */
function onLayoutModeToggle() {
  var currentlySelectedLayout = $('portrait').checked ? $('portrait') :
      $('landscape');

  // If the chosen layout is same as before, nothing needs to be done.
  if (previouslySelectedLayout == currentlySelectedLayout)
    return;

  previouslySelectedLayout = currentlySelectedLayout;
  $('individual-pages').value = '';
  $('all-pages').checked = true;
  totalPageCount = -1;
  previouslySelectedPages.length = 0;
  requestPrintPreview();
}

/**
 * Returns a list of all pages in the specified ranges. The pages are listed in
 * the order they appear in the 'individual-pages' textbox and duplicates are
 * not eliminated. If the page ranges can't be parsed an empty list is
 * returned.
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
  var pageList = getSelectedPagesSet();
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
 * Returns the selected pages in ascending order without any duplicates.
 */
function getSelectedPagesSet() {
  var pageList = getSelectedPages();
  pageList.sort(function(a,b) { return a - b; });
  pageList = removeDuplicates(pageList);
  return pageList;
}

/**
 * Removes duplicate elements from |inArray| and returns a new  array.
 * |inArray| is not affected. It assumes that the array is already sorted.
 *
 * @param {Array} inArray The array to be processed.
 */
function removeDuplicates(inArray) {
  var out = [];

  if(inArray.length == 0)
    return out;

  out.push(inArray[0]);
  for (var i = 1; i < inArray.length; ++i)
    if(inArray[i] != inArray[i - 1])
      out.push(inArray[i]);
  return out;
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
  var currentlySelectedPages = getSelectedPagesSet();

  if (currentlySelectedPages.length == 0)
    return;
  if (areArraysEqual(previouslySelectedPages, currentlySelectedPages))
    return;

  previouslySelectedPages = currentlySelectedPages;
  requestPrintPreview();
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
