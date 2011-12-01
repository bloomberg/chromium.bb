// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: cr/ui/print_preview_cloud.js

var localStrings = new LocalStrings();

// If useCloudPrint is true we attempt to connect to cloud print
// and populate the list of printers with cloud print printers.
var useCloudPrint = false;

// Store the last selected printer index.
var lastSelectedPrinterIndex = 0;

// Used to disable some printing options when the preview is not modifiable.
var previewModifiable = false;

// Destination list special value constants.
const MANAGE_CLOUD_PRINTERS = 'manageCloudPrinters';
const MANAGE_LOCAL_PRINTERS = 'manageLocalPrinters';
const SIGN_IN = 'signIn';
const PRINT_TO_PDF = 'Print to PDF';
const PRINT_WITH_CLOUD_PRINT = 'printWithCloudPrint';

// State of the print preview settings.
var printSettings = new PrintSettings();

// Print ready data index.
const PRINT_READY_DATA_INDEX = -1;

// The name of the default or last used printer.
var defaultOrLastUsedPrinterName = '';

// True when a pending print preview request exists.
var hasPendingPreviewRequest = false;

// True when the first page is loaded in the plugin.
var isFirstPageLoaded = false;

// The ID of the last preview request.
var lastPreviewRequestID = -1;

// The ID of the initial preview request.
var initialPreviewRequestID = -1;

// True when a pending print file request exists.
var hasPendingPrintDocumentRequest = false;

// True when the complete metafile for the previewed doc is ready.
var isPrintReadyMetafileReady = false;

// True when preview tab is hidden.
var isTabHidden = false;

// @type {print_preview.PrintHeader} Holds the print and cancel buttons.
var printHeader;

// @type {print_preview.PageSettings} Holds all the pages related settings.
var pageSettings;

// @type {print_preview.CopiesSettings} Holds all the copies related settings.
var copiesSettings;

// @type {print_preview.LayoutSettings} Holds all the layout related settings.
var layoutSettings;

// @type {print_preview.MarginSettings} Holds all the margin related settings.
var marginSettings;

// @type {print_preview.HeaderFooterSettings} Holds all the header footer
//     related settings.
var headerFooterSettings;

// @type {print_preview.ColorSettings} Holds all the color related settings.
var colorSettings;

// @type {print_preview.PreviewArea} Holds information related to the preview
//     area (on the right).
var previewArea;

// True if the user has click 'Advanced...' in order to open the system print
// dialog.
var showingSystemDialog = false;

// True if the user has clicked 'Open PDF in Preview' option.
var previewAppRequested = false;

// The range of options in the printer dropdown controlled by cloud print.
var firstCloudPrintOptionPos = 0;
var lastCloudPrintOptionPos = firstCloudPrintOptionPos;

// Store the current previewUid.
var currentPreviewUid = '';

// True if we need to generate draft preview data.
var generateDraftData = true;

// A dictionary of cloud printers that have been added to the printer
// dropdown.
var addedCloudPrinters = {};

// The maximum number of cloud printers to allow in the dropdown.
const maxCloudPrinters = 10;

const MIN_REQUEST_ID = 0;
const MAX_REQUEST_ID = 32000;

// Names of all the custom events used.
var customEvents = {
  // Fired when the mouse moves while a margin line is being dragged.
  MARGIN_LINE_DRAG: 'marginLineDrag',
  // Fired when a mousedown event occurs on a margin line.
  MARGIN_LINE_MOUSE_DOWN: 'marginLineMouseDown',
  // Fired when a margin textbox gains focus.
  MARGIN_TEXTBOX_FOCUSED: 'marginTextboxFocused',
  // Fired when a new preview might be needed because of margin changes.
  MARGINS_MAY_HAVE_CHANGED: 'marginsMayHaveChanged',
  // Fired when the margins selection in the dropdown changes.
  MARGINS_SELECTION_CHANGED: 'marginsSelectionChanged',
  // Fired when a pdf generation related error occurs.
  PDF_GENERATION_ERROR: 'PDFGenerationError',
  // Fired once the first page of the pdf document is loaded in the plugin.
  PDF_LOADED: 'PDFLoaded',
  // Fired when the selected printer capabilities change.
  PRINTER_CAPABILITIES_UPDATED: 'printerCapabilitiesUpdated',
  // Fired when the print button needs to be updated.
  UPDATE_PRINT_BUTTON: 'updatePrintButton',
  // Fired when the print summary needs to be updated.
  UPDATE_SUMMARY: 'updateSummary'
};

/**
 * Window onload handler, sets up the page and starts print preview by getting
 * the printer list.
 */
