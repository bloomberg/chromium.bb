// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');
/**
 * @typedef {{accessibility: Function,
 *            documentLoadComplete: Function,
 *            getHeight: Function,
 *            getHorizontalScrollbarThickness: Function,
 *            getPageLocationNormalized: Function,
 *            getVerticalScrollbarThickness: Function,
 *            getWidth: Function,
 *            getZoomLevel: Function,
 *            goToPage: Function,
 *            grayscale: Function,
 *            loadPreviewPage: Function,
 *            onload: Function,
 *            onPluginSizeChanged: Function,
 *            onScroll: Function,
 *            pageXOffset: Function,
 *            pageYOffset: Function,
 *            reload: Function,
 *            resetPrintPreviewMode: Function,
 *            sendKeyEvent: Function,
 *            setPageNumbers: Function,
 *            setPageXOffset: Function,
 *            setPageYOffset: Function,
 *            setZoomLevel: Function,
 *            fitToHeight: Function,
 *            fitToWidth: Function,
 *            zoomIn: Function,
 *            zoomOut: Function}}
 */
print_preview_new.PDFPlugin;

/**
 * Constant values matching printing::DuplexMode enum.
 * @enum {number}
 */
print_preview_new.DuplexMode = {
  SIMPLEX: 0,
  LONG_EDGE: 1,
  UNKNOWN_DUPLEX_MODE: -1
};

