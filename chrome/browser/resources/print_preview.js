// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();

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

// Store the last selected printer index.
var lastSelectedPrinterIndex = 0;

// Indicates whether a preview has been requested but not received yet.
var isPreviewStillLoading = true;

// Currently selected printer capabilities.
var printerCapabilities;

// Used to disable some printing options when the preview is not modifiable.
var previewModifiable = false;

// Destination list special value constants.
const PRINT_TO_PDF = 'Print To PDF';
const MANAGE_PRINTERS = 'Manage Printers';

/**
 * Window onload handler, sets up the page and starts print preview by getting
 * the printer list.
 */
function onLoad() {
  $('system-dialog-link').addEventListener('click', showSystemDialog);
  $('cancel-button').addEventListener('click', handleCancelButtonClick);

  if (!checkCompatiblePluginExists()) {
    displayErrorMessage(localStrings.getString('noPlugin'));
    $('mainview').parentElement.removeChild($('dummy-viewer'));
    return;
  }
  $('mainview').parentElement.removeChild($('dummy-viewer'));

  $('printer-list').disabled = true;
  $('print-button').disabled = true;
  $('print-button').addEventListener('click', printFile);
  $('all-pages').addEventListener('click', onPageSelectionMayHaveChanged);
  $('copies').addEventListener('input', copiesFieldChanged);
  $('print-pages').addEventListener('click', handleIndividualPagesCheckbox);
  $('individual-pages').addEventListener('blur', function() {
      clearTimeout(timerId);
      onPageSelectionMayHaveChanged();
  });
  $('individual-pages').addEventListener('focus', addTimerToPageRangeField);
  $('individual-pages').addEventListener('input', resetPageRangeFieldTimer);
  $('two-sided').addEventListener('click', handleTwoSidedClick)
  $('landscape').addEventListener('click', onLayoutModeToggle);
  $('portrait').addEventListener('click', onLayoutModeToggle);
  $('color').addEventListener('click', function() { setColor(true); });
  $('bw').addEventListener('click', function() { setColor(false); });
  $('printer-list').addEventListener(
      'change', updateControlsWithSelectedPrinterCapabilities);
  $('increment').addEventListener('click',
                                  function() { onCopiesButtonsClicked(1); });
  $('decrement').addEventListener('click',
                                  function() { onCopiesButtonsClicked(-1); });
  $('controls').onsubmit = function() { return false; };
  $('dancing-dots').classList.remove('invisible');
  chrome.send('getPrinters');
}

/**
 * Asks the browser to close the preview tab.
 */
function handleCancelButtonClick() {
  chrome.send('closePrintPreviewTab');
}

/**
 * Asks the browser to show the native print dialog for printing.
 */
function showSystemDialog() {
  chrome.send('showSystemDialog');
}

/**
 * Disables the controls which need the initiator tab to generate preview
 * data. This function is called when the initiator tab is closed.
 * @param {string} initiatorTabURL The URL of the initiator tab.
 */
function onInitiatorTabClosed(initiatorTabURL) {
  displayErrorMessage(localStrings.getStringF('initiatorTabClosed',
                                              initiatorTabURL));
}

/**
 * Gets the selected printer capabilities and updates the controls accordingly.
 */
function updateControlsWithSelectedPrinterCapabilities() {
  var printerList = $('printer-list');
  var selectedIndex = printerList.selectedIndex;
  if (selectedIndex < 0)
    return;

  var selectedValue = printerList.options[selectedIndex].value;
  if (selectedValue == PRINT_TO_PDF) {
    updateWithPrinterCapabilities({'disableColorOption': true,
                                   'setColorAsDefault': true});
  } else if (selectedValue == MANAGE_PRINTERS) {
    printerList.selectedIndex = lastSelectedPrinterIndex;
    chrome.send('managePrinters');
    return;
  } else {
    // This message will call back to 'updateWithPrinterCapabilities'
    // function.
    chrome.send('getPrinterCapabilities', [selectedValue]);
  }

  lastSelectedPrinterIndex = selectedIndex;

  // Regenerate the preview data based on selected printer settings.
  setDefaultValuesAndRegeneratePreview();
}