function onLoad() {
  cr.enablePlatformSpecificCSSRules();
  initialPreviewRequestID = randomInteger(MIN_REQUEST_ID, MAX_REQUEST_ID);
  lastPreviewRequestID = initialPreviewRequestID;

  previewArea = print_preview.PreviewArea.getInstance();
  printHeader = print_preview.PrintHeader.getInstance();
  document.addEventListener(customEvents.PDF_GENERATION_ERROR,
                            cancelPendingPrintRequest);

  if (!checkCompatiblePluginExists()) {
    disableInputElementsInSidebar();
    $('cancel-button').focus();
    previewArea.displayErrorMessageWithButtonAndNotify(
        localStrings.getString('noPlugin'),
        localStrings.getString('launchNativeDialog'),
        launchNativePrintDialog);
    $('mainview').parentElement.removeChild($('dummy-viewer'));
    return;
  }

  $('system-dialog-link').addEventListener('click', onSystemDialogLinkClicked);
  if (cr.isMac) {
    $('open-pdf-in-preview-link').addEventListener(
        'click', onOpenPdfInPreviewLinkClicked);
  }
  $('mainview').parentElement.removeChild($('dummy-viewer'));

  $('printer-list').disabled = true;

  pageSettings = print_preview.PageSettings.getInstance();
  copiesSettings = print_preview.CopiesSettings.getInstance();
  layoutSettings = print_preview.LayoutSettings.getInstance();
  marginSettings = print_preview.MarginSettings.getInstance();
  headerFooterSettings = print_preview.HeaderFooterSettings.getInstance();
  colorSettings = print_preview.ColorSettings.getInstance();
  $('printer-list').onchange = updateControlsWithSelectedPrinterCapabilities;

  previewArea.showLoadingAnimation();
  chrome.send('getInitialSettings');
}

/**
 * @param {object} initialSettings An object containing all the initial
 *     settings.
 */
function setInitialSettings(initialSettings) {
  setInitiatorTabTitle(initialSettings['initiatorTabTitle']);
  previewModifiable = initialSettings['previewModifiable'];
  if (previewModifiable) {
    print_preview.MarginSettings.setNumberFormatAndMeasurementSystem(
        initialSettings['numberFormat'],
        initialSettings['measurementSystem']);
    marginSettings.setLastUsedMargins(initialSettings);
  }
  setDefaultPrinter(initialSettings['printerName'],
                    initialSettings['cloudPrintData']);
}

/**
 * Disables the input elements in the sidebar.
 */
function disableInputElementsInSidebar() {
  var els = $('navbar-container').querySelectorAll('input, button, select');
  for (var i = 0; i < els.length; i++) {
    if (els[i] == printHeader.cancelButton)
      continue;
    els[i].disabled = true;
  }
}

/**
 * Enables the input elements in the sidebar.
 */
function enableInputElementsInSidebar() {
  var els = $('navbar-container').querySelectorAll('input, button, select');
  for (var i = 0; i < els.length; i++)
    els[i].disabled = false;
}

/**
 * Disables the controls in the sidebar, shows the throbber and instructs the
 * backend to open the native print dialog.
 */
function onSystemDialogLinkClicked() {
  if (showingSystemDialog)
    return;
  showingSystemDialog = true;
  disableInputElementsInSidebar();
  printHeader.disableCancelButton();
  $('system-dialog-throbber').hidden = false;
  chrome.send('showSystemDialog');
}

/**
 * Disables the controls in the sidebar, shows the throbber and instructs the
 * backend to open the pdf in native preview app. This is only for Mac.
 */
function onOpenPdfInPreviewLinkClicked() {
  if (previewAppRequested)
    return;
  previewAppRequested = true;
  disableInputElementsInSidebar();
  $('open-preview-app-throbber').hidden = false;
  printHeader.disableCancelButton();
  requestToPrintDocument();
}

/**
 * Similar to onSystemDialogLinkClicked(), but specific to the
 * 'Launch native print dialog' UI.
 */
function launchNativePrintDialog() {
  if (showingSystemDialog)
    return;
  showingSystemDialog = true;
  previewArea.errorButton.disabled = true;
  printHeader.disableCancelButton();
  $('native-print-dialog-throbber').hidden = false;
  chrome.send('showSystemDialog');
}

/**
 * Gets the selected printer capabilities and updates the controls accordingly.
 */
function updateControlsWithSelectedPrinterCapabilities() {
  var printerList = $('printer-list');
  var selectedIndex = printerList.selectedIndex;
  if (selectedIndex < 0)
    return;
  if (cr.isMac)
    $('open-pdf-in-preview-link').disabled = false;

  var skip_refresh = false;
  var selectedValue = printerList.options[selectedIndex].value;
  if (cloudprint.isCloudPrint(printerList.options[selectedIndex])) {
    cloudprint.updatePrinterCaps(printerList.options[selectedIndex],
                                 doUpdateCloudPrinterCapabilities);
    skip_refresh = true;
  } else if (selectedValue == SIGN_IN ||
             selectedValue == MANAGE_CLOUD_PRINTERS ||
             selectedValue == MANAGE_LOCAL_PRINTERS) {
    printerList.selectedIndex = lastSelectedPrinterIndex;
    chrome.send(selectedValue);
    skip_refresh = true;
  } else if (selectedValue == PRINT_TO_PDF ||
             selectedValue == PRINT_WITH_CLOUD_PRINT) {
    updateWithPrinterCapabilities({
        'disableColorOption': true,
        'setColorAsDefault': true,
        'setDuplexAsDefault': false,
        'printerColorModelForColor': print_preview.ColorSettings.COLOR,
        'printerDefaultDuplexValue': copiesSettings.UNKNOWN_DUPLEX_MODE,
        'disableCopiesOption': true});
    if (cr.isChromeOS && selectedValue == PRINT_WITH_CLOUD_PRINT)
      sendPrintDocumentRequest();
  } else {
    // This message will call back to 'updateWithPrinterCapabilities'
    // function.
    chrome.send('getPrinterCapabilities', [selectedValue]);
  }
  if (!skip_refresh) {
    lastSelectedPrinterIndex = selectedIndex;

    // Regenerate the preview data based on selected printer settings.
    // Do not reset the margins if no preview request has been made.
    var resetMargins = lastPreviewRequestID != initialPreviewRequestID;
    setDefaultValuesAndRegeneratePreview(resetMargins);
  }
}

