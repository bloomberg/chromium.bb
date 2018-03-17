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

(function() {
'use strict';

/** @enum {string} */
const PreviewAreaState_ = {
  LOADING: 'loading',
  DISPLAY_PREVIEW: 'display-preview',
  OPEN_IN_PREVIEW: 'open-in-preview',
  INVALID_SETTINGS: 'invalid-settings',
  PREVIEW_FAILED: 'preview-failed',
};

Polymer({
  is: 'print-preview-preview-area',

  behaviors: [WebUIListenerBehavior, SettingsBehavior, I18nBehavior],

  properties: {
    /** @type {print_preview.DocumentInfo} */
    documentInfo: Object,

    /** @type {print_preview.Destination} */
    destination: Object,

    /** @type {!print_preview_new.State} */
    state: {
      type: Number,
      observer: 'onStateChanged_',
    },

    /** @type {?print_preview.MeasurementSystem} */
    measurementSystem: Object,

    /** @private {string} */
    previewState_: {
      type: String,
      notify: true,
      value: PreviewAreaState_.LOADING,
    },

    /** @private {boolean} */
    previewLoaded_: {
      type: Boolean,
      notify: true,
      computed: 'computePreviewLoaded_(previewState_)',
    },
  },

  listeners: {
    'pointerover': 'onPointerOver_',
    'pointerout': 'onPointerOut_',
  },

  observers: [
    'onSettingsChanged_(settings.color.value, settings.cssBackground.value, ' +
        'settings.fitToPage.value, settings.headerFooter.value, ' +
        'settings.layout.value, settings.margins.value, ' +
        'settings.mediaSize.value, settings.ranges.value,' +
        'settings.selectionOnly.value, settings.scaling.value, ' +
        'settings.rasterize.value, destination.id, destination.capabilities)',
  ],

  /** @private {print_preview.NativeLayer} */
  nativeLayer_: null,

  /** @private {number} */
  inFlightRequestId_: -1,

  /** @private {boolean} */
  requestPreviewWhenReady_: false,

  /** @private {HTMLEmbedElement|print_preview_new.PDFPlugin} */
  plugin_: null,

  /** @private {boolean} Whether the plugin is loaded */
  pluginLoaded_: false,

  /** @private {boolean} Whether the document is ready */
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
    if (!isOOPCompatible) {
      this.previewState_ = PreviewAreaState_.PREVIEW_FAILED;
      this.fire('preview-failed');
    }
  },

  /**
   * @return {boolean} Whether the preview is loaded.
   * @private
   */
  computePreviewLoaded_: function() {
    return this.previewState_ == PreviewAreaState_.DISPLAY_PREVIEW;
  },

  /** @return {boolean} Whether the preview is loaded. */
  previewLoaded: function() {
    return this.previewLoaded_;
  },

  /**
   * Called when the pointer moves onto the component. Shows the margin
   * controls if custom margins are being used.
   * @param {!Event} event Contains element pointer moved from.
   * @private
   */
  onPointerOver_: function(event) {
    const marginControlContainer = this.$.marginControlContainer;
    let fromElement = event.fromElement;
    while (fromElement != null) {
      if (fromElement == marginControlContainer)
        return;

      fromElement = fromElement.parentElement;
    }
    marginControlContainer.visible = true;
  },

  /**
   * Called when the pointer moves off of the component. Hides the margin
   * controls if they are visible.
   * @param {!Event} event Contains element pointer moved to.
   * @private
   */
  onPointerOut_: function(event) {
    const marginControlContainer = this.$.marginControlContainer;
    let toElement = event.toElement;
    while (toElement != null) {
      if (toElement == marginControlContainer)
        return;

      toElement = toElement.parentElement;
    }

    if (marginControlContainer.isDragging())
      return;

    marginControlContainer.visible = false;
  },

  /** @private */
  onSettingsChanged_: function() {
    if (this.state == print_preview_new.State.READY) {
      this.startPreview_();
      return;
    }
    this.requestPreviewWhenReady_ = true;
  },

  /**
   * @return {string} 'invisible' if overlay is invisible, '' otherwise.
   * @private
   */
  getInvisible_: function() {
    return this.previewLoaded() ? 'invisible' : '';
  },

  /**
   * @return {boolean} Whether the preview is currently loading.
   * @private
   */
  isPreviewLoading_: function() {
    return this.previewState_ == PreviewAreaState_.LOADING;
  },

  /**
   * @return {string} 'jumping-dots' to enable animation, '' otherwise.
   * @private
   */
  getJumpingDots_: function() {
    return this.isPreviewLoading_() ? 'jumping-dots' : '';
  },

  /**
   * @return {boolean} Whether the system dialog button should be shown.
   * @private
   */
  displaySystemDialogButton_: function() {
    return this.previewState_ == PreviewAreaState_.INVALID_SETTINGS ||
        this.previewState_ == PreviewAreaState_.OPEN_IN_PREVIEW;
  },

  /**
   * @return {string} The current preview area message to display.
   * @private
   */
  currentMessage_: function() {
    if (this.previewState_ == PreviewAreaState_.LOADING)
      return this.i18n('loading');
    if (this.previewState_ == PreviewAreaState_.OPEN_IN_PREVIEW)
      return this.i18n('openPdfInPreview');
    if (this.previewState_ == PreviewAreaState_.INVALID_SETTINGS)
      return this.i18n('invalidSettings');
    if (this.previewState_ == PreviewAreaState_.PREVIEW_FAILED)
      return this.i18n('previewFailed');
    return '';
  },

  /** @private */
  startPreview_: function() {
    this.previewState_ = PreviewAreaState_.LOADING;
    this.documentReady_ = false;
    this.getPreview_().then(
        previewUid => {
          if (!this.documentInfo.isModifiable)
            this.onPreviewStart_(previewUid, -1);
          this.documentReady_ = true;
          if (this.pluginLoaded_) {
            this.previewState_ = PreviewAreaState_.DISPLAY_PREVIEW;
            this.fire('preview-loaded');
          }
        },
        type => {
          if (/** @type{string} */ (type) == 'SETTINGS_INVALID')
            this.previewState_ = PreviewAreaState_.INVALID_SETTINGS;
          else if (/** @type{string} */ (type) != 'CANCELLED') {
            this.previewState_ = PreviewAreaState_.PREVIEW_FAILED;
            this.fire('preview-failed');
          }
        });
  },

  /** @private */
  onStateChanged_: function() {
    switch (this.state) {
      case (print_preview_new.State.NOT_READY):
        // Resetting the destination clears the invalid settings error.
        this.previewState_ = PreviewAreaState_.LOADING;
        break;
      case (print_preview_new.State.READY):
        // Request a new preview.
        if (this.requestPreviewWhenReady_) {
          this.startPreview_();
          this.requestPreviewWhenReady_ = false;
        }
        break;
      case (print_preview_new.State.INVALID_PRINTER):
        this.previewState_ = PreviewAreaState_.INVALID_SETTINGS;
        break;
      default:
        break;
    }
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
    if (this.documentReady_) {
      this.previewState_ = PreviewAreaState_.DISPLAY_PREVIEW;
      this.fire('preview-loaded');
    }
  },

  /**
   * Called when the preview plugin's visual state has changed. This is a
   * consequence of scrolling or zooming the plugin. Updates the custom
   * margins component if shown.
   * @param {number} pageX The horizontal offset for the page corner in pixels.
   * @param {number} pageY The vertical offset for the page corner in pixels.
   * @param {number} pageWidth The page width in pixels.
   * @param {number} viewportWidth The viewport width in pixels.
   * @param {number} viewportHeight The viewport height in pixels.
   * @private
   */
  onPreviewVisualStateChange_: function(
      pageX, pageY, pageWidth, viewportWidth, viewportHeight) {
    this.$.marginControlContainer.updateTranslationTransform(
        new print_preview.Coordinate2d(pageX, pageY));
    this.$.marginControlContainer.updateScaleTransform(
        pageWidth / this.documentInfo.pageSize.width);
    this.$.marginControlContainer.updateClippingMask(
        new print_preview.Size(viewportWidth, viewportHeight));
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
    this.plugin_.setViewportChangedCallback(
        this.onPreviewVisualStateChange_.bind(this));
  },

  /**
   * Called when dragging margins starts or stops.
   */
  onMarginDragChanged_: function(e) {
    if (!this.plugin_)
      return;

    // When hovering over the plugin (which may be in a separate iframe)
    // pointer events will be sent to the frame. When dragging the margins,
    // we don't want this to happen as it can cause the margin to stop
    // being draggable.
    this.plugin_.style.pointerEvents = e.detail ? 'none' : 'auto';
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
      rasterizePDF: this.getSettingValue('rasterize'),
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
})();