/**
 * Updates the controls with printer capabilities information.
 * @param {Object} settingInfo printer setting information.
 */
function updateWithPrinterCapabilities(settingInfo) {
  printerCapabilities = settingInfo;

  if (isPreviewStillLoading)
    return;

  var disableColorOption = settingInfo.disableColorOption;
  var setColorAsDefault = settingInfo.setColorAsDefault;
  var colorOption = $('color');
  var bwOption = $('bw');

  if (disableColorOption)
    $('color-options').classList.add("hidden");
  else
    $('color-options').classList.remove("hidden");

  if (colorOption.checked != setColorAsDefault) {
    colorOption.checked = setColorAsDefault;
    bwOption.checked = !setColorAsDefault;
    setColor(colorOption.checked);
  }
}

/**
 * Disables or enables all controls in the options pane except for the cancel
 * button.
 */
function setControlsDisabled(disabled) {
  var elementList = $('controls').elements;
  for (var i = 0; i < elementList.length; ++i) {
    if (elementList[i] == $('cancel-button'))
      continue;
    elementList[i].disabled = disabled;
  }
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

  return (isInteger(copiesFieldText) && Number(copiesFieldText) > 0);
}

/**
 * Returns true if |toTest| contains only digits. Leading and trailing
 * whitespace is allowed.
 * @param {string} toTest The string to be tested.
 */
function isInteger(toTest) {
  var numericExp = /^\s*[0-9]+\s*$/;
  return numericExp.test(toTest);
}

/**
 * Parses the page ranges textfield and returns a suggestion.
 * For example: '1-3,2,4,8,9-10,4,4' becomes '1-4, 8-10'. If it can't parse the
 * whole string it will suggest only the part that was parsed.
 * For example: '1-3,2,4,8,abcdef,9-10,4,4' becomes '1-4, 8-10'.
 */
function getPageRangesSuggestion() {
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
  return parsedPageRanges;
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
  // Constants values matches printing::DuplexMode enum.
  const SIMPLEX = 0;
  const LONG_EDGE = 1;
  return !isTwoSided() ? SIMPLEX : LONG_EDGE;
}

/**
 * Creates a JSON string based on the values in the printer settings.
 *
 * @return {string} JSON string with print job settings.
 */