/**
 * Helper function to do the actual work of updating cloud printer
 * capabilities.
 * @param {Object} printer The printer object to set capabilities for.
 */
function doUpdateCloudPrinterCapabilities(printer) {
  var settings = {'disableColorOption': !cloudprint.supportsColor(printer),
                  'setColorAsDefault': cloudprint.colorIsDefault(printer),
                  'disableCopiesOption': true,
                  'disableLandscapeOption': true};
  updateWithPrinterCapabilities(settings);
  var printerList = $('printer-list');
  var selectedIndex = printerList.selectedIndex;
  lastSelectedPrinterIndex = selectedIndex;

  // Regenerate the preview data based on selected printer settings.
  // Do not reset the margins if no preview request has been made.
  var resetMargins = lastPreviewRequestID != initialPreviewRequestID;
  setDefaultValuesAndRegeneratePreview(resetMargins);
}

/**
 * Notifies listeners of |customEvents.PRINTER_CAPABILITIES_UPDATED| about the
 * capabilities of the currently selected printer. It is called from C++ too.
 * @param {Object} settingInfo printer setting information.
 */
function updateWithPrinterCapabilities(settingInfo) {
  var customEvent = new cr.Event(customEvents.PRINTER_CAPABILITIES_UPDATED);
  customEvent.printerCapabilities = settingInfo;
  document.dispatchEvent(customEvent);
}

/**
 * Reloads the printer list.
 */
function reloadPrintersList() {
  $('printer-list').length = 0;
  firstCloudPrintOptionPos = 0;
  lastCloudPrintOptionPos = 0;
  chrome.send('getPrinters');
}

/**
 * Turn on the integration of Cloud Print.
 * @param {string} cloudPrintURL The URL to use for cloud print servers.
 */
function setUseCloudPrint(cloudPrintURL) {
  useCloudPrint = true;
  cloudprint.setBaseURL(cloudPrintURL);
}

/**
 * Take the PDF data handed to us and submit it to the cloud, closing the print
 * preview tab once the upload is successful.
 * @param {string} data Data to send as the print job.
 */
function printToCloud(data) {
  cloudprint.printToCloud(data, finishedCloudPrinting);
}

/**
 * Cloud print upload of the PDF file is finished, time to close the dialog.
 */
function finishedCloudPrinting() {
  closePrintPreviewTab();
}

/**
 * Checks whether the specified settings are valid.
 *
 * @return {boolean} true if settings are valid, false if not.
 */
function areSettingsValid() {
  var selectedPrinter = getSelectedPrinterName();
  return pageSettings.isPageSelectionValid() &&
      marginSettings.areMarginSettingsValid() &&
      (copiesSettings.isValid() || selectedPrinter == PRINT_TO_PDF ||
       selectedPrinter == PRINT_WITH_CLOUD_PRINT);
}

/**
 * Creates an object based on the values in the printer settings.
 *
 * @return {Object} Object containing print job settings.
 */
function getSettings() {
  var deviceName = getSelectedPrinterName();
  var printToPDF = deviceName == PRINT_TO_PDF;
  var printWithCloudPrint = deviceName == PRINT_WITH_CLOUD_PRINT;

  var settings =
      {'deviceName': deviceName,
       'pageRange': pageSettings.selectedPageRanges,
       'duplex': copiesSettings.duplexMode,
       'copies': copiesSettings.numberOfCopies,
       'collate': copiesSettings.isCollated(),
       'landscape': layoutSettings.isLandscape(),
       'color': colorSettings.colorMode,
       'printToPDF': printToPDF,
       'printWithCloudPrint': printWithCloudPrint,
       'isFirstRequest' : false,
       'headerFooterEnabled': headerFooterSettings.hasHeaderFooter(),
       'marginsType': marginSettings.selectedMarginsValue,
       'requestID': -1,
       'generateDraftData': generateDraftData,
       'previewModifiable': previewModifiable};

  if (marginSettings.isCustomMarginsSelected())
    settings['marginsCustom'] = marginSettings.customMargins;

  var printerList = $('printer-list');
  var selectedPrinter = printerList.selectedIndex;
  if (cloudprint.isCloudPrint(printerList.options[selectedPrinter])) {
    settings['cloudPrintID'] =
        printerList.options[selectedPrinter].value;
  }
  return settings;
}

