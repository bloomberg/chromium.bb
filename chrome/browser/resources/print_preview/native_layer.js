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
 *   isInKioskAutoPrintMode: boolean,
 *   isInAppKioskMode: boolean,
 *   thousandsDelimeter: string,
 *   decimalDelimeter: string,
 *   unitType: !print_preview.MeasurementSystemUnitType,
 *   previewModifiable: boolean,
 *   documentTitle: string,
 *   documentHasSelection: boolean,
 *   shouldPrintSelectionOnly: boolean,
 *   printerName: string,
 *   serializedAppStateStr: ?string,
 *   serializedDefaultDestinationSelectionRulesStr: ?string,
 * }}
 * @see corresponding field name definitions in print_preview_handler.cc
 */
print_preview.NativeInitialSettings;

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
 *   printer:(print_preview.PrivetPrinterDescription |
 *            print_preview.LocalDestinationInfo |
 *            undefined),
 *   capabilities: !print_preview.Cdd,
 * }}
 */
print_preview.CapabilitiesResponse;

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
  PDF_PRINTER: 2,
  LOCAL_PRINTER: 3,
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
      return cr.sendWithPromise('getInitialSettings');
    }

    /**
     * Requests the system's print destinations. The promise will be resolved
     * when all destinations of that type have been retrieved. One or more
     * 'printers-added' events may be fired in response before resolution.
     * @param {!print_preview.PrinterType} type The type of destinations to
     *     request.
     * @return {!Promise}
     */
    getPrinters(type) {
      return cr.sendWithPromise('getPrinters', type);
    }

    /**
     * Requests the destination's printing capabilities. Returns a promise that
     * will be resolved with the capabilities if they are obtained successfully.
     * @param {string} destinationId ID of the destination.
     * @param {!print_preview.PrinterType} type The destination's printer type.
     * @return {!Promise<!print_preview.CapabilitiesResponse>}
     */
    getPrinterCapabilities(destinationId, type) {
      return cr.sendWithPromise(
          'getPrinterCapabilities', destinationId,
          destinationId ==
                  print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ?
              print_preview.PrinterType.PDF_PRINTER :
              type);
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

      // Note: update
      // chrome/browser/ui/webui/print_preview/print_preview_handler_unittest.cc
      // with any changes to ticket creation.
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

  // Export
  return {
    NativeLayer: NativeLayer
  };
});
