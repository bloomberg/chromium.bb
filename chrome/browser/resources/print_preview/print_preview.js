// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();

// Store the last selected printer index.
var lastSelectedPrinterIndex = 0;

// Used to disable some printing options when the preview is not modifiable.
var previewModifiable = false;

// Destination list special value constants.
const PRINT_TO_PDF = 'Print To PDF';
const MANAGE_PRINTERS = 'Manage Printers';

// State of the print preview settings.
var printSettings = new PrintSettings();

// The name of the default or last used printer.
var defaultOrLastUsedPrinterName = '';

// True when a pending print preview request exists.
var hasPendingPreviewRequest = false;

// True when a pending print file request exists.
var hasPendingPrintFileRequest = false;

// True when preview tab is hidden.
var isTabHidden = false;

// Object holding all the pages related settings.
var pageSettings;

// Object holding all the copies related settings.
var copiesSettings;

// True if the user has click 'Advanced...' in order to open the system print
// dialog.
var showingSystemDialog = false;

/**
 * Window onload handler, sets up the page and starts print preview by getting
 * the printer list.
 */
function onLoad() {
  cr.enablePlatformSpecificCSSRules();

  $('cancel-button').addEventListener('click', handleCancelButtonClick);

  if (!checkCompatiblePluginExists()) {
    disableInputElementsInSidebar();
    displayErrorMessageWithButton(localStrings.getString('noPlugin'),
                                  localStrings.getString('launchNativeDialog'),
                                  launchNativePrintDialog);
    $('mainview').parentElement.removeChild($('dummy-viewer'));
    return;
  }

  $('print-button').focus();
  $('system-dialog-link').addEventListener('click', onSystemDialogLinkClicked);
  $('mainview').parentElement.removeChild($('dummy-viewer'));

  $('printer-list').disabled = true;
  $('print-button').onclick = printFile;

  pageSettings = print_preview.PageSettings.getInstance();
  copiesSettings = print_preview.CopiesSettings.getInstance();
  pageSettings.addEventListeners();
  copiesSettings.addEventListeners();

  showLoadingAnimation();
  chrome.send('getDefaultPrinter');
}

/**
 * Adds event listeners to the settings controls.
 */
function addEventListeners() {
  // Controls that require preview rendering.
  $('landscape').onclick = onLayoutModeToggle;
  $('portrait').onclick = onLayoutModeToggle;
  $('printer-list').onchange = updateControlsWithSelectedPrinterCapabilities;

  // Controls that do not require preview rendering.
  $('color').onclick = function() { setColor(true); };
  $('bw').onclick = function() { setColor(false); };
}

/**
 * Removes event listeners from the settings controls.
 */
function removeEventListeners() {
  clearTimeout(pageSettings.timerId_);

  // Controls that require preview rendering
  $('landscape').onclick = null;
  $('portrait').onclick = null;
  $('printer-list').onchange = null;

  // Controls that don't require preview rendering.
  $('color').onclick = null;
  $('bw').onclick = null;
}

/**
 * Disables the input elements in the sidebar.
 */
function disableInputElementsInSidebar() {
  var els = $('sidebar').querySelectorAll('input, button, select');
  for (var i = 0; i < els.length; i++)
    els[i].disabled = true;
}

/**
 * Asks the browser to close the preview tab.
 */
function handleCancelButtonClick() {
  chrome.send('closePrintPreviewTab');
}

/**
 * Disables the controls in the sidebar, shows the throbber and instructs the
 * backend to open the native print dialog.
 */
function onSystemDialogLinkClicked() {
  showingSystemDialog = true;
  disableInputElementsInSidebar();
  $('system-dialog-throbber').classList.remove('hidden');
  chrome.send('showSystemDialog');
}

/**
 * Similar to onSystemDialogLinkClicked(), but specific to the
 * 'Launch native print dialog' UI.
 */
function launchNativePrintDialog() {
  showingSystemDialog = true;
  $('error-button').disabled = true;
  $('native-print-dialog-throbber').classList.remove('hidden');
  chrome.send('showSystemDialog');
}