/**
 * Creates an object based on the values in the printer settings.
 * Note: |lastPreviewRequestID| is being modified every time this function is
 * called. Only call this function when a preview request is actually sent,
 * otherwise (for example when debugging) call getSettings().
 *
 * @return {Object} Object containing print job settings.
 */
function getSettingsWithRequestID() {
  var settings = getSettings();
  settings.requestID = generatePreviewRequestID();
  settings.isFirstRequest = isFirstPreviewRequest();
  return settings;
}

/**
 * @return {number} The next unused preview request id.
 */
function generatePreviewRequestID() {
  return ++lastPreviewRequestID;
}

/**
 * @return {boolean} True iff a preview has been requested.
 */
function hasRequestedPreview() {
  return lastPreviewRequestID != initialPreviewRequestID;
}

/**
 * @return {boolean} True if |lastPreviewRequestID| corresponds to the initial
 *     preview request.
 */
function isFirstPreviewRequest() {
  return lastPreviewRequestID == initialPreviewRequestID + 1;
}

/**
 * Checks if |previewResponseId| matches |lastPreviewRequestId|. Used to ignore
 * obsolete preview data responses.
 * @param {number} previewResponseId The id to check.
 * @return {boolean} True if previewResponseId reffers to the expected response.
 */
function isExpectedPreviewResponse(previewResponseId) {
  return lastPreviewRequestID == previewResponseId;
}

/**
 * Returns the name of the selected printer or the empty string if no
 * printer is selected.
 * @return {string} The name of the currently selected printer.
 */
function getSelectedPrinterName() {
  var printerList = $('printer-list');
  var selectedPrinter = printerList.selectedIndex;
  if (selectedPrinter < 0)
    return '';
  return printerList.options[selectedPrinter].value;
}

/**
 * Asks the browser to print the preview PDF based on current print
 * settings. If the preview is still loading, printPendingFile() will get
 * called once the preview loads.
 */
function requestToPrintDocument() {
  hasPendingPrintDocumentRequest = !isPrintReadyMetafileReady;
  var selectedPrinterName = getSelectedPrinterName();
  var printToPDF = selectedPrinterName == PRINT_TO_PDF;
  var printWithCloudPrint = selectedPrinterName == PRINT_WITH_CLOUD_PRINT;
  if (hasPendingPrintDocumentRequest) {
    if (previewAppRequested) {
      previewArea.showCustomMessage(
          localStrings.getString('openingPDFInPreview'));
    } else if (printToPDF) {
      sendPrintDocumentRequest();
    } else if (printWithCloudPrint) {
      previewArea.showCustomMessage(
          localStrings.getString('printWithCloudPrintWait'));
      disableInputElementsInSidebar();
    } else {
      isTabHidden = true;
      chrome.send('hidePreview');
    }
    return;
  }

  if (printToPDF || previewAppRequested) {
    sendPrintDocumentRequest();
  } else {
    window.setTimeout(function() { sendPrintDocumentRequest(); }, 1000);
  }
}

/**
 * Sends a message to cancel the pending print request.
 */
function cancelPendingPrintRequest() {
  if (isTabHidden)
    chrome.send('cancelPendingPrintRequest');
}

/**
 * Sends a message to initiate print workflow.
 */
function sendPrintDocumentRequest() {
  var printerList = $('printer-list');
  var printer = printerList[printerList.selectedIndex];
  chrome.send('saveLastPrinter', [printer.value, cloudprint.getData(printer)]);

  var settings = getSettings();
  if (cr.isMac && previewAppRequested)
    settings.OpenPDFInPreview = true;

  chrome.send('print', [JSON.stringify(settings),
                        cloudprint.getPrintTicketJSON(printer)]);
}

/**
 * Loads the selected preview pages.
 */
function loadSelectedPages() {
  pageSettings.updatePageSelection();
  var pageSet = pageSettings.previouslySelectedPages;
  var pageCount = pageSet.length;
  if (pageCount == 0 || currentPreviewUid == '')
    return;

  cr.dispatchSimpleEvent(document, customEvents.UPDATE_SUMMARY);
  for (var i = 0; i < pageCount; i++)
    onDidPreviewPage(pageSet[i] - 1, currentPreviewUid, lastPreviewRequestID);
}

/**
 * Asks the browser to generate a preview PDF based on current print settings.
 */
function requestPrintPreview() {
  if (!isTabHidden)
    previewArea.showLoadingAnimation();

  if (!hasPendingPreviewRequest && previewModifiable &&
      hasOnlyPageSettingsChanged()) {
    loadSelectedPages();
    generateDraftData = false;
  } else {
    hasPendingPreviewRequest = true;
    generateDraftData = true;
    pageSettings.updatePageSelection();
  }

  printSettings.save();
  layoutSettings.updateState();
  previewArea.resetState();
  isPrintReadyMetafileReady = false;
  isFirstPageLoaded = false;

  var totalPageCount = pageSettings.totalPageCount;
  if (!previewModifiable && totalPageCount > 0)
    generateDraftData = false;

  var pageCount = totalPageCount != undefined ? totalPageCount : -1;
  chrome.send('getPreview', [JSON.stringify(getSettingsWithRequestID()),
                             pageCount,
                             previewModifiable]);
}