function getSettingsJSON() {
  var printerList = $('printer-list')
  var selectedPrinter = printerList.selectedIndex;
  var deviceName = '';
  if (selectedPrinter >= 0)
    deviceName = printerList.options[selectedPrinter].value;
  var printAll = $('all-pages').checked;
  var printToPDF = (deviceName == PRINT_TO_PDF);

  return JSON.stringify({'deviceName': deviceName,
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
  isPreviewStillLoading = true;
  setControlsDisabled(true);
  $('dancing-dots').classList.remove('invisible');
  chrome.send('getPreview', [getSettingsJSON()]);
}

/**
 * Fill the printer list drop down.
 * Called from PrintPreviewHandler::SendPrinterList().
 * @param {Array} printers Array of printer info objects.
 * @param {number} defaultPrinterIndex The index of the default printer.
 */
function setPrinters(printers, defaultPrinterIndex) {
  var printerList = $('printer-list');
  for (var i = 0; i < printers.length; ++i) {
    addDestinationListOption(printers[i].printerName, printers[i].deviceName,
                             i == defaultPrinterIndex, false);
  }
  addDestinationListOption('','',false, true);

  // Adding option for saving PDF to disk.
  addDestinationListOption(localStrings.getString('printToPDF'),
                           PRINT_TO_PDF, false, false);
  addDestinationListOption('','',false, true);

  // Add an option to manage printers.
  addDestinationListOption(localStrings.getString('managePrinters'),
                           MANAGE_PRINTERS, false, false);

  printerList.disabled = false;
  updateControlsWithSelectedPrinterCapabilities();
}

/**
 * Adds an option to the printer destination list.
 * @param {String} optionText specifies the option text content.
 * @param {String} optionValue specifies the option value.
 * @param {boolean} isDefault is true if the option needs to be selected.
 * @param {boolean} isDisabled is true if the option needs to be disabled.
 */
function addDestinationListOption(optionText, optionValue, isDefault,
    isDisabled) {
  var option = document.createElement('option');
  option.textContent = optionText;
  option.value = optionValue;
  $('printer-list').add(option);
  option.selected = isDefault;
  option.disabled = isDisabled;
}

/**
 * Sets the color mode for the PDF plugin.
 * Called from PrintPreviewHandler::ProcessColorSetting().
 * @param {boolean} color is true if the PDF plugin should display in color.
 */
function setColor(color) {
  var pdfViewer = $('pdf-viewer');
  if (!pdfViewer) {
    return;
  }
  pdfViewer.grayscale(!color);
}

/**
 * Display an error message in the center of the preview area.
 * @param (string) errorMessage The error message to be displayed.
 */
function displayErrorMessage(errorMessage) {
  isPreviewStillLoading = false;
  $('dancing-dots').classList.remove('invisible');
  $('dancing-dots-text').classList.add('hidden');
  $('error-text').innerHTML = errorMessage;
  $('error-text').classList.remove('hidden');
  setControlsDisabled(true);

  var pdfViewer = $('pdf-viewer');
  if (pdfViewer)
    $('mainview').removeChild(pdfViewer);
}

/**
 * Display an error message when print preview fails.
 * Called from PrintPreviewMessageHandler::OnPrintPreviewFailed().
 */
function printPreviewFailed() {
  displayErrorMessage(localStrings.getString('previewFailed'));
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
  setControlsDisabled(false);

  if (!previewModifiable) {
    $('landscape').disabled = true;
    $('portrait').disabled = true;
  }

  updateCopiesButtonsState();
  updateWithPrinterCapabilities(printerCapabilities);
}

/**
 * Update the print preview when new preview data is available.
 * Create the PDF plugin as needed.
 * Called from PrintPreviewUI::PreviewDataIsAvailable().
 * @param {number} pageCount The expected total pages count.
 * @param {string} jobTitle The print job title.
 * @param {boolean} modifiable If the preview is modifiable.
 *
 */
function updatePrintPreview(pageCount, jobTitle, modifiable) {
  previewModifiable = modifiable;

  if (totalPageCount == -1)
    totalPageCount = pageCount;

  if (previouslySelectedPages.length == 0)
    for (var i = 0; i < totalPageCount; i++)
      previouslySelectedPages.push(i+1);

  if (previouslySelectedLayout == null)
    previouslySelectedLayout = $('portrait');

  // Update the current tab title.
  document.title = localStrings.getStringF('printPreviewTitleFormat', jobTitle);

  createPDFPlugin();
  isPreviewStillLoading = false;
  updatePrintSummary();
}

/**
 * Create the PDF plugin or reload the existing one.
 */
function createPDFPlugin() {
  // Enable the print button.
  if (!$('printer-list').disabled) {
    $('print-button').disabled = false;
  }

  var pdfViewer = $('pdf-viewer');
  if (pdfViewer) {
    // Older version of the PDF plugin may not have this method.
    // TODO(thestig) Eventually remove this check.
    if (pdfViewer.goToPage) {
      // Need to call this before the reload(), where the plugin resets its
      // internal page count.
      pdfViewer.goToPage('0');
    }
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
  pdfPlugin.onload('onPDFLoad()');

  // Older version of the PDF plugin may not have this method.
  // TODO(thestig) Eventually remove this check.
  if (pdfPlugin.removePrintButton) {
    pdfPlugin.removePrintButton();
  }

  pdfPlugin.grayscale(true);
}

/**
 * Returns true if a compatible pdf plugin exists, false if it doesn't.
 */
function checkCompatiblePluginExists() {
  var dummyPlugin = $('dummy-viewer')
  return !!dummyPlugin.onload;
}

/**
 * Updates the state of print button depending on the user selection.
 * The button is enabled only when the following conditions are true.
 * 1) The selected page ranges are valid.
 * 2) The number of copies is valid.
 */
function updatePrintButtonState() {
  $('print-button').disabled = (!isNumberOfCopiesValid() ||
                                getSelectedPagesValidityLevel() != 1);
}

window.addEventListener('DOMContentLoaded', onLoad);

/**
 * Listener function that executes whenever a change occurs in the 'copies'
 * field.
 */
function copiesFieldChanged() {
  updateCopiesButtonsState();
  updatePrintButtonState();
  $('collate-option').hidden = getCopies() <= 1;
  updatePrintSummary();
}

/**
 * Executes whenever a blur event occurs on the 'individual-pages'
 * field or when the timer expires. It takes care of
 * 1) showing/hiding warnings/suggestions
 * 2) updating print button/summary
 */
function pageRangesFieldChanged() {
  var currentlySelectedPages = getSelectedPagesSet();
  var individualPagesField = $('individual-pages');
  var individualPagesHint = $('individual-pages-hint');
  var validityLevel = getSelectedPagesValidityLevel();

  if (validityLevel == 1)
    individualPagesField.classList.remove('invalid');
  else
    individualPagesField.classList.add('invalid');

  if (individualPagesField.value.length == 0) {
    hideInvalidHint(individualPagesHint);
  } else if (currentlySelectedPages.length == 0) {
    individualPagesHint.classList.remove('suggestion');
    individualPagesHint.classList.add('invalid');
    individualPagesHint.innerHTML =
        localStrings.getStringF('pageRangeInstruction',
                                localStrings.getString(
                                    'examplePageRangeText'));
    showInvalidHint(individualPagesHint);
  } else if (individualPagesField.value.replace(/\s*/g,'') !=
      getPageRangesSuggestion().replace(/\s*/g,'')) {
    individualPagesHint.innerHTML =
        localStrings.getStringF('didYouMean', getPageRangesSuggestion());
    individualPagesHint.classList.remove('invalid');
    individualPagesHint.classList.add('suggestion');
    showInvalidHint(individualPagesHint);
  } else if (individualPagesField.value.replace(/\s*/g,'') ==
      getPageRangesSuggestion().replace(/\s*/g,'')) {
    hideInvalidHint(individualPagesHint);
  }

  resetPageRangeFieldTimer();
  updatePrintButtonState();
  updatePrintSummary();
}

/**
 * Updates the state of the increment/decrement buttons based on the current
 * 'copies' value.
 */
function updateCopiesButtonsState() {
  var copiesField = $('copies');
  if (!isNumberOfCopiesValid()) {
    copiesField.classList.add('invalid');
    $('increment').disabled = false;
    $('decrement').disabled = false;
    showInvalidHint($('copies-hint'));
  }
  else {
    copiesField.classList.remove('invalid');
    $('increment').disabled = (getCopies() == copiesField.max) ? true : false;
    $('decrement').disabled = (getCopies() == copiesField.min) ? true : false;
    hideInvalidHint($('copies-hint'));
  }
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

  if (getSelectedPagesValidityLevel() != 1) {
    printSummary.innerHTML = localStrings.getString('invalidPageRange');
    return;
  }

  var pageList = getSelectedPagesSet();
  var numOfSheets = pageList.length;
  var sheetsLabel = localStrings.getString('printPreviewSheetsLabelSingular');
  var numOfPagesText = '';
  var pagesLabel = '';

  if (isTwoSided())
    numOfSheets = Math.ceil(numOfSheets / 2);
  numOfSheets *= copies;

  if (numOfSheets > 1)
    sheetsLabel = localStrings.getString('printPreviewSheetsLabelPlural');

  var html = '';
  if (pageList.length * copies != numOfSheets) {
    numOfPagesText = pageList.length * copies;
    pagesLabel = localStrings.getString('printPreviewPageLabelPlural');
    html = localStrings.getStringF('printPreviewSummaryFormatLong',
                                   '<b>' + numOfSheets + '</b>',
                                   '<b>' + sheetsLabel + '</b>',
                                   numOfPagesText, pagesLabel);
  } else
    html = localStrings.getStringF('printPreviewSummaryFormatShort',
                                   '<b>' + numOfSheets + '</b>',
                                   '<b>' + sheetsLabel + '</b>');

  // Removing extra spaces from within the string.
  html = html.replace(/\s{2,}/g, ' ');
  printSummary.innerHTML = html;
}

/**
 * Handles a click event on the two-sided option.
 */
function handleTwoSidedClick() {
  updatePrintSummary();
}

/**
 * Gives focus to the individual pages textfield when 'print-pages' textbox is
 * clicked.
 */
function handleIndividualPagesCheckbox() {
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
  $('individual-pages').classList.remove('invalid');
  setDefaultValuesAndRegeneratePreview();
}

/**
 * Sets the default values and sends a request to regenerate preview data.
 */
function setDefaultValuesAndRegeneratePreview() {
  $('individual-pages').value = '';
  hideInvalidHint($('individual-pages-hint'));
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

  if ($('all-pages').checked || pageText.length == 0)
    pageText = '1-' + totalPageCount;

  var pageList = [];
  var parts = pageText.split(/,/);

  for (var i = 0; i < parts.length; ++i) {
    var part = parts[i];
    var match = part.match(/^\s*([0-9]+)\s*-\s*([0-9]+)\s*$/);

    if (match && match[1] && match[2]) {
      var from = parseInt(match[1], 10);
      var to = parseInt(match[2], 10);

      if (from && to) {
        for (var j = from; j <= to; ++j)
          if (j <= totalPageCount)
            pageList.push(j);
      }
    } else {
      var singlePageNumber = parseInt(part, 10);
      if (singlePageNumber && singlePageNumber > 0 &&
          singlePageNumber <= totalPageCount) {
        pageList.push(parseInt(part, 10));
      }
    }
  }
  return pageList;
}

/**
 * Checks the 'individual-pages' field and returns -1 if nothing is valid, 0 if
 * it is partially valid, 1 if it is completely valid.
 * Note: This function is stricter than getSelectedPages(), in other words this
 * could return -1 and getSelectedPages() might still extract some pages.
 */
function getSelectedPagesValidityLevel() {
  var pageText = $('individual-pages').value;

  if ($('all-pages').checked || pageText.length == 0)
    return 1;

  var successfullyParsed = 0;
  var failedToParse = 0;

  var parts = pageText.split(/,/);

  for (var i = 0; i < parts.length; ++i) {
    var part = parts[i].replace(/\s*/g, '');
    if (part.length == 0)
      continue;

    var match = part.match(/^([0-9]+)-([0-9]+)$/);

    if (match && match[1] && match[2]) {
      var from = parseInt(match[1], 10);
      var to = parseInt(match[2], 10);

      if (from && to && from < to && to <= totalPageCount)
        successfullyParsed += 1;
      else
        failedToParse += 1;

    } else if (isInteger(part) && parseInt(part, 10) <= totalPageCount)
      successfullyParsed += 1;
    else
      failedToParse += 1;
  }
  if (successfullyParsed > 0 && failedToParse == 0)
    return 1;
  else if (successfullyParsed > 0 && failedToParse > 0)
    return 0;
  else
    return -1;
}

function isSelectedPagesFieldValid() {
  return (getSelectedPages().length != 0)
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
  timerId = window.setTimeout(onPageSelectionMayHaveChanged, 1000);
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
 * 1) The input is compeletely valid (it can be parsed in its entirety).
 * 2) The newly selected pages differ from the previously selected.
 */
function onPageSelectionMayHaveChanged() {
  if ($('print-pages').checked)
    pageRangesFieldChanged();
  var validityLevel = getSelectedPagesValidityLevel();
  var currentlySelectedPages = getSelectedPagesSet();

  // Toggling between "all pages"/"some pages" radio buttons while having an
  // invalid entry in the page selection textfield still requires updating the
  // print summary and print button.
  if (validityLevel < 1 ||
      areArraysEqual(previouslySelectedPages, currentlySelectedPages)) {
    updatePrintButtonState();
    updatePrintSummary();
    return;
  }

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

/**
 * Executed when the 'increment' or 'decrement' button is clicked.
 */
function onCopiesButtonsClicked(sign) {
  var copiesField = $('copies');
  if (!isNumberOfCopiesValid())
    copiesField.value = 1;
  else {
    var newValue = getCopies() + sign * 1;
    if (newValue < copiesField.min || newValue > copiesField.max)
      return;
    copiesField.value = newValue;
  }
  copiesFieldChanged();
}