/**
 * Disables the controls which need the initiator tab to generate preview
 * data. This function is called when the initiator tab is closed.
 * @param {string} initiatorTabURL The URL of the initiator tab.
 */
function onInitiatorTabClosed(initiatorTabURL) {
  disableInputElementsInSidebar();
  displayErrorMessageWithButton(
      localStrings.getString('initiatorTabClosed'),
      localStrings.getString('reopenPage'),
      function() { window.location = initiatorTabURL; });
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
    updateWithPrinterCapabilities({
        'disableColorOption': true,
        'setColorAsDefault': true,
        'setDuplexAsDefault': false,
        'disableCopiesOption': true});
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
  var disableColorOption = settingInfo.disableColorOption;
  var disableCopiesOption = settingInfo.disableCopiesOption;
  var setColorAsDefault = settingInfo.setColorAsDefault;
  var setDuplexAsDefault = settingInfo.setDuplexAsDefault;
  var color = $('color');
  var bw = $('bw');
  var colorOptions = $('color-options');

  if (disableCopiesOption) {
    fadeOutElement($('copies-option'));
    $('hr-before-copies').classList.remove('invisible');
  } else {
    fadeInElement($('copies-option'));
    $('hr-before-copies').classList.add('invisible');
  }

  disableColorOption ? fadeOutElement(colorOptions) :
      fadeInElement(colorOptions);
  colorOptions.setAttribute('aria-hidden', disableColorOption);

  if (color.checked != setColorAsDefault) {
    color.checked = setColorAsDefault;
    bw.checked = !setColorAsDefault;
  }
  copiesSettings.twoSidedCheckbox.checked = setDuplexAsDefault;
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
  return !copiesSettings.collateOption.hidden && $('collate').checked;
}

/**
 * Gets the duplex mode for printing.
 * @return {number} duplex mode.
 */
function getDuplexMode() {
  // Constants values matches printing::DuplexMode enum.
  const SIMPLEX = 0;
  const LONG_EDGE = 1;
  return !copiesSettings.twoSidedCheckbox.checked ? SIMPLEX : LONG_EDGE;
}

/**
 * Creates a JSON string based on the values in the printer settings.
 *
 * @return {string} JSON string with print job settings.
 */
function getSettingsJSON() {
  var deviceName = getSelectedPrinterName();
  var printToPDF = (deviceName == PRINT_TO_PDF);

  return JSON.stringify(
      {'deviceName': deviceName,
       'pageRange': pageSettings.selectedPageRanges,
       'printAll': pageSettings.allPagesRadioButton.checked,
       'duplex': getDuplexMode(),
       'copies': copiesSettings.numberOfCopies,
       'collate': isCollated(),
       'landscape': isLandscape(),
       'color': isColor(),
       'printToPDF': printToPDF});
}

/**
 * Returns the name of the selected printer or the empty string if no
 * printer is selected.
 */
function getSelectedPrinterName() {
  var printerList = $('printer-list')
  var selectedPrinter = printerList.selectedIndex;
  if (selectedPrinter < 0)
    return '';
  return printerList.options[selectedPrinter].value;
}

/**
 * Asks the browser to print the preview PDF based on current print settings.
 * If the preview is still loading, printPendingFile() will get called once
 * the preview loads.
 */
function printFile() {
  hasPendingPrintFileRequest = hasPendingPreviewRequest;
  var printToPDF = getSelectedPrinterName() == PRINT_TO_PDF;

  if (hasPendingPrintFileRequest) {
    if (printToPDF) {
      // TODO(thestig) disable controls here.
    } else {
      isTabHidden = true;
      chrome.send('hidePreview');
    }
    return;
  }

  if (printToPDF) {
    sendPrintFileRequest();
  } else {
    $('print-button').classList.add('loading');
    $('cancel-button').classList.add('loading');
    $('print-summary').innerHTML = localStrings.getString('printing');
    removeEventListeners();
    window.setTimeout(function() { sendPrintFileRequest(); }, 1000);
  }
}

/**
 * Asks the browser to print the pending preview PDF that just finished loading.
 */