/**
 * Called from PrintPreviewUI::OnFileSelectionCancelled to notify the print
 * preview tab regarding the file selection cancel event.
 */
function fileSelectionCancelled() {
  printHeader.enableCancelButton();
}

/**
 * Called from PrintPreviewUI::OnFileSelectionCompleted to notify the print
 * preview tab regarding the file selection completed event.
 */
function fileSelectionCompleted() {
  // If the file selection is completed and the tab is not already closed it
  // means that a pending print to pdf request exists.
  disableInputElementsInSidebar();
  previewArea.showCustomMessage(
      localStrings.getString('printingToPDFInProgress'));
}

/**
 * Set the default printer. If there is one, generate a print preview.
 * @param {string} printerName Name of the default printer. Empty if none.
 * @param {string} cloudPrintData Cloud print related data to restore if
 *     the default printer is a cloud printer.
 */
function setDefaultPrinter(printerName, cloudPrintData) {
  // Add a placeholder value so the printer list looks valid.
  addDestinationListOption('', '', true, true, true);
  if (printerName) {
    defaultOrLastUsedPrinterName = printerName;
    if (cloudPrintData) {
      cloudprint.setDefaultPrinter(printerName,
                                   cloudPrintData,
                                   addDestinationListOptionAtPosition,
                                   doUpdateCloudPrinterCapabilities);
    } else {
      $('printer-list')[0].value = defaultOrLastUsedPrinterName;
      updateControlsWithSelectedPrinterCapabilities();
    }
  }
  chrome.send('getPrinters');
}

/**
 * Fill the printer list drop down.
 * Called from PrintPreviewHandler::SetupPrinterList().
 * @param {Array} printers Array of printer info objects.
 */
function setPrinters(printers) {
  var printerList = $('printer-list');
  // Remove empty entry added by setDefaultPrinter.
  if (printerList[0] && printerList[0].textContent == '')
    printerList.remove(0);
  for (var i = 0; i < printers.length; ++i) {
    var isDefault = (printers[i].deviceName == defaultOrLastUsedPrinterName);
    addDestinationListOption(printers[i].printerName, printers[i].deviceName,
                             isDefault, false, false);
  }

  if (printers.length != 0)
    addDestinationListOption('', '', false, true, true);

  // Adding option for saving PDF to disk.
  addDestinationListOption(localStrings.getString('printToPDF'),
                           PRINT_TO_PDF,
                           defaultOrLastUsedPrinterName == PRINT_TO_PDF,
                           false,
                           false);
  addDestinationListOption('', '', false, true, true);
  if (useCloudPrint) {
    addDestinationListOption(localStrings.getString('printWithCloudPrint'),
                             PRINT_WITH_CLOUD_PRINT,
                             false,
                             false,
                             false);
    addDestinationListOption('', '', false, true, true);
  }
  // Add options to manage printers.
  if (!cr.isChromeOS) {
    addDestinationListOption(localStrings.getString('managePrinters'),
        MANAGE_LOCAL_PRINTERS, false, false, false);
  } else if (useCloudPrint) {
    // Fetch recent printers.
    cloudprint.fetchPrinters(addDestinationListOptionAtPosition, false);
    // Fetch the full printer list.
    cloudprint.fetchPrinters(addDestinationListOptionAtPosition, true);
    addDestinationListOption(localStrings.getString('managePrinters'),
        MANAGE_CLOUD_PRINTERS, false, false, false);
  }

  printerList.disabled = false;

  if (!hasRequestedPreview())
    updateControlsWithSelectedPrinterCapabilities();
}

/**
 * Creates an option that can be added to the printer destination list.
 * @param {string} optionText specifies the option text content.
 * @param {string} optionValue specifies the option value.
 * @param {boolean} isDefault is true if the option needs to be selected.
 * @param {boolean} isDisabled is true if the option needs to be disabled.
 * @param {boolean} isSeparator is true if the option is a visual separator and
 *     needs to be disabled.
 * @return {Object} The created option.
 */
function createDestinationListOption(optionText, optionValue, isDefault,
    isDisabled, isSeparator) {
  var option = document.createElement('option');
  option.textContent = optionText;
  option.value = optionValue;
  option.selected = isDefault;
  option.disabled = isSeparator || isDisabled;
  // Adding attribute for improved accessibility.
  if (isSeparator)
    option.setAttribute('role', 'separator');
  return option;
}

/**
 * Adds an option to the printer destination list.
 * @param {string} optionText specifies the option text content.
 * @param {string} optionValue specifies the option value.
 * @param {boolean} isDefault is true if the option needs to be selected.
 * @param {boolean} isDisabled is true if the option needs to be disabled.
 * @param {boolean} isSeparator is true if the option serves just as a
 *     separator.
 * @return {Object} The created option.
 */
