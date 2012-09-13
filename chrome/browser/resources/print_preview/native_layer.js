// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * An interface to the native Chromium printing system layer.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function NativeLayer() {
    cr.EventTarget.call(this);

    // Bind global handlers
    global['setInitialSettings'] = this.onSetInitialSettings_.bind(this);
    global['setUseCloudPrint'] = this.onSetUseCloudPrint_.bind(this);
    global['setPrinters'] = this.onSetPrinters_.bind(this);
    global['updateWithPrinterCapabilities'] =
        this.onUpdateWithPrinterCapabilities_.bind(this);
    global['failedToGetPrinterCapabilities'] =
        this.onFailedToGetPrinterCapabilities_.bind(this);
    global['reloadPrintersList'] = this.onReloadPrintersList_.bind(this);
    global['printToCloud'] = this.onPrintToCloud_.bind(this);
    global['fileSelectionCancelled'] =
        this.onFileSelectionCancelled_.bind(this);
    global['fileSelectionCompleted'] =
        this.onFileSelectionCompleted_.bind(this);
    global['printPreviewFailed'] = this.onPrintPreviewFailed_.bind(this);
    global['invalidPrinterSettings'] =
        this.onInvalidPrinterSettings_.bind(this);
    global['onDidGetDefaultPageLayout'] =
        this.onDidGetDefaultPageLayout_.bind(this);
    global['onDidGetPreviewPageCount'] =
        this.onDidGetPreviewPageCount_.bind(this);
    global['reloadPreviewPages'] = this.onReloadPreviewPages_.bind(this);
    global['onDidPreviewPage'] = this.onDidPreviewPage_.bind(this);
    global['updatePrintPreview'] = this.onUpdatePrintPreview_.bind(this);
    global['printScalingDisabledForSourcePDF'] =
        this.onPrintScalingDisabledForSourcePDF_.bind(this);
  };

  /**
   * Event types dispatched from the Chromium native layer.
   * @enum {string}
   * @const
   */
  NativeLayer.EventType = {
    CAPABILITIES_SET: 'print_preview.NativeLayer.CAPABILITIES_SET',
    CLOUD_PRINT_ENABLE: 'print_preview.NativeLayer.CLOUD_PRINT_ENABLE',
    DESTINATIONS_RELOAD: 'print_preview.NativeLayer.DESTINATIONS_RELOAD',
    DISABLE_SCALING: 'print_preview.NativeLayer.DISABLE_SCALING',
    FILE_SELECTION_CANCEL: 'print_preview.NativeLayer.FILE_SELECTION_CANCEL',
    FILE_SELECTION_COMPLETE:
        'print_preview.NativeLayer.FILE_SELECTION_COMPLETE',
    GET_CAPABILITIES_FAIL: 'print_preview.NativeLayer.GET_CAPABILITIES_FAIL',
    INITIAL_SETTINGS_SET: 'print_preview.NativeLayer.INITIAL_SETTINGS_SET',
    LOCAL_DESTINATIONS_SET: 'print_preview.NativeLayer.LOCAL_DESTINATIONS_SET',
    PAGE_COUNT_READY: 'print_preview.NativeLayer.PAGE_COUNT_READY',
    PAGE_LAYOUT_READY: 'print_preview.NativeLayer.PAGE_LAYOUT_READY',
    PAGE_PREVIEW_READY: 'print_preview.NativeLayer.PAGE_PREVIEW_READY',
    PREVIEW_GENERATION_DONE:
        'print_preview.NativeLayer.PREVIEW_GENERATION_DONE',
    PREVIEW_GENERATION_FAIL:
        'print_preview.NativeLayer.PREVIEW_GENERATION_FAIL',
    PREVIEW_RELOAD: 'print_preview.NativeLayer.PREVIEW_RELOAD',
    PRINT_TO_CLOUD: 'print_preview.NativeLayer.PRINT_TO_CLOUD',
    SETTINGS_INVALID: 'print_preview.NativeLayer.SETTINGS_INVALID'
  };

  /**
   * Constant values matching printing::DuplexMode enum.
   * @enum {number}
   */
  NativeLayer.DuplexMode = {
    SIMPLEX: 0,
    LONG_EDGE: 1,
    UNKNOWN_DUPLEX_MODE: -1
  };

  /**
   * Enumeration of color modes used by Chromium.
   * @enum {number}
   * @private
   */
  NativeLayer.ColorMode_ = {
    GRAY: 1,
    COLOR: 2
  };

  /**
   * Version of the serialized state of the print preview.
   * @type {number}
   * @const
   * @private
   */
  NativeLayer.SERIALIZED_STATE_VERSION_ = 1;

  NativeLayer.prototype = {
    __proto__: cr.EventTarget.prototype,

    /** Gets the initial settings to initialize the print preview with. */
    startGetInitialSettings: function() {
      chrome.send('getInitialSettings');
    },

    /**
     * Requests the system's local print destinations. A LOCAL_DESTINATIONS_SET
     * event will be dispatched in response.
     */
    startGetLocalDestinations: function() {
      chrome.send('getPrinters');
    },

    /**
     * Requests the destination's printing capabilities. A CAPABILITIES_SET
     * event will be dispatched in response.
     * @param {string} destinationId ID of the destination.
     */
    startGetLocalDestinationCapabilities: function(destinationId) {
      chrome.send('getPrinterCapabilities', [destinationId]);
    },

    /**
     * Requests that a preview be generated. The following events may be
     * dispatched in response:
     *   - PAGE_COUNT_READY
     *   - PAGE_LAYOUT_READY
     *   - PAGE_PREVIEW_READY
     *   - PREVIEW_GENERATION_DONE
     *   - PREVIEW_GENERATION_FAIL
     *   - PREVIEW_RELOAD
     * @param {print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to get the
     *     state of the print ticket.
     * @param {number} ID of the preview request.
     */
    startGetPreview: function(destination, printTicketStore, requestId) {
      assert(printTicketStore.isTicketValidForPreview(),
             'Trying to generate preview when ticket is not valid');

      var pageRanges =
          (requestId > 0 && printTicketStore.hasPageRangeCapability()) ?
          printTicketStore.getPageNumberSet().getPageRanges() : [];

      var ticket = {
        'pageRange': pageRanges,
        'landscape': printTicketStore.isLandscapeEnabled(),
        'color': printTicketStore.isColorEnabled() ?
            NativeLayer.ColorMode_.COLOR : NativeLayer.ColorMode_.GRAY,
        'headerFooterEnabled': printTicketStore.isHeaderFooterEnabled(),
        'marginsType': printTicketStore.getMarginsType(),
        'isFirstRequest': requestId == 0,
        'requestID': requestId,
        'previewModifiable': printTicketStore.isDocumentModifiable,
        'printToPDF':
            destination != null &&
            destination.id ==
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        'printWithCloudPrint': destination != null && !destination.isLocal,
        'deviceName': destination == null ? 'foo' : destination.id,
        'generateDraftData': printTicketStore.isDocumentModifiable,
        'fitToPageEnabled': printTicketStore.isFitToPageEnabled(),

        // NOTE: Even though the following fields don't directly relate to the
        // preview, they still need to be included.
        'duplex': printTicketStore.isDuplexEnabled() ?
            NativeLayer.DuplexMode.LONG_EDGE : NativeLayer.DuplexMode.SIMPLEX,
        'copies': printTicketStore.getCopies(),
        'collate': printTicketStore.isCollateEnabled()
      };

      // Set 'cloudPrintID' only if the destination is not local.
      if (!destination.isLocal) {
        ticket['cloudPrintID'] = destination.id;
      }

      if (printTicketStore.hasMarginsCapability() &&
          printTicketStore.getMarginsType() ==
              print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        var customMargins = printTicketStore.getCustomMargins();
        var orientationEnum =
            print_preview.ticket_items.CustomMargins.Orientation;
        ticket['marginsCustom'] = {
          'marginTop': customMargins.get(orientationEnum.TOP),
          'marginRight': customMargins.get(orientationEnum.RIGHT),
          'marginBottom': customMargins.get(orientationEnum.BOTTOM),
          'marginLeft': customMargins.get(orientationEnum.LEFT)
        };
      }

      chrome.send(
          'getPreview',
          [JSON.stringify(ticket),
           requestId > 0 ? printTicketStore.pageCount : -1,
           printTicketStore.isDocumentModifiable]);
    },

    /**
     * Requests that the document be printed.
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to get the
     *     state of the print ticket.
     * @param {print_preview.CloudPrintInterface} cloudPrintInterface Interface
     *     to Google Cloud Print.
     * @param {boolean=} opt_isOpenPdfInPreview Whether to open the PDF in the
     *     system's preview application.
     */
    startPrint: function(destination, printTicketStore, cloudPrintInterface,
                         opt_isOpenPdfInPreview) {
      assert(printTicketStore.isTicketValid(),
             'Trying to print when ticket is not valid');

      var ticket = {
        'pageRange': printTicketStore.hasPageRangeCapability() ?
            printTicketStore.getPageNumberSet().getPageRanges() : [],
        'landscape': printTicketStore.isLandscapeEnabled(),
        'color': printTicketStore.isColorEnabled() ?
            NativeLayer.ColorMode_.COLOR : NativeLayer.ColorMode_.GRAY,
        'headerFooterEnabled': printTicketStore.isHeaderFooterEnabled(),
        'marginsType': printTicketStore.getMarginsType(),
        'generateDraftData': true, // TODO(rltoscano): What should this be?
        'duplex': printTicketStore.isDuplexEnabled() ?
            NativeLayer.DuplexMode.LONG_EDGE : NativeLayer.DuplexMode.SIMPLEX,
        'copies': printTicketStore.getCopies(),
        'collate': printTicketStore.isCollateEnabled(),
        'previewModifiable': printTicketStore.isDocumentModifiable,
        'printToPDF': destination.id ==
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        'printWithCloudPrint': !destination.isLocal,
        'deviceName': destination.id,
        'isFirstRequest': false,
        'requestID': -1,
        'fitToPageEnabled': printTicketStore.isFitToPageEnabled()
      };

      if (!destination.isLocal) {
        // We can't set cloudPrintID if the destination is "Print with Cloud
        // Print" because the native system will try to print to Google Cloud
        // Print with this ID instead of opening a Google Cloud Print dialog.
        ticket['cloudPrintID'] = destination.id;
      }

      if (printTicketStore.hasMarginsCapability() &&
          printTicketStore.getMarginsType() ==
              print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        var customMargins = printTicketStore.getCustomMargins();
        var orientationEnum =
            print_preview.ticket_items.CustomMargins.Orientation;
        ticket['marginsCustom'] = {
          'marginTop': customMargins.get(orientationEnum.TOP),
          'marginRight': customMargins.get(orientationEnum.RIGHT),
          'marginBottom': customMargins.get(orientationEnum.BOTTOM),
          'marginLeft': customMargins.get(orientationEnum.LEFT)
        };
      }

      if (opt_isOpenPdfInPreview) {
        ticket['OpenPDFInPreview'] = true;
      }

      var cloudTicket = null;
      if (!destination.isLocal) {
        assert(cloudPrintInterface != null,
               'Trying to print to a cloud destination but Google Cloud ' +
               'Print integration is disabled');
        cloudTicket = cloudPrintInterface.createPrintTicket(
            destination, printTicketStore);
        cloudTicket = JSON.stringify(cloudTicket);
      }

      chrome.send('print', [JSON.stringify(ticket), cloudTicket]);
    },

    /** Requests that the current pending print request be cancelled. */
    startCancelPendingPrint: function() {
      chrome.send('cancelPendingPrintRequest');
    },

    /** Shows the system's native printing dialog. */
    startShowSystemDialog: function() {
      chrome.send('showSystemDialog');
    },

    /** Shows Google Cloud Print's web-based print dialog. */
    startShowCloudPrintDialog: function() {
      chrome.send('printWithCloudPrint');
    },

    /** Closes the print preview dialog. */
    startCloseDialog: function() {
      chrome.send('closePrintPreviewTab');
      chrome.send('DialogClose');
    },

    /** Hide the print preview dialog and allow the native layer to close it. */
    startHideDialog: function() {
      chrome.send('hidePreview');
    },

    /**
     * Opens the Google Cloud Print sign-in dialog. The DESTINATIONS_RELOAD
     * event will be dispatched in response.
     */
    startCloudPrintSignIn: function() {
      chrome.send('signIn');
    },

    /** Navigates the user to the system printer settings interface. */
    startManageLocalDestinations: function() {
      chrome.send('manageLocalPrinters');
    },

    /** Navigates the user to the Google Cloud Print management page. */
    startManageCloudDestinations: function() {
      chrome.send('manageCloudPrinters');
    },

    /**
     * @param {!Object} initialSettings Object containing all initial settings.
     */
    onSetInitialSettings_: function(initialSettings) {
      var numberFormatSymbols =
          print_preview.MeasurementSystem.parseNumberFormat(
              initialSettings['numberFormat']);
      var unitType = print_preview.MeasurementSystem.UnitType.IMPERIAL;
      if (initialSettings['measurementSystem'] != null) {
        unitType = initialSettings['measurementSystem'];
      }

      var nativeInitialSettings = new print_preview.NativeInitialSettings(
          initialSettings['printAutomaticallyInKioskMode'] || false,
          numberFormatSymbols[0] || ',',
          numberFormatSymbols[1] || '.',
          unitType,
          initialSettings['previewModifiable'] || false,
          initialSettings['initiatorTabTitle'] || '',
          initialSettings['printerName'] || null,
          initialSettings['appState'] || null);

      var initialSettingsSetEvent = new cr.Event(
          NativeLayer.EventType.INITIAL_SETTINGS_SET);
      initialSettingsSetEvent.initialSettings = nativeInitialSettings;
      this.dispatchEvent(initialSettingsSetEvent);
    },

    /**
     * Turn on the integration of Cloud Print.
     * @param {string} cloudPrintURL The URL to use for cloud print servers.
     * @private
     */
    onSetUseCloudPrint_: function(cloudPrintURL) {
      var cloudPrintEnableEvent = new cr.Event(
          NativeLayer.EventType.CLOUD_PRINT_ENABLE);
      cloudPrintEnableEvent.baseCloudPrintUrl = cloudPrintURL;
      this.dispatchEvent(cloudPrintEnableEvent);
    },

    /**
     * Updates the print preview with local printers.
     * Called from PrintPreviewHandler::SetupPrinterList().
     * @param {Array} printers Array of printer info objects.
     * @private
     */
    onSetPrinters_: function(printers) {
      var localDestsSetEvent = new cr.Event(
          NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
      localDestsSetEvent.destinationInfos = printers;
      this.dispatchEvent(localDestsSetEvent);
    },

    /**
     * Called when native layer gets settings information for a requested local
     * destination.
     * @param {Object} settingsInfo printer setting information.
     * @private
     */
    onUpdateWithPrinterCapabilities_: function(settingsInfo) {
      var capsSetEvent = new cr.Event(NativeLayer.EventType.CAPABILITIES_SET);
      capsSetEvent.settingsInfo = settingsInfo;
      this.dispatchEvent(capsSetEvent);
    },

    /**
     * Called when native layer gets settings information for a requested local
     * destination.
     * @param {string} printerId printer affected by error.
     * @private
     */
    onFailedToGetPrinterCapabilities_: function(destinationId) {
      var getCapsFailEvent = new cr.Event(
          NativeLayer.EventType.GET_CAPABILITIES_FAIL);
      getCapsFailEvent.destinationId = destinationId;
      this.dispatchEvent(getCapsFailEvent);
    },

    /** Reloads the printer list. */
    onReloadPrintersList_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.DESTINATIONS_RELOAD);
    },

    /**
     * Called from the C++ layer.
     * Take the PDF data handed to us and submit it to the cloud, closing the
     * print preview tab once the upload is successful.
     * @param {string} data Data to send as the print job.
     * @private
     */
    onPrintToCloud_: function(data) {
      var printToCloudEvent = new cr.Event(
          NativeLayer.EventType.PRINT_TO_CLOUD);
      printToCloudEvent.data = data;
      this.dispatchEvent(printToCloudEvent);
    },

    /**
     * Called from PrintPreviewUI::OnFileSelectionCancelled to notify the print
     * preview tab regarding the file selection cancel event.
     * @private
     */
    onFileSelectionCancelled_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.FILE_SELECTION_CANCEL);
    },

    /**
     * Called from PrintPreviewUI::OnFileSelectionCompleted to notify the print
     * preview tab regarding the file selection completed event.
     * @private
     */
    onFileSelectionCompleted_: function() {
      // If the file selection is completed and the tab is not already closed it
      // means that a pending print to pdf request exists.
      cr.dispatchSimpleEvent(
          this, NativeLayer.EventType.FILE_SELECTION_COMPLETE);
    },

    /**
     * Display an error message when print preview fails.
     * Called from PrintPreviewMessageHandler::OnPrintPreviewFailed().
     * @private
     */
    onPrintPreviewFailed_: function() {
      cr.dispatchSimpleEvent(
          this, NativeLayer.EventType.PREVIEW_GENERATION_FAIL);
    },

    /**
     * Display an error message when encountered invalid printer settings.
     * Called from PrintPreviewMessageHandler::OnInvalidPrinterSettings().
     * @private
     */
    onInvalidPrinterSettings_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.SETTINGS_INVALID);
    },

    /**
     * @param {{contentWidth: number, contentHeight: number, marginLeft: number,
     *          marginRight: number, marginTop: number, marginBottom: number,
     *          printableAreaX: number, printableAreaY: number,
     *          printableAreaWidth: number, printableAreaHeight: number}}
     *          pageLayout Specifies default page layout details in points.
     * @param {boolean} hasCustomPageSizeStyle Indicates whether the previewed
     *     document has a custom page size style.
     * @private
     */
    onDidGetDefaultPageLayout_: function(pageLayout, hasCustomPageSizeStyle) {
      var pageLayoutChangeEvent = new cr.Event(
          NativeLayer.EventType.PAGE_LAYOUT_READY);
      pageLayoutChangeEvent.pageLayout = pageLayout;
      pageLayoutChangeEvent.hasCustomPageSizeStyle = hasCustomPageSizeStyle;
      this.dispatchEvent(pageLayoutChangeEvent);
    },

    /**
     * Update the page count and check the page range.
     * Called from PrintPreviewUI::OnDidGetPreviewPageCount().
     * @param {number} pageCount The number of pages.
     * @param {number} previewResponseId The preview request id that resulted in
     *      this response.
     * @private
     */
    onDidGetPreviewPageCount_: function(pageCount, previewResponseId) {
      var pageCountChangeEvent = new cr.Event(
          NativeLayer.EventType.PAGE_COUNT_READY);
      pageCountChangeEvent.pageCount = pageCount;
      pageCountChangeEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(pageCountChangeEvent);
    },

    /**
     * Called when no pipelining previewed pages.
     * @param {number} previewUid Preview unique identifier.
     * @param {number} previewResponseId The preview request id that resulted in
     *     this response.
     * @private
     */
    onReloadPreviewPages_: function(previewUid, previewResponseId) {
      var previewReloadEvent = new cr.Event(
          NativeLayer.EventType.PREVIEW_RELOAD);
      previewReloadEvent.previewUid = previewUid;
      previewReloadEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(previewReloadEvent);
    },

    /**
     * Notification that a print preview page has been rendered.
     * Check if the settings have changed and request a regeneration if needed.
     * Called from PrintPreviewUI::OnDidPreviewPage().
     * @param {number} pageNumber The page number, 0-based.
     * @param {number} previewUid Preview unique identifier.
     * @param {number} previewResponseId The preview request id that resulted in
     *     this response.
     * @private
     */
    onDidPreviewPage_: function(pageNumber, previewUid, previewResponseId) {
      var pagePreviewGenEvent = new cr.Event(
          NativeLayer.EventType.PAGE_PREVIEW_READY);
      pagePreviewGenEvent.pageIndex = pageNumber;
      pagePreviewGenEvent.previewUid = previewUid;
      pagePreviewGenEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(pagePreviewGenEvent);
    },

    /**
     * Update the print preview when new preview data is available.
     * Create the PDF plugin as needed.
     * Called from PrintPreviewUI::PreviewDataIsAvailable().
     * @param {number} previewUid Preview unique identifier.
     * @param {number} previewResponseId The preview request id that resulted in
     *     this response.
     * @private
     */
    onUpdatePrintPreview_: function(previewUid, previewResponseId) {
      var previewGenDoneEvent = new cr.Event(
          NativeLayer.EventType.PREVIEW_GENERATION_DONE);
      previewGenDoneEvent.previewUid = previewUid;
      previewGenDoneEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(previewGenDoneEvent);
    },

    /**
     * Updates the fit to page option state based on the print scaling option of
     * source pdf. PDF's have an option to enable/disable print scaling. When we
     * find out that the print scaling option is disabled for the source pdf, we
     * uncheck the fitToPage_ to page checkbox. This function is called from C++
     * code.
     * @private
     */
    onPrintScalingDisabledForSourcePDF_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.DISABLE_SCALING);
    }
  };

  /**
   * Initial settings retrieved from the native layer.
   * @param {boolean} isInKioskAutoPrintMode Whether the print preview should be
   *     in auto-print mode.
   * @param {string} thousandsDelimeter Character delimeter of thousands digits.
   * @param {string} decimalDelimeter Character delimeter of the decimal point.
   * @param {!print_preview.MeasurementSystem.UnitType} unitType Unit type of
   *     local machine's measurement system.
   * @param {boolean} isDocumentModifiable Whether the document to print is
   *     modifiable.
   * @param {string} documentTitle Title of the document.
   * @param {?string} systemDefaultDestinationId ID of the system default
   *     destination.
   * @param {?string} serializedAppStateStr Serialized app state.
   * @constructor
   */
  function NativeInitialSettings(
      isInKioskAutoPrintMode,
      thousandsDelimeter,
      decimalDelimeter,
      unitType,
      isDocumentModifiable,
      documentTitle,
      systemDefaultDestinationId,
      serializedAppStateStr) {

    /**
     * Whether the print preview should be in auto-print mode.
     * @type {boolean}
     * @private
     */
    this.isInKioskAutoPrintMode_ = isInKioskAutoPrintMode;

    /**
     * Character delimeter of thousands digits.
     * @type {string}
     * @private
     */
    this.thousandsDelimeter_ = thousandsDelimeter;

    /**
     * Character delimeter of the decimal point.
     * @type {string}
     * @private
     */
    this.decimalDelimeter_ = decimalDelimeter;

    /**
     * Unit type of local machine's measurement system.
     * @type {string}
     * @private
     */
    this.unitType_ = unitType;

    /**
     * Whether the document to print is modifiable.
     * @type {boolean}
     * @private
     */
    this.isDocumentModifiable_ = isDocumentModifiable;

    /**
     * Title of the document.
     * @type {string}
     * @private
     */
    this.documentTitle_ = documentTitle;

    /**
     * ID of the system default destination.
     * @type {?string}
     * @private
     */
    this.systemDefaultDestinationId_ = systemDefaultDestinationId;

    /**
     * Serialized app state.
     * @type {?string}
     * @private
     */
    this.serializedAppStateStr_ = serializedAppStateStr;
  };

  NativeInitialSettings.prototype = {
    /**
     * @return {boolean} Whether the print preview should be in auto-print mode.
     */
    get isInKioskAutoPrintMode() {
      return this.isInKioskAutoPrintMode_;
    },

    /** @return {string} Character delimeter of thousands digits. */
    get thousandsDelimeter() {
      return this.thousandsDelimeter_;
    },

    /** @return {string} Character delimeter of the decimal point. */
    get decimalDelimeter() {
      return this.decimalDelimeter_;
    },

    /**
     * @return {!print_preview.MeasurementSystem.UnitType} Unit type of local
     *     machine's measurement system.
     */
    get unitType() {
      return this.unitType_;
    },

    /** @return {boolean} Whether the document to print is modifiable. */
    get isDocumentModifiable() {
      return this.isDocumentModifiable_;
    },

    /** @return {string} Document title. */
    get documentTitle() {
      return this.documentTitle_;
    },

    /** @return {?string} ID of the system default destination. */
    get systemDefaultDestinationId() {
      return this.systemDefaultDestinationId_;
    },

    /** @return {?string} Serialized app state. */
    get serializedAppStateStr() {
      return this.serializedAppStateStr_;
    }
  };

  // Export
  return {
    NativeInitialSettings: NativeInitialSettings,
    NativeLayer: NativeLayer
  };
});