function printPendingFile() {
  hasPendingPrintFileRequest = false;

  if ($('print-button').disabled) {
    if (isTabHidden)
      cancelPendingPrintRequest();
    return;
  }

  sendPrintFileRequest();
}

/**
 * Sends a message to cancel the pending print request.
 */
function cancelPendingPrintRequest() {
  chrome.send('cancelPendingPrintRequest');
}

/**
 * Sends a message to initiate print workflow.
 */
function sendPrintFileRequest() {
  chrome.send('print', [getSettingsJSON()]);
}

/**
 * Asks the browser to generate a preview PDF based on current print settings.
 */
function requestPrintPreview() {
  hasPendingPreviewRequest = true;
  removeEventListeners();
  printSettings.save();
  if (!isTabHidden)
    showLoadingAnimation();

  chrome.send('getPreview', [getSettingsJSON()]);
}

/**
 * Called from PrintPreviewUI::OnFileSelectionCancelled to notify the print
 * preview tab regarding the file selection cancel event.
 */
function fileSelectionCancelled() {
  // TODO(thestig) re-enable controls here.
}

/**
 * Set the default printer. If there is one, generate a print preview.
 * @param {string} printer Name of the default printer. Empty if none.
 */
function setDefaultPrinter(printer) {
  // Add a placeholder value so the printer list looks valid.
  addDestinationListOption('', '', true, true);
  if (printer) {
    defaultOrLastUsedPrinterName = printer;
    $('printer-list')[0].value = defaultOrLastUsedPrinterName;
    updateControlsWithSelectedPrinterCapabilities();
  }
  chrome.send('getPrinters');
}

/**
 * Fill the printer list drop down.
 * Called from PrintPreviewHandler::SendPrinterList().
 * @param {Array} printers Array of printer info objects.
 */
function setPrinters(printers) {
  var printerList = $('printer-list');
  // If there exists a dummy printer value, then setDefaultPrinter() already
  // requested a preview, so no need to do it again.
  var needPreview = (printerList[0].value == '');
  for (var i = 0; i < printers.length; ++i) {
    var isDefault = (printers[i].deviceName == defaultOrLastUsedPrinterName);
    addDestinationListOption(printers[i].printerName, printers[i].deviceName,
                             isDefault, false);
  }

  // Remove the dummy printer added in setDefaultPrinter().
  printerList.remove(0);

  if (printers.length != 0)
    addDestinationListOption('', '', false, true);

  // Adding option for saving PDF to disk.
  addDestinationListOption(localStrings.getString('printToPDF'),
                           PRINT_TO_PDF,
                           defaultOrLastUsedPrinterName == PRINT_TO_PDF,
                           false);
  addDestinationListOption('', '', false, true);

  // Add an option to manage printers.
  addDestinationListOption(localStrings.getString('managePrinters'),
                           MANAGE_PRINTERS, false, false);

  printerList.disabled = false;

  if (needPreview)
    updateControlsWithSelectedPrinterCapabilities();
}

/**
 * Adds an option to the printer destination list.
 * @param {String} optionText specifies the option text content.
 * @param {String} optionValue specifies the option value.
 * @param {boolean} isDefault is true if the option needs to be selected.
 * @param {boolean} isSeparator is true if the option is a visual separator and
 *                  needs to be disabled.
 */