function addDestinationListOption(optionText, optionValue, isDefault,
    isDisabled, isSeparator) {
  var option = createDestinationListOption(optionText,
                                           optionValue,
                                           isDefault,
                                           isDisabled,
                                           isSeparator);
  $('printer-list').add(option);
  return option;
}

/**
 * Adds an option to the printer destination list at a specified position.
 * @param {number} position The index in the printer-list wher the option
       should be created.
 * @param {string} optionText specifies the option text content.
 * @param {string} optionValue specifies the option value.
 * @param {boolean} isDefault is true if the option needs to be selected.
 * @param {boolean} isDisabled is true if the option needs to be disabled.
 * @param {boolean} isSeparator is true if the option is a visual separator and
 *     needs to be disabled.
 * @return {Object} The created option.
 */
function addDestinationListOptionAtPosition(position,
                                            optionText,
                                            optionValue,
                                            isDefault,
                                            isDisabled,
                                            isSeparator) {
  var option = createDestinationListOption(optionText,
                                           optionValue,
                                           isDefault,
                                           isDisabled,
                                           isSeparator);
  var printerList = $('printer-list');
  var before = printerList[position];
  printerList.add(option, before);
  return option;
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
  var printerList = $('printer-list');
  cloudprint.setColor(printerList[printerList.selectedIndex], color);
}

/**
 * Display an error message when print preview fails.
 * Called from PrintPreviewMessageHandler::OnPrintPreviewFailed().
 */
function printPreviewFailed() {
  previewArea.displayErrorMessageWithButtonAndNotify(
      localStrings.getString('previewFailed'),
      localStrings.getString('launchNativeDialog'),
      launchNativePrintDialog);
}

/**
 * Display an error message when encountered invalid printer settings.
 * Called from PrintPreviewMessageHandler::OnInvalidPrinterSettings().
 */
function invalidPrinterSettings() {
  if (cr.isMac) {
    if (previewAppRequested) {
      $('open-preview-app-throbber').hidden = true;
      previewArea.clearCustomMessageWithDots();
      previewAppRequested = false;
      hasPendingPrintDocumentRequest = false;
      enableInputElementsInSidebar();
    }
    $('open-pdf-in-preview-link').disabled = true;
  }
  previewArea.displayErrorMessageAndNotify(
      localStrings.getString('invalidPrinterSettings'));
}

/**
 * Called when the PDF plugin loads its document.
 */
function onPDFLoad() {
  if (previewModifiable) {
    setPluginPreviewPageCount();
  }
  cr.dispatchSimpleEvent(document, customEvents.PDF_LOADED);
  isFirstPageLoaded = true;
  checkAndHideOverlayLayerIfValid();
  sendPrintDocumentRequestIfNeeded();
}

function setPluginPreviewPageCount() {
  $('pdf-viewer').printPreviewPageCount(
      pageSettings.previouslySelectedPages.length);
}

/**
 * Update the page count and check the page range.
 * Called from PrintPreviewUI::OnDidGetPreviewPageCount().
 * @param {number} pageCount The number of pages.
 * @param {number} previewResponseId The preview request id that resulted in
 *     this response.
 */
function onDidGetPreviewPageCount(pageCount, previewResponseId) {
  if (!isExpectedPreviewResponse(previewResponseId))
    return;
  pageSettings.updateState(pageCount);
  if (!previewModifiable && pageSettings.requestPrintPreviewIfNeeded())
    return;

  cr.dispatchSimpleEvent(document, customEvents.UPDATE_SUMMARY);
}

/**
 * @param {printing::PageSizeMargins} pageLayout The default layout of the page
 *     in points.
 */
function onDidGetDefaultPageLayout(pageLayout) {
  marginSettings.currentDefaultPageLayout = new print_preview.PageLayout(
      pageLayout.contentWidth,
      pageLayout.contentHeight,
      pageLayout.marginLeft,
      pageLayout.marginTop,
      pageLayout.marginRight,
      pageLayout.marginBottom);
}

/**
 * This function sends a request to hide the overlay layer only if there is no
 * pending print document request and we are not waiting for the print ready
 * metafile.
 */
function checkAndHideOverlayLayerIfValid() {
  var selectedPrinter = getSelectedPrinterName();
  var printToDialog = selectedPrinter == PRINT_TO_PDF ||
      selectedPrinter == PRINT_WITH_CLOUD_PRINT;
  if ((printToDialog || !previewModifiable) &&
      !isPrintReadyMetafileReady && hasPendingPrintDocumentRequest) {
    return;
  }
  previewArea.hideOverlayLayer();
}

/**
 * Called when no pipelining previewed pages.
 * @param {string} previewUid Preview unique identifier.
 * @param {number} previewResponseId The preview request id that resulted in
 *     this response.
 */
