// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/**
 * @typedef {{selectSaveAsPdfDestination: boolean,
 *            layoutSettings.portrait: boolean,
 *            pageRange: string,
 *            headersAndFooters: boolean,
 *            backgroundColorsAndImages: boolean,
 *            margins: number}}
 * @see chrome/browser/printing/print_preview_pdf_generated_browsertest.cc
 */
print_preview.PreviewSettings;

/**
 * @typedef {{
 *   deviceName: string,
 *   printerName: string,
 *   printerDescription: (string | undefined),
 *   cupsEnterprisePrinter: (boolean | undefined),
 *   printerOptions: (Object | undefined),
 * }}
 */
print_preview.LocalDestinationInfo;

/**
 * @typedef {{
 *   printerId: string,
 *   printerName: string,
 *   printerDescription: string,
 *   cupsEnterprisePrinter: (boolean | undefined),
 *   capabilities: !print_preview.Cdd,
 * }}
 */
print_preview.PrinterCapabilitiesResponse;

/**
 * @typedef {{
 *   serviceName: string,
 *   name: string,
 *   hasLocalPrinting: boolean,
 *   isUnregistered: boolean,
 *   cloudID: string,
 * }}
 * @see PrintPreviewHandler::FillPrinterDescription in print_preview_handler.cc
 */
print_preview.PrivetPrinterDescription;

/**
 * @typedef {{
 *   printer: !print_preview.PrivetPrinterDescription,
 *   capabilities: !print_preview.Cdd,
 * }}
 */
print_preview.PrivetPrinterCapabilitiesResponse;

/**
 * @typedef {{
 *   printerId: string,
 *   success: boolean,
 *   capabilities: Object,
 * }}
 */
print_preview.PrinterSetupResponse;

/**
 * @typedef {{
 *   extensionId: string,
 *   extensionName: string,
 *   id: string,
 *   name: string,
 *   description: (string|undefined),
 * }}
 */
print_preview.ProvisionalDestinationInfo;

/**
 * Printer types for capabilities and printer list requests.
 * Should match PrinterType in print_preview_handler.h
 * @enum {number}
 */
print_preview.PrinterType = {
  PRIVET_PRINTER: 0,
  EXTENSION_PRINTER: 1,
};