Polymer({
  is: 'print-preview-preview-area',

  behaviors: [WebUIListenerBehavior, SettingsBehavior],
  properties: {
    /** @type {print_preview.DocumentInfo} */
    documentInfo: Object,

    /** @type {print_preview.Destination} */
    destination: Object,

    /** @type {print_preview_new.State} */
    state: {
      type: Object,
      notify: true,
    },
  },

  observers: [
    'onSettingsChanged_(settings.color.value, settings.cssBackground.value, ' +
        'settings.fitToPage.value, settings.headerFooter.value, ' +
        'settings.layout.value, settings.margins.value, ' +
        'settings.mediaSize.value, settings.ranges.value,' +
        'settings.selectionOnly.value, settings.scaling.value, ' +
        'destination.id, destination.capabilities, state.initialized)',
    'onPreviewStateChanged_(state.previewLoading, state.invalidSettings, ' +
        'state.previewFailed)',
  ],

  /** @private {print_preview.NativeLayer} */
  nativeLayer_: null,

  /** @private {number} */
  inFlightRequestId_: -1,

  /** @private {HTMLEmbedElement|print_preview_new.PDFPlugin} */
  plugin_: null,

  /** @private {boolean} */
  pluginLoaded_: false,

  /** @private {boolean} */
  documentReady_: false,

  /** @override */
  attached: function() {
    this.nativeLayer_ = print_preview.NativeLayer.getInstance();
    this.addWebUIListener(
        'page-count-ready', this.onPageCountReady_.bind(this));
    this.addWebUIListener(
        'page-layout-ready', this.onPageLayoutReady_.bind(this));
    this.addWebUIListener(
        'page-preview-ready', this.onPagePreviewReady_.bind(this));

    const oopCompatObj =
        this.$$('.preview-area-compatibility-object-out-of-process');
    const isOOPCompatible = oopCompatObj.postMessage;
    oopCompatObj.parentElement.removeChild(oopCompatObj);
    if (!isOOPCompatible)
      this.set('state.previewFailed', true);
    else
      this.set('state.previewLoading', true);
  },

  /** @private */
  onSettingsChanged_: function() {
    if (!this.state.initialized || !this.getSetting('scaling').valid ||
        !this.getSetting('pages').valid || !this.getSetting('copies').valid ||
        !this.destination || !this.destination.capabilities) {
      return;
    }
    this.documentReady_ = false;
    this.set('state.previewLoading', true);
    this.getPreview_().then(
        previewUid => {
          if (!this.documentInfo.isModifiable)
            this.onPreviewStart_(previewUid, -1);
          this.documentReady_ = true;
          if (this.pluginLoaded_)
            this.set('state.previewLoading', false);
        },
        type => {
          if (/** @type{string} */ (type) == 'SETTINGS_INVALID')
            this.set('state.invalidSettings', true);
          else if (/** @type{string} */ (type) != 'CANCELLED')
            this.set('state.previewFailed', true);
          this.set('state.previewLoading', false);
        });
  },

  /**
   * Set the visibility of the message overlay.
   * @param {boolean} visible Whether to make the overlay visible or not
   * @private
   */
  setOverlayVisible_: function(visible) {
    const overlayEl = this.$$('.preview-area-overlay-layer');
    overlayEl.classList.toggle('invisible', !visible);
    overlayEl.setAttribute('aria-hidden', !visible);
  },

  /** @private */
  onPreviewStateChanged_: function() {
    // update the appearance here.
    const visible = this.state.previewLoading || this.state.previewFailed ||
        this.state.invalidSettings;
    this.setOverlayVisible_(visible);

    // Disable jumping animation to conserve cycles.
    const jumpingDotsEl = this.$$('.preview-area-loading-message-jumping-dots');
    jumpingDotsEl.classList.toggle('jumping-dots', this.state.previewLoading);
  },

  /**
   * @param {number} previewUid The unique identifier of the preview.
   * @param {number} index The index of the page to preview.
   * @private
   */
  onPreviewStart_: function(previewUid, index) {
    if (!this.plugin_)
      this.createPlugin_(previewUid, index);
    this.pluginLoaded_ = false;
    this.plugin_.resetPrintPreviewMode(
        this.getPreviewUrl_(previewUid, index), !this.getSettingValue('color'),
        this.getSetting('pages').value, this.documentInfo.isModifiable);
  },

  /**
   * Called when the page layout of the document is ready. Always occurs
   * as a result of a preview request.
   * @param {{marginTop: number,
   *          marginLeft: number,
   *          marginBottom: number,
   *          marginRight: number,
   *          contentWidth: number,
   *          contentHeight: number,
   *          printableAreaX: number,
   *          printableAreaY: number,
   *          printableAreaWidth: number,
   *          printableAreaHeight: number,
   *        }} pageLayout Layout information about the document.
   * @param {boolean} hasCustomPageSizeStyle Whether this document has a
   *     custom page size or style to use.
   * @private
   */
  onPageLayoutReady_: function(pageLayout, hasCustomPageSizeStyle) {
    const origin = new print_preview.Coordinate2d(
        pageLayout.printableAreaX, pageLayout.printableAreaY);
    const size = new print_preview.Size(
        pageLayout.printableAreaWidth, pageLayout.printableAreaHeight);

    const margins = new print_preview.Margins(
        Math.round(pageLayout.marginTop), Math.round(pageLayout.marginRight),
        Math.round(pageLayout.marginBottom), Math.round(pageLayout.marginLeft));

    const o = print_preview.ticket_items.CustomMarginsOrientation;
    const pageSize = new print_preview.Size(
        pageLayout.contentWidth + margins.get(o.LEFT) + margins.get(o.RIGHT),
        pageLayout.contentHeight + margins.get(o.TOP) + margins.get(o.BOTTOM));

    this.documentInfo.updatePageInfo(
        new print_preview.PrintableArea(origin, size), pageSize,
        hasCustomPageSizeStyle, margins);
    this.notifyPath('documentInfo.printableArea');
    this.notifyPath('documentInfo.pageSize');
    this.notifyPath('documentInfo.margins');
    this.notifyPath('documentInfo.hasCssMediaStyles');
  },

  /**
   * Called when the document page count is received from the native layer.
   * Always occurs as a result of a preview request.
   * @param {number} pageCount The document's page count.
   * @param {number} previewResponseId The request ID that corresponds to this
   *     page count.
   * @param {number} fitToPageScaling The scaling required to fit the document
   *     to page (unused).
   * @private
   */
  onPageCountReady_: function(pageCount, previewResponseId, fitToPageScaling) {
    if (this.inFlightRequestId_ != previewResponseId)
      return;
    this.documentInfo.updatePageCount(pageCount);
    this.documentInfo.fitToPageScaling_ = fitToPageScaling;
    this.notifyPath('documentInfo.pageCount');
    this.notifyPath('documentInfo.fitToPageScaling');
  },

  /**
   * Called when the plugin loads. This is a consequence of calling
   * plugin.reload(). Certain plugin state can only be set after the plugin
   * has loaded.
   * @private
   */
  onPluginLoad_: function() {
    this.pluginLoaded_ = true;
    if (this.documentReady_)
      this.set('state.previewLoading', false);
  },

  /**
   * Get the URL for the plugin.
   * @param {number} previewUid Unique identifier of preview.
   * @param {number} index Page index for plugin.
   * @return {string} The URL
   * @private
   */
  getPreviewUrl_: function(previewUid, index) {
    return `chrome://print/${previewUid}/${index}/print.pdf`;
  },

  /**
   * Called when a page's preview has been generated.
   * @param {number} pageIndex The index of the page whose preview is ready.
   * @param {number} previewUid The unique ID of the print preview UI.
   * @param {number} previewResponseId The preview request ID that this page
   *     preview is a response to.
   * @private
   */
  onPagePreviewReady_: function(pageIndex, previewUid, previewResponseId) {
    if (this.inFlightRequestId_ != previewResponseId)
      return;
    const pageNumber = pageIndex + 1;
    const index = this.getSettingValue('pages').indexOf(pageNumber);
    if (index == 0)
      this.onPreviewStart_(previewUid, pageIndex);
    if (index != -1) {
      this.plugin_.loadPreviewPage(
          this.getPreviewUrl_(previewUid, pageIndex), index);
    }
  },

  /**
   * Creates a preview plugin and adds it to the DOM.
   * @param {number} previewUid The unique ID of the preview. Used to determine
   *     the URL for the plugin.
   * @param {number} index The index of the page to load. Used to determine the
   *     URL for the plugin.
   * @private
   */
  createPlugin_: function(previewUid, index) {
    assert(!this.plugin_);
    const srcUrl = this.getPreviewUrl_(previewUid, index);
    this.plugin_ = /** @type {print_preview_new.PDFPlugin} */ (
        PDFCreateOutOfProcessPlugin(srcUrl));
    this.plugin_.classList.add('preview-area-plugin');
    this.plugin_.setAttribute('aria-live', 'polite');
    this.plugin_.setAttribute('aria-atomic', 'true');
    // NOTE: The plugin's 'id' field must be set to 'pdf-viewer' since
    // chrome/renderer/printing/print_render_frame_helper.cc actually
    // references it.
    this.plugin_.setAttribute('id', 'pdf-viewer');
    this.$$('.preview-area-plugin-wrapper')
        .appendChild(/** @type {Node} */ (this.plugin_));

    this.plugin_.setLoadCallback(this.onPluginLoad_.bind(this));
  },

  /**
   * Requests a preview from the native layer.
   * @return {!Promise} Promise that resolves when the preview has been
   *     generated.
   */
  getPreview_: function() {
    this.inFlightRequestId_++;
    const dpi = /** @type {{horizontal_dpi: (number | undefined),
                            vertical_dpi: (number | undefined),
                            vendor_id: (number | undefined)}} */ (
        this.getSettingValue('dpi'));
    const ticket = {
      pageRange: this.getSettingValue('ranges'),
      mediaSize: this.getSettingValue('mediaSize'),
      landscape: this.getSettingValue('layout'),
      color: this.destination.getNativeColorModel(
          /** @type {boolean} */ (this.getSettingValue('color'))),
      headerFooterEnabled: this.getSettingValue('headerFooter'),
      marginsType: this.getSettingValue('margins'),
      isFirstRequest: this.inFlightRequestId_ == 0,
      requestID: this.inFlightRequestId_,
      previewModifiable: this.documentInfo.isModifiable,
      generateDraftData: this.documentInfo.isModifiable,
      fitToPageEnabled: this.getSettingValue('fitToPage'),
      scaleFactor: parseInt(this.getSettingValue('scaling'), 10),
      shouldPrintBackgrounds: this.getSettingValue('cssBackground'),
      shouldPrintSelectionOnly: this.getSettingValue('selectionOnly'),
      // NOTE: Even though the remaining fields don't directly relate to the
      // preview, they still need to be included.
      // e.g. printing::PrintSettingsFromJobSettings() still checks for them.
      collate: true,
      copies: 1,
      deviceName: this.destination.id,
      dpiHorizontal: (dpi && 'horizontal_dpi' in dpi) ? dpi.horizontal_dpi : 0,
      dpiVertical: (dpi && 'vertical_dpi' in dpi) ? dpi.vertical_dpi : 0,
      duplex: this.getSettingValue('duplex') ?
          print_preview_new.DuplexMode.LONG_EDGE :
          print_preview_new.DuplexMode.SIMPLEX,
      printToPDF: this.destination.id ==
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
      printWithCloudPrint: !this.destination.isLocal,
      printWithPrivet: this.destination.isPrivet,
      printWithExtension: this.destination.isExtension,
      rasterizePDF: false,
    };

    // Set 'cloudPrintID' only if the this.destination is not local.
    if (this.destination && !this.destination.isLocal) {
      ticket.cloudPrintID = this.destination.id;
    }

    if (this.getSettingValue('margins') ==
        print_preview.ticket_items.MarginsTypeValue.CUSTOM) {
      // TODO (rbpotter): Replace this with real values when custom margins are
      // implemented.
      ticket.marginsCustom = {
        marginTop: 70,
        marginRight: 70,
        marginBottom: 70,
        marginLeft: 70,
      };
    }
    let pageCount = -1;
    if (this.inFlightRequestId_ > 0) {
      pageCount = this.documentInfo.isModifiable ?
          this.documentInfo.pageCount : 0;
    }
    return this.nativeLayer_.getPreview(JSON.stringify(ticket), pageCount);
  },
});