function reloadPreviewPages(previewUid, previewResponseId) {
  if (!isExpectedPreviewResponse(previewResponseId))
    return;

  cr.dispatchSimpleEvent(document, customEvents.UPDATE_PRINT_BUTTON);
  checkAndHideOverlayLayerIfValid();
  var pageSet = pageSettings.previouslySelectedPages;
  for (var i = 0; i < pageSet.length; i++) {
    $('pdf-viewer').loadPreviewPage(
        getPageSrcURL(previewUid, pageSet[i] - 1), i);
  }

  hasPendingPreviewRequest = false;
  isPrintReadyMetafileReady = true;
  previewArea.pdfLoaded = true;
  sendPrintDocumentRequestIfNeeded();
}

/**
 * Notification that a print preview page has been rendered.
 * Check if the settings have changed and request a regeneration if needed.
 * Called from PrintPreviewUI::OnDidPreviewPage().
 * @param {number} pageNumber The page number, 0-based.
 * @param {string} previewUid Preview unique identifier.
 * @param {number} previewResponseId The preview request id that resulted in
 *     this response.
 */
function onDidPreviewPage(pageNumber, previewUid, previewResponseId) {
  if (!isExpectedPreviewResponse(previewResponseId))
    return;

  // Refactor
  if (!previewModifiable)
    return;

  if (pageSettings.requestPrintPreviewIfNeeded())
    return;

  var pageIndex = pageSettings.previouslySelectedPages.indexOf(pageNumber + 1);
  if (pageIndex == -1)
    return;

  currentPreviewUid = previewUid;
  if (pageIndex == 0)
    createPDFPlugin(pageNumber);

  $('pdf-viewer').loadPreviewPage(
      getPageSrcURL(previewUid, pageNumber), pageIndex);

  if (pageIndex + 1 == pageSettings.previouslySelectedPages.length) {
    hasPendingPreviewRequest = false;
    if (pageIndex != 0)
      sendPrintDocumentRequestIfNeeded();
  }
}

/**
 * Update the print preview when new preview data is available.
 * Create the PDF plugin as needed.
 * Called from PrintPreviewUI::PreviewDataIsAvailable().
 * @param {string} previewUid Preview unique identifier.
 * @param {number} previewResponseId The preview request id that resulted in
 *     this response.
 */
function updatePrintPreview(previewUid, previewResponseId) {
  if (!isExpectedPreviewResponse(previewResponseId))
    return;
  isPrintReadyMetafileReady = true;

  if (!previewModifiable) {
    // If the preview is not modifiable the plugin has not been created yet.
    currentPreviewUid = previewUid;
    hasPendingPreviewRequest = false;
    createPDFPlugin(PRINT_READY_DATA_INDEX);
  }

  cr.dispatchSimpleEvent(document, customEvents.UPDATE_PRINT_BUTTON);
  if (previewModifiable)
    sendPrintDocumentRequestIfNeeded();
}

/**
 * Checks to see if the requested print data is available for printing and
 * sends a print document request if needed.
 */
function sendPrintDocumentRequestIfNeeded() {
  if (!hasPendingPrintDocumentRequest || !isFirstPageLoaded)
    return;

  // If the selected printer is PRINT_TO_PDF or PRINT_WITH_CLOUD_PRINT or
  // the preview source is not modifiable, we need the print ready data for
  // printing. If the preview source is modifiable, we need to wait till all
  // the requested pages are loaded in the plugin for printing.
  var selectedPrinter = getSelectedPrinterName();
  var printToDialog = selectedPrinter == PRINT_TO_PDF ||
      selectedPrinter == PRINT_WITH_CLOUD_PRINT;
  if (((printToDialog || !previewModifiable) && !isPrintReadyMetafileReady) ||
      (previewModifiable && hasPendingPreviewRequest)) {
    return;
  }

  hasPendingPrintDocumentRequest = false;

  if (!areSettingsValid()) {
    cancelPendingPrintRequest();
    return;
  }
  sendPrintDocumentRequest();
}

/**
 * Check if only page selection has been changed since the last preview request
 * and is valid.
 * @return {boolean} true if the new page selection is valid.
 */
function hasOnlyPageSettingsChanged() {
  var tempPrintSettings = new PrintSettings();
  tempPrintSettings.save();

  return !!(printSettings.deviceName == tempPrintSettings.deviceName &&
            printSettings.isLandscape == tempPrintSettings.isLandscape &&
            printSettings.hasHeaderFooter ==
                tempPrintSettings.hasHeaderFooter &&
            pageSettings.hasPageSelectionChangedAndIsValid());
}

/**
 * Create the PDF plugin or reload the existing one.
 * @param {number} srcDataIndex Preview data source index.
 */