cr.define('print_preview', function() {
  'use strict';

  /**
   * An interface to the native Chromium printing system layer.
   */
  class NativeLayer {
    /**
     * Creates a new NativeLayer if the current instance is not set.
     * @return {!print_preview.NativeLayer} The singleton instance.
     */
    static getInstance() {
      if (currentInstance == null)
        currentInstance = new NativeLayer();
      return assert(currentInstance);
    }

    /**
     * @param {!print_preview.NativeLayer} instance The NativeLayer instance
     *     to set for print preview construction.
     */
    static setInstance(instance) {
      currentInstance = instance;
    }

    /**
     * Requests access token for cloud print requests.
     * @param {string} authType type of access token.
     * @return {!Promise<string>}
     */
    getAccessToken(authType) {
      return cr.sendWithPromise('getAccessToken', authType);
    }

    /**
     * Gets the initial settings to initialize the print preview with.
     * @return {!Promise<!print_preview.NativeInitialSettings>}
     */
    getInitialSettings() {
      return cr.sendWithPromise('getInitialSettings')
          .then(
              /**
               * @param {!Object} initialSettings Object containing the raw
               *     Print Preview settings.
               */
              function(initialSettings) {
                var numberFormatSymbols =
                    print_preview.MeasurementSystem.parseNumberFormat(
                        initialSettings['numberFormat']);
                var unitType = print_preview.MeasurementSystemUnitType.IMPERIAL;
                if (initialSettings['measurementSystem'] != null) {
                  unitType = initialSettings['measurementSystem'];
                }
                return new print_preview.NativeInitialSettings(
                    initialSettings['printAutomaticallyInKioskMode'] || false,
                    initialSettings['appKioskMode'] || false,
                    numberFormatSymbols[0] || ',',
                    numberFormatSymbols[1] || '.', unitType,
                    initialSettings['previewModifiable'] || false,
                    initialSettings['initiatorTitle'] || '',
                    initialSettings['documentHasSelection'] || false,
                    initialSettings['shouldPrintSelectionOnly'] || false,
                    initialSettings['printerName'] || null,
                    initialSettings['appState'] || null,
                    initialSettings['defaultDestinationSelectionRules'] ||
                        null);
              });
    }

    /**
     * Requests the system's local print destinations. The promise will be
     * resolved with a list of the local destinations.
     * @return {!Promise<!Array<print_preview.LocalDestinationInfo>>}
     */
    getPrinters() {
      return cr.sendWithPromise('getPrinters');
    }

    /**
     * Requests the network's privet print destinations. After this is called,
     * a number of privet-printer-changed events may be fired.
     * @return {!Promise} Resolves when privet printer search is completed.
     *     Rejected if privet printers are not enabled.
     */
    getPrivetPrinters() {
      return cr.sendWithPromise(
          'getExtensionOrPrivetPrinters',
          print_preview.PrinterType.PRIVET_PRINTER);
    }

    /**
     * Request a list of extension printers. Printers are reported as they are
     * found by a series of 'extension-printers-added' events.
     * @return {!Promise} Will be resolved when all extension managed printers
     *     have been sent.
     */
    getExtensionPrinters() {
      return cr.sendWithPromise(
          'getExtensionOrPrivetPrinters',
          print_preview.PrinterType.EXTENSION_PRINTER);
    }

    /**
     * Requests the destination's printing capabilities. Returns a promise that
     * will be resolved with the capabilities if they are obtained successfully.
     * @param {string} destinationId ID of the destination.
     * @return {!Promise<!print_preview.PrinterCapabilitiesResponse>}
     */
    getPrinterCapabilities(destinationId) {
      return cr.sendWithPromise('getPrinterCapabilities', destinationId);
    }

    /**
     * Requests the privet destination's printing capabilities. Returns a
     * promise that will be resolved with capabilities and printer information
     * if capabilities are obtained successfully.
     * @param {string} destinationId The ID of the destination
     * @return {!Promise<!print_preview.PrivetPrinterCapabilitiesResponse>}
     */
    getPrivetPrinterCapabilities(destinationId) {
      return cr.sendWithPromise(
          'getExtensionOrPrivetPrinterCapabilities', destinationId,
          print_preview.PrinterType.PRIVET_PRINTER);
    }

    /**
     * Requests the extension destination's printing capabilities. Returns a
     * promise that will be resolved with the capabilities if capabilities are
     * obtained successfully.
     * @param {string} destinationId The ID of the destination whose
     *     capabilities are requested.
     * @return {!Promise<!print_preview.Cdd>}
     */
    getExtensionPrinterCapabilities(destinationId) {
      return cr.sendWithPromise(
          'getExtensionOrPrivetPrinterCapabilities', destinationId,
          print_preview.PrinterType.EXTENSION_PRINTER);
    }

    /**
     * Requests Chrome to resolve provisional extension destination by granting
     * the provider extension access to the printer.
     * @param {string} provisionalDestinationId
     * @return {!Promise<!print_preview.ProvisionalDestinationInfo>}
     */
    grantExtensionPrinterAccess(provisionalDestinationId) {
      return cr.sendWithPromise('grantExtensionPrinterAccess',
                                provisionalDestinationId);
    }

    /**
     * Requests that Chrome peform printer setup for the given printer.
     * @param {string} printerId
     * @return {!Promise<!print_preview.PrinterSetupResponse>}
     */
    setupPrinter(printerId) {
      return cr.sendWithPromise('setupPrinter', printerId);
    }

    /**
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.ticket_items.Color} color Color ticket item.
     * @return {number} Native layer color model.
     * @private
     */
    getNativeColorModel_(destination, color) {
      // For non-local printers native color model is ignored anyway.
      var option = destination.isLocal ? color.getSelectedOption() : null;
      var nativeColorModel = parseInt(option ? option.vendor_id : null, 10);
      if (isNaN(nativeColorModel)) {
        return color.getValue() ? NativeLayer.ColorMode_.COLOR :
                                  NativeLayer.ColorMode_.GRAY;
      }
      return nativeColorModel;
    }

    /**
     * Requests that a preview be generated. The following Web UI events may
     * be triggered in response:
     *   'print-preset-options',
     *   'page-count-ready',
     *   'page-layout-ready',
     *   'page-preview-ready'
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to get the
     *     state of the print ticket.
     * @param {!print_preview.DocumentInfo} documentInfo Document data model.
     * @param {boolean} generateDraft Tell the renderer to re-render.
     * @param {number} requestId ID of the preview request.
     * @return {!Promise<number>} Promise that resolves with the unique ID of
     *     the preview UI when the preview has been generated.
     */
    getPreview(
        destination, printTicketStore, documentInfo, generateDraft, requestId) {
      assert(
          printTicketStore.isTicketValidForPreview(),
          'Trying to generate preview when ticket is not valid');

      var ticket = {
        'pageRange': printTicketStore.pageRange.getDocumentPageRanges(),
        'mediaSize': printTicketStore.mediaSize.getValue(),
        'landscape': printTicketStore.landscape.getValue(),
        'color': this.getNativeColorModel_(destination, printTicketStore.color),
        'headerFooterEnabled': printTicketStore.headerFooter.getValue(),
        'marginsType': printTicketStore.marginsType.getValue(),
        'isFirstRequest': requestId == 0,
        'requestID': requestId,
        'previewModifiable': documentInfo.isModifiable,
        'generateDraftData': generateDraft,
        'fitToPageEnabled': printTicketStore.fitToPage.getValue(),
        'scaleFactor': printTicketStore.scaling.getValueAsNumber(),
        // NOTE: Even though the following fields don't directly relate to the
        // preview, they still need to be included.
        // e.g. printing::PrintSettingsFromJobSettings() still checks for them.
        'collate': true,
        'copies': 1,
        'deviceName': destination.id,
        'dpiHorizontal': 'horizontal_dpi' in printTicketStore.dpi.getValue() ?
            printTicketStore.dpi.getValue().horizontal_dpi :
            0,
        'dpiVertical': 'vertical_dpi' in printTicketStore.dpi.getValue() ?
            printTicketStore.dpi.getValue().vertical_dpi :
            0,
        'duplex': printTicketStore.duplex.getValue() ?
            NativeLayer.DuplexMode.LONG_EDGE :
            NativeLayer.DuplexMode.SIMPLEX,
        'printToPDF': destination.id ==
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        'printWithCloudPrint': !destination.isLocal,
        'printWithPrivet': destination.isPrivet,
        'printWithExtension': destination.isExtension,
        'rasterizePDF': false,
        'shouldPrintBackgrounds': printTicketStore.cssBackground.getValue(),
        'shouldPrintSelectionOnly': printTicketStore.selectionOnly.getValue()
      };

      // Set 'cloudPrintID' only if the destination is not local.
      if (destination && !destination.isLocal) {
        ticket['cloudPrintID'] = destination.id;
      }

      if (printTicketStore.marginsType.isCapabilityAvailable() &&
          printTicketStore.marginsType.getValue() ==
              print_preview.ticket_items.MarginsTypeValue.CUSTOM) {
        var customMargins = printTicketStore.customMargins.getValue();
        var orientationEnum =
            print_preview.ticket_items.CustomMarginsOrientation;
        ticket['marginsCustom'] = {
          'marginTop': customMargins.get(orientationEnum.TOP),
          'marginRight': customMargins.get(orientationEnum.RIGHT),
          'marginBottom': customMargins.get(orientationEnum.BOTTOM),
          'marginLeft': customMargins.get(orientationEnum.LEFT)
        };
      }

      return cr.sendWithPromise(
          'getPreview', JSON.stringify(ticket),
          requestId > 0 ? documentInfo.pageCount : -1);
    }

    /**
     * Requests that the document be printed.
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to get the
     *     state of the print ticket.
     * @param {cloudprint.CloudPrintInterface} cloudPrintInterface Interface
     *     to Google Cloud Print.
     * @param {!print_preview.DocumentInfo} documentInfo Document data model.
     * @param {boolean=} opt_isOpenPdfInPreview Whether to open the PDF in the
     *     system's preview application.
     * @param {boolean=} opt_showSystemDialog Whether to open system dialog for
     *     advanced settings.
     * @return {!Promise} Promise that will resolve when the print request is
     *     finished or rejected.
     */
    print(
        destination, printTicketStore, cloudPrintInterface, documentInfo,
        opt_isOpenPdfInPreview, opt_showSystemDialog) {
      assert(
          printTicketStore.isTicketValid(),
          'Trying to print when ticket is not valid');

      assert(
          !opt_showSystemDialog || (cr.isWindows && destination.isLocal),
          'Implemented for Windows only');

      var ticket = {
        'mediaSize': printTicketStore.mediaSize.getValue(),
        'pageCount': printTicketStore.pageRange.getPageNumberSet().size,
        'landscape': printTicketStore.landscape.getValue(),
        'color': this.getNativeColorModel_(destination, printTicketStore.color),
        'headerFooterEnabled': false,  // Only used in print preview
        'marginsType': printTicketStore.marginsType.getValue(),
        'duplex': printTicketStore.duplex.getValue() ?
            NativeLayer.DuplexMode.LONG_EDGE :
            NativeLayer.DuplexMode.SIMPLEX,
        'copies': printTicketStore.copies.getValueAsNumber(),
        'collate': printTicketStore.collate.getValue(),
        'shouldPrintBackgrounds': printTicketStore.cssBackground.getValue(),
        'shouldPrintSelectionOnly': false,  // Only used in print preview
        'previewModifiable': documentInfo.isModifiable,
        'printToPDF': destination.id ==
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        'printWithCloudPrint': !destination.isLocal,
        'printWithPrivet': destination.isPrivet,
        'printWithExtension': destination.isExtension,
        'rasterizePDF': printTicketStore.rasterize.getValue(),
        'scaleFactor': printTicketStore.scaling.getValueAsNumber(),
        'dpiHorizontal': 'horizontal_dpi' in printTicketStore.dpi.getValue() ?
            printTicketStore.dpi.getValue().horizontal_dpi :
            0,
        'dpiVertical': 'vertical_dpi' in printTicketStore.dpi.getValue() ?
            printTicketStore.dpi.getValue().vertical_dpi :
            0,
        'deviceName': destination.id,
        'fitToPageEnabled': printTicketStore.fitToPage.getValue(),
        'pageWidth': documentInfo.pageSize.width,
        'pageHeight': documentInfo.pageSize.height,
        'showSystemDialog': opt_showSystemDialog
      };

      if (!destination.isLocal) {
        // We can't set cloudPrintID if the destination is "Print with Cloud
        // Print" because the native system will try to print to Google Cloud
        // Print with this ID instead of opening a Google Cloud Print dialog.
        ticket['cloudPrintID'] = destination.id;
      }

      if (printTicketStore.marginsType.isCapabilityAvailable() &&
          printTicketStore.marginsType.isValueEqual(
              print_preview.ticket_items.MarginsTypeValue.CUSTOM)) {
        var customMargins = printTicketStore.customMargins.getValue();
        var orientationEnum =
            print_preview.ticket_items.CustomMarginsOrientation;
        ticket['marginsCustom'] = {
          'marginTop': customMargins.get(orientationEnum.TOP),
          'marginRight': customMargins.get(orientationEnum.RIGHT),
          'marginBottom': customMargins.get(orientationEnum.BOTTOM),
          'marginLeft': customMargins.get(orientationEnum.LEFT)
        };
      }

      if (destination.isPrivet || destination.isExtension) {
        ticket['ticket'] = printTicketStore.createPrintTicket(destination);
        ticket['capabilities'] = JSON.stringify(destination.capabilities);
      }

      if (opt_isOpenPdfInPreview) {
        ticket['OpenPDFInPreview'] = true;
      }

      return cr.sendWithPromise('print', JSON.stringify(ticket));
    }

    /** Requests that the current pending print request be cancelled. */
    cancelPendingPrintRequest() {
      chrome.send('cancelPendingPrintRequest');
    }

    /**
     * Sends the app state to be saved in the sticky settings.
     * @param {string} appStateStr JSON string of the app state to persist
     */
    saveAppState(appStateStr) {
      chrome.send('saveAppState', [appStateStr]);
    }

    /** Shows the system's native printing dialog. */
    showSystemDialog() {
      assert(!cr.isWindows);
      chrome.send('showSystemDialog');
    }

    /**
     * Closes the print preview dialog.
     * If |isCancel| is true, also sends a message to Print Preview Handler in
     * order to update UMA statistics.
     * @param {boolean} isCancel whether this was called due to the user
     *     closing the dialog without printing.
     */
    dialogClose(isCancel) {
      if (isCancel)
        chrome.send('closePrintPreviewDialog');
      chrome.send('dialogClose');
    }

    /** Hide the print preview dialog and allow the native layer to close it. */
    hidePreview() {
      chrome.send('hidePreview');
    }

    /**
     * Opens the Google Cloud Print sign-in tab. The DESTINATIONS_RELOAD event
     *     will be dispatched in response.
     * @param {boolean} addAccount Whether to open an 'add a new account' or
     *     default sign in page.
     * @return {!Promise} Promise that resolves when the sign in tab has been
     *     closed and the destinations should be reloaded.
     */
    signIn(addAccount) {
      return cr.sendWithPromise('signIn', addAccount);
    }

    /** Navigates the user to the system printer settings interface. */
    manageLocalPrinters() {
      chrome.send('manageLocalPrinters');
    }

    /**
     * Navigates the user to the Google Cloud Print management page.
     * @param {?string} user Email address of the user to open the management
     *     page for (user must be currently logged in, indeed) or {@code null}
     *     to open this page for the primary user.
     */
    manageCloudPrinters(user) {
      chrome.send('manageCloudPrinters', [user || '']);
    }

    /** Forces browser to open a new tab with the given URL address. */
    forceOpenNewTab(url) {
      chrome.send('forceOpenNewTab', [url]);
    }

    /**
     * Sends a message to the test, letting it know that an
     * option has been set to a particular value and that the change has
     * finished modifying the preview area.
     */
    uiLoadedForTest() {
      chrome.send('UILoadedForTest');
    }

    /**
     * Notifies the test that the option it tried to change
     * had not been changed successfully.
     */
    uiFailedLoadingForTest() {
      chrome.send('UIFailedLoadingForTest');
    }

    /**
     * Notifies the metrics handler to record an action.
     * @param {string} action The action to record.
     */
    recordAction(action) {
      chrome.send('metricsHandler:recordAction', [action]);
    }

    /**
     * Notifies the metrics handler to record a histogram value.
     * @param {string} histogram The name of the histogram to record
     * @param {number} bucket The bucket to record
     * @param {number} maxBucket The maximum bucket value in the histogram.
     */
    recordInHistogram(histogram, bucket, maxBucket) {
      chrome.send(
          'metricsHandler:recordInHistogram', [histogram, bucket, maxBucket]);
    }
  }

  /** @private {?print_preview.NativeLayer} */
  var currentInstance = null;

  /**
   * Constant values matching printing::DuplexMode enum.
   * @enum {number}
   */
  NativeLayer.DuplexMode = {SIMPLEX: 0, LONG_EDGE: 1, UNKNOWN_DUPLEX_MODE: -1};

  /**
   * Enumeration of color modes used by Chromium.
   * @enum {number}
   * @private
   */
  NativeLayer.ColorMode_ = {GRAY: 1, COLOR: 2};

  /**
   * Version of the serialized state of the print preview.
   * @type {number}
   * @const
   * @private
   */
  NativeLayer.SERIALIZED_STATE_VERSION_ = 1;

  /**
   * Initial settings retrieved from the native layer.
   */
  class NativeInitialSettings {
    /**
     * @param {boolean} isInKioskAutoPrintMode Whether the print preview should
     *     be in auto-print mode.
     * @param {boolean} isInAppKioskMode Whether the print preview is in App
     *     Kiosk mode.
     * @param {string} thousandsDelimeter Character delimeter of thousands
     *     digits.
     * @param {string} decimalDelimeter Character delimeter of the decimal
     *     point.
     * @param {!print_preview.MeasurementSystemUnitType} unitType Unit type of
     *     local machine's measurement system.
     * @param {boolean} isDocumentModifiable Whether the document to print is
     *     modifiable.
     * @param {string} documentTitle Title of the document.
     * @param {boolean} documentHasSelection Whether the document has selected
     *     content.
     * @param {boolean} selectionOnly Whether only selected content should be
     *     printed.
     * @param {?string} systemDefaultDestinationId ID of the system default
     *     destination.
     * @param {?string} serializedAppStateStr Serialized app state.
     * @param {?string} serializedDefaultDestinationSelectionRulesStr Serialized
     *     default destination selection rules.
     */
    constructor(
        isInKioskAutoPrintMode, isInAppKioskMode, thousandsDelimeter,
        decimalDelimeter, unitType, isDocumentModifiable, documentTitle,
        documentHasSelection, selectionOnly, systemDefaultDestinationId,
        serializedAppStateStr, serializedDefaultDestinationSelectionRulesStr) {
      /**
       * Whether the print preview should be in auto-print mode.
       * @private {boolean}
       */
      this.isInKioskAutoPrintMode_ = isInKioskAutoPrintMode;

      /**
       * Whether the print preview should switch to App Kiosk mode.
       * @private {boolean}
       */
      this.isInAppKioskMode_ = isInAppKioskMode;

      /**
       * Character delimeter of thousands digits.
       * @private {string}
       */
      this.thousandsDelimeter_ = thousandsDelimeter;

      /**
       * Character delimeter of the decimal point.
       * @private {string}
       */
      this.decimalDelimeter_ = decimalDelimeter;

      /**
       * Unit type of local machine's measurement system.
       * @private {print_preview.MeasurementSystemUnitType}
       */
      this.unitType_ = unitType;

      /**
       * Whether the document to print is modifiable.
       * @private {boolean}
       */
      this.isDocumentModifiable_ = isDocumentModifiable;

      /**
       * Title of the document.
       * @private {string}
       */
      this.documentTitle_ = documentTitle;

      /**
       * Whether the document has selection.
       * @private {boolean}
       */
      this.documentHasSelection_ = documentHasSelection;

      /**
       * Whether selection only should be printed.
       * @private {boolean}
       */
      this.selectionOnly_ = selectionOnly;

      /**
       * ID of the system default destination.
       * @private {?string}
       */
      this.systemDefaultDestinationId_ = systemDefaultDestinationId;

      /**
       * Serialized app state.
       * @private {?string}
       */
      this.serializedAppStateStr_ = serializedAppStateStr;

      /**
       * Serialized default destination selection rules.
       * @private {?string}
       */
      this.serializedDefaultDestinationSelectionRulesStr_ =
          serializedDefaultDestinationSelectionRulesStr;
    }

    /**
     * @return {boolean} Whether the print preview should be in auto-print mode.
     */
    get isInKioskAutoPrintMode() {
      return this.isInKioskAutoPrintMode_;
    }

    /**
     * @return {boolean} Whether the print preview should switch to App Kiosk
     *     mode.
     */
    get isInAppKioskMode() {
      return this.isInAppKioskMode_;
    }

    /** @return {string} Character delimeter of thousands digits. */
    get thousandsDelimeter() {
      return this.thousandsDelimeter_;
    }

    /** @return {string} Character delimeter of the decimal point. */
    get decimalDelimeter() {
      return this.decimalDelimeter_;
    }

    /**
     * @return {!print_preview.MeasurementSystemUnitType} Unit type of local
     *     machine's measurement system.
     */
    get unitType() {
      return this.unitType_;
    }

    /** @return {boolean} Whether the document to print is modifiable. */
    get isDocumentModifiable() {
      return this.isDocumentModifiable_;
    }

    /** @return {string} Document title. */
    get documentTitle() {
      return this.documentTitle_;
    }

    /** @return {boolean} Whether the document has selection. */
    get documentHasSelection() {
      return this.documentHasSelection_;
    }

    /** @return {boolean} Whether selection only should be printed. */
    get selectionOnly() {
      return this.selectionOnly_;
    }

    /** @return {?string} ID of the system default destination. */
    get systemDefaultDestinationId() {
      return this.systemDefaultDestinationId_;
    }

    /** @return {?string} Serialized app state. */
    get serializedAppStateStr() {
      return this.serializedAppStateStr_;
    }

    /** @return {?string} Serialized default destination selection rules. */
    get serializedDefaultDestinationSelectionRulesStr() {
      return this.serializedDefaultDestinationSelectionRulesStr_;
    }
  }

  // Export
  return {
    NativeInitialSettings: NativeInitialSettings,
    NativeLayer: NativeLayer
  };
});