function addDestinationListOption(optionText, optionValue, isDefault,
    isSeparator) {
  var option = document.createElement('option');
  option.textContent = optionText;
  option.value = optionValue;
  $('printer-list').add(option);
  option.selected = isDefault;
  option.disabled = isSeparator;
  // Adding attribute for improved accessibility.
  if (isSeparator)
    option.setAttribute("role", "separator");
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
 * @param {string} errorMessage The error message to be displayed.
 */
function displayErrorMessage(errorMessage) {
  $('print-button').disabled = true;
  $('overlay-layer').classList.remove('invisible');
  $('dancing-dots-text').classList.add('hidden');
  $('error-text').innerHTML = errorMessage;
  $('error-text').classList.remove('hidden');
  removeEventListeners();
  var pdfViewer = $('pdf-viewer');
  if (pdfViewer)
    $('mainview').removeChild(pdfViewer);

  if (isTabHidden)
    cancelPendingPrintRequest();
}

/**
 * Display an error message in the center of the preview area followed by a
 * button.
 * @param {string} errorMessage The error message to be displayed.
 * @param {string} buttonText The text to be displayed within the button.
 * @param {string} buttonListener The listener to be executed when the button is
 * clicked.
 */
function displayErrorMessageWithButton(
    errorMessage, buttonText, buttonListener) {
  var errorButton = $('error-button');
  errorButton.disabled = false;
  errorButton.textContent = buttonText;
  errorButton.onclick = buttonListener;
  errorButton.classList.remove('hidden');
  $('system-dialog-throbber').classList.add('hidden');
  $('native-print-dialog-throbber').classList.add('hidden');
  displayErrorMessage(errorMessage);
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

  setColor($('color').checked);

  hideLoadingAnimation();

  if (!previewModifiable)
    fadeOutElement($('landscape-option'));

  cr.dispatchSimpleEvent(document, 'PDFLoaded');
}

/**
 * Update the page count and check the page range.
 * Called from PrintPreviewUI::OnDidGetPreviewPageCount().
 * @param {number} pageCount The number of pages.
 */
function onDidGetPreviewPageCount(pageCount) {
  pageSettings.updateState(pageCount);
}

/**
 * Notification that a print preview page has been rendered.
 * Check if the settings have changed and request a regeneration if needed.
 * Called from PrintPreviewUI::OnDidPreviewPage().
 * @param {number} pageNumber The page number, 0-based.
 */
function onDidPreviewPage(pageNumber) {
  if (checkIfSettingsChangedAndRegeneratePreview())
    return;
  // TODO(thestig) Make use of |pageNumber| for pipelined preview generation.
}

/**
 * Update the print preview when new preview data is available.
 * Create the PDF plugin as needed.
 * Called from PrintPreviewUI::PreviewDataIsAvailable().
 * @param {string} jobTitle The print job title.
 * @param {boolean} modifiable If the preview is modifiable.
 * @param {string} previewUid Preview unique identifier.
 */
function updatePrintPreview(jobTitle, modifiable, previewUid) {
  hasPendingPreviewRequest = false;

  if (checkIfSettingsChangedAndRegeneratePreview())
    return;

  previewModifiable = modifiable;
  document.title = localStrings.getStringF('printPreviewTitleFormat', jobTitle);

  createPDFPlugin(previewUid);
  updatePrintSummary();
  updatePrintButtonState();
  addEventListeners();

  if (hasPendingPrintFileRequest)
    printPendingFile();
}

/**
 * Check if any print settings changed and regenerate the preview if needed.
 * @return {boolean} true if a new preview is required.
 */
function checkIfSettingsChangedAndRegeneratePreview() {
  var tempPrintSettings = new PrintSettings();
  tempPrintSettings.save();

  if (printSettings.deviceName != tempPrintSettings.deviceName) {
    updateControlsWithSelectedPrinterCapabilities();
    return true;
  }
  if (printSettings.isLandscape != tempPrintSettings.isLandscape) {
    setDefaultValuesAndRegeneratePreview();
    return true;
  }
  if (pageSettings.requestPrintPreviewIfNeeded())
    return true;

  return false;
}

/**
 * Create the PDF plugin or reload the existing one.
 * @param {string} previewUid Preview unique identifier.
 */
function createPDFPlugin(previewUid) {
  var pdfViewer = $('pdf-viewer');
  if (pdfViewer) {
    // Need to call this before the reload(), where the plugin resets its
    // internal page count.
    pdfViewer.goToPage('0');

    pdfViewer.reload();
    pdfViewer.grayscale(!isColor());
    return;
  }

  pdfViewer = document.createElement('embed');
  pdfViewer.setAttribute('id', 'pdf-viewer');
  pdfViewer.setAttribute('type',
                         'application/x-google-chrome-print-preview-pdf');
  pdfViewer.setAttribute('src', 'chrome://print/' + previewUid + '/print.pdf');
  pdfViewer.setAttribute('aria-live', 'polite');
  pdfViewer.setAttribute('aria-atomic', 'true');
  $('mainview').appendChild(pdfViewer);
  pdfViewer.onload('onPDFLoad()');
  pdfViewer.removePrintButton();
  pdfViewer.grayscale(true);
}

/**
 * Returns true if a compatible pdf plugin exists, false if it doesn't.
 */
function checkCompatiblePluginExists() {
  var dummyPlugin = $('dummy-viewer')
  return (dummyPlugin.onload &&
          dummyPlugin.goToPage &&
          dummyPlugin.removePrintButton);
}

/**
 * Updates the state of print button depending on the user selection.
 * The button is enabled only when the following conditions are true.
 * 1) The selected page ranges are valid.
 * 2) The number of copies is valid (if applicable).
 */
function updatePrintButtonState() {
  if (showingSystemDialog)
    return;

  var settingsValid = pageSettings.isPageSelectionValid() &&
      (copiesSettings.isValid() || getSelectedPrinterName() == PRINT_TO_PDF);
  $('print-button').disabled = !settingsValid;
}

window.addEventListener('DOMContentLoaded', onLoad);

/**
 * Updates the print summary based on the currently selected user options.
 *
 */
function updatePrintSummary() {
  var printToPDF = getSelectedPrinterName() == PRINT_TO_PDF;
  var copies = printToPDF ? 1 : copiesSettings.numberOfCopies;
  var printSummary = $('print-summary');

  if (!printToPDF && !copiesSettings.isValid()) {
    printSummary.innerHTML = localStrings.getString('invalidNumberOfCopies');
    return;
  }

  if (!pageSettings.isPageSelectionValid()) {
    printSummary.innerHTML = '';
    return;
  }

  var pageSet = pageSettings.selectedPagesSet;
  var numOfSheets = pageSet.length;
  var summaryLabel = localStrings.getString('printPreviewSheetsLabelSingular');
  var numOfPagesText = '';
  var pagesLabel = localStrings.getString('printPreviewPageLabelPlural');

  if (printToPDF)
    summaryLabel = localStrings.getString('printPreviewPageLabelSingular');

  if (!printToPDF && copiesSettings.twoSidedCheckbox.checked)
    numOfSheets = Math.ceil(numOfSheets / 2);
  numOfSheets *= copies;

  if (numOfSheets > 1) {
    if (printToPDF)
      summaryLabel = pagesLabel;
    else
      summaryLabel = localStrings.getString('printPreviewSheetsLabelPlural');
  }

  var html = '';
  if (pageSet.length * copies != numOfSheets) {
    numOfPagesText = pageSet.length * copies;
    html = localStrings.getStringF('printPreviewSummaryFormatLong',
                                   '<b>' + numOfSheets + '</b>',
                                   '<b>' + summaryLabel + '</b>',
                                   numOfPagesText, pagesLabel);
  } else
    html = localStrings.getStringF('printPreviewSummaryFormatShort',
                                   '<b>' + numOfSheets + '</b>',
                                   '<b>' + summaryLabel + '</b>');

  // Removing extra spaces from within the string.
  html = html.replace(/\s{2,}/g, ' ');
  printSummary.innerHTML = html;
}

/**
 * When the user switches printing orientation mode the page field selection is
 * reset to "all pages selected". After the change the number of pages will be
 * different and currently selected page numbers might no longer be valid.
 * Even if they are still valid the content of these pages will be different.
 */
function onLayoutModeToggle() {
  // If the chosen layout is same as before, nothing needs to be done.
  if (printSettings.isLandscape == isLandscape())
    return;
  setDefaultValuesAndRegeneratePreview();
}

/**
 * Sets the default values and sends a request to regenerate preview data.
 */
function setDefaultValuesAndRegeneratePreview() {
  pageSettings.resetState();
  requestPrintPreview();
}

/**
 * Class that represents the state of the print settings.
 */
function PrintSettings() {
  this.deviceName = '';
  this.isLandscape = '';
}

/**
 * Takes a snapshot of the print settings.
 */
PrintSettings.prototype.save = function() {
  this.deviceName = getSelectedPrinterName();
  this.isLandscape = isLandscape();
}