function createPDFPlugin(srcDataIndex) {
  var pdfViewer = $('pdf-viewer');
  var srcURL = getPageSrcURL(currentPreviewUid, srcDataIndex);
  if (pdfViewer) {
    // Need to call this before the reload(), where the plugin resets its
    // internal page count.
    pdfViewer.goToPage('0');
    pdfViewer.resetPrintPreviewUrl(srcURL);
    pdfViewer.reload();
    pdfViewer.grayscale(
        colorSettings.colorMode == print_preview.ColorSettings.GRAY);
    return;
  }

  pdfViewer = document.createElement('embed');
  pdfViewer.setAttribute('id', 'pdf-viewer');
  pdfViewer.setAttribute('type',
                         'application/x-google-chrome-print-preview-pdf');
  pdfViewer.setAttribute('src', srcURL);
  pdfViewer.setAttribute('aria-live', 'polite');
  pdfViewer.setAttribute('aria-atomic', 'true');
  $('mainview').appendChild(pdfViewer);
  pdfViewer.onload('onPDFLoad()');
  pdfViewer.onScroll('onPreviewPositionChanged()');
  pdfViewer.onPluginSizeChanged('onPreviewPositionChanged()');
  pdfViewer.removePrintButton();
  pdfViewer.grayscale(true);
}

/**
 * Called either when there is a scroll event or when the plugin size changes.
 */
function onPreviewPositionChanged() {
  marginSettings.onPreviewPositionChanged();
}

/**
 * @return {boolean} true if a compatible pdf plugin exists.
 */
function checkCompatiblePluginExists() {
  var dummyPlugin = $('dummy-viewer');
  var pluginInterface = [ dummyPlugin.onload,
                          dummyPlugin.goToPage,
                          dummyPlugin.removePrintButton,
                          dummyPlugin.loadPreviewPage,
                          dummyPlugin.printPreviewPageCount,
                          dummyPlugin.resetPrintPreviewUrl,
                          dummyPlugin.onPluginSizeChanged,
                          dummyPlugin.onScroll,
                          dummyPlugin.pageXOffset,
                          dummyPlugin.pageYOffset,
                          dummyPlugin.setZoomLevel,
                          dummyPlugin.setPageXOffset,
                          dummyPlugin.setPageYOffset,
                          dummyPlugin.getHorizontalScrollbarThickness,
                          dummyPlugin.getVerticalScrollbarThickness,
                          dummyPlugin.getPageLocationNormalized,
                          dummyPlugin.getHeight,
                          dummyPlugin.getWidth ];

  for (var i = 0; i < pluginInterface.length; i++) {
    if (!pluginInterface[i])
      return false;
  }
  return true;
}

/**
 * Sets the default values and sends a request to regenerate preview data.
 * Resets the margin options only if |resetMargins| is true.
 * @param {boolean} resetMargins True if margin settings should be resetted.
 */
function setDefaultValuesAndRegeneratePreview(resetMargins) {
  if (resetMargins)
    marginSettings.resetMarginsIfNeeded();
  pageSettings.resetState();
  requestPrintPreview();
}

/**
 * Class that represents the state of the print settings.
 */
function PrintSettings() {
  this.deviceName = '';
  this.isLandscape = '';
  this.hasHeaderFooter = '';
}

/**
 *  Takes a snapshot of the print settings.
 */
PrintSettings.prototype.save = function() {
  this.deviceName = getSelectedPrinterName();
  this.isLandscape = layoutSettings.isLandscape();
  this.hasHeaderFooter = headerFooterSettings.hasHeaderFooter();
};

/**
 * Updates the title of the print preview tab according to |initiatorTabTitle|.
 * @param {string} initiatorTabTitle The title of the initiator tab.
 */
function setInitiatorTabTitle(initiatorTabTitle) {
  if (initiatorTabTitle == '')
    return;
  document.title = localStrings.getStringF(
      'printPreviewTitleFormat', initiatorTabTitle);
}

/**
 * Closes this print preview tab.
 */
function closePrintPreviewTab() {
  chrome.send('closePrintPreviewTab');
  chrome.send('DialogClose');
}

/**
  * Handle keyboard events.
  * @param {KeyboardEvent} e The keyboard event.
  */
function onKeyDown(e) {
  // Escape key closes the dialog.
  if (e.keyCode == 27 && !e.shiftKey && !e.ctrlKey && !e.altKey && !e.metaKey) {
    printHeader.disableCancelButton();
    closePrintPreviewTab();
  }
  if (e.keyCode == 80) {
    if ((cr.isMac && e.metaKey && e.altKey && !e.shiftKey && !e.ctrlKey) ||
        (!cr.isMac && e.shiftKey && e.ctrlKey && !e.altKey && !e.metaKey)) {
      window.onkeydown = null;
      onSystemDialogLinkClicked();
    }
  }
}

window.addEventListener('DOMContentLoaded', onLoad);
window.onkeydown = onKeyDown;

/// Pull in all other scripts in a single shot.
<include src="print_preview_animations.js"/>
<include src="print_preview_cloud.js"/>
<include src="print_preview_utils.js"/>
<include src="print_header.js"/>
<include src="page_settings.js"/>
<include src="copies_settings.js"/>
<include src="header_footer_settings.js"/>
<include src="layout_settings.js"/>
<include src="color_settings.js"/>
<include src="margin_settings.js"/>
<include src="margin_textbox.js"/>
<include src="margin_utils.js"/>
<include src="margins_ui.js"/>
<include src="margins_ui_pair.js"/>
<include src="preview_area.js"/>
