// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Interface to the Chromium print preview generator.
   * @param {!print_preview.DestinationStore} destinationStore Used to get the
   *     currently selected destination.
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to read the
   *     state of the ticket and write document information.
   * @param {!print_preview.NativeLayer} nativeLayer Used to communicate to
   *     Chromium's preview rendering system.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function PreviewGenerator(destinationStore, printTicketStore, nativeLayer) {
    cr.EventTarget.call(this);

    /**
     * Used to get the currently selected destination.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Used to read the state of the ticket and write document information.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Interface to the Chromium native layer.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * ID of current in-flight request. Requests that do not share this ID will
     * be ignored.
     * @type {number}
     * @private
     */
    this.inFlightRequestId_ = -1;

    /**
     * Whether the previews are being generated in landscape mode.
     * @type {boolean}
     * @private
     */
    this.isLandscapeEnabled_ = false;

    /**
     * Whether the previews are being generated with a header and footer.
     * @type {boolean}
     * @private
     */
    this.isHeaderFooterEnabled_ = false;

    /**
     * Whether the previews are being generated in color.
     * @type {boolean}
     * @private
     */
    this.isColorEnabled_ = false;

    /**
     * Whether the document should be fitted to the page.
     * @type {boolean}
     * @private
     */
    this.isFitToPageEnabled_ = false;

    /**
     * Page number set used to generate the last preview.
     * @type {print_preview.PageNumberSet}
     * @private
     */
    this.pageNumberSet_ = null;

    /**
     * Margins type used to generate the last preview.
     * @type {!print_preview.ticket_items.MarginsType.Value}
     * @private
     */
    this.marginsType_ = print_preview.ticket_items.MarginsType.Value.DEFAULT;

    /**
     * Whether the document should have element CSS backgrounds printed.
     * @type {boolean}
     * @private
     */
    this.isCssBackgroundEnabled_ = false;

    /**
     * Destination that was selected for the last preview.
     * @type {print_preview.Destination}
     * @private
     */
    this.selectedDestination_ = null;

    /**
     * Event tracker used to keep track of native layer events.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    this.addEventListeners_();
  };

  /**
   * Event types dispatched by the preview generator.
   * @enum {string}
   */
  PreviewGenerator.EventType = {
    // Dispatched when the document can be printed.
    DOCUMENT_READY: 'print_preview.PreviewGenerator.DOCUMENT_READY',

    // Dispatched when a page preview is ready. The previewIndex field of the
    // event is the index of the page in the modified document, not the
    // original. So page 4 of the original document might be previewIndex = 0 of
    // the modified document.
    PAGE_READY: 'print_preview.PreviewGenerator.PAGE_READY',

    // Dispatched when the document preview starts to be generated.
    PREVIEW_START: 'print_preview.PreviewGenerator.PREVIEW_START',

    // Dispatched when the current print preview request fails.
    FAIL: 'print_preview.PreviewGenerator.FAIL'
  };

  PreviewGenerator.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Request that new preview be generated. A preview request will not be
     * generated if the print ticket has not changed sufficiently.
     * @return {boolean} Whether a new preview was actually requested.
     */
    requestPreview: function() {
      if (!this.printTicketStore_.isTicketValidForPreview()) {
        return false;
      }
      if (!this.hasPreviewChanged_()) {
        // Changes to these ticket items might not trigger a new preview, but
        // they still need to be recorded.
        this.marginsType_ = this.printTicketStore_.getMarginsType();
        return false;
      }
      this.isLandscapeEnabled_ = this.printTicketStore_.isLandscapeEnabled();
      this.isHeaderFooterEnabled_ =
          this.printTicketStore_.isHeaderFooterEnabled();
      this.isColorEnabled_ = this.printTicketStore_.isColorEnabled();
      this.isFitToPageEnabled_ = this.printTicketStore_.isFitToPageEnabled();
      this.pageNumberSet_ = this.printTicketStore_.getPageNumberSet();
      this.marginsType_ = this.printTicketStore_.getMarginsType();
      this.isCssBackgroundEnabled_ =
          this.printTicketStore_.isCssBackgroundEnabled();
      this.selectedDestination_ = this.destinationStore_.selectedDestination;

      this.inFlightRequestId_++;
      this.nativeLayer_.startGetPreview(
          this.destinationStore_.selectedDestination,
          this.printTicketStore_,
          this.inFlightRequestId_);
      return true;
    },

    /** Removes all event listeners that the preview generator has attached. */
    removeEventListeners: function() {
      this.tracker_.removeAll();
    },

    /**
     * Adds event listeners to the relevant native layer events.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PAGE_LAYOUT_READY,
          this.onPageLayoutReady_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PAGE_COUNT_READY,
          this.onPageCountReady_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PREVIEW_RELOAD,
          this.onPreviewReload_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PAGE_PREVIEW_READY,
          this.onPagePreviewReady_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PREVIEW_GENERATION_DONE,
          this.onPreviewGenerationDone_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PREVIEW_GENERATION_FAIL,
          this.onPreviewGenerationFail_.bind(this));
    },

    /**
     * Dispatches a PAGE_READY event to signal that a page preview is ready.
     * @param {number} previewIndex Index of the page with respect to the pages
     *     shown in the preview. E.g an index of 0 is the first displayed page,
     *     but not necessarily the first original document page.
     * @param {number} pageNumber Number of the page with respect to the
     *     document. A value of 3 means it's the third page of the original
     *     document.
     * @param {number} previewUid Unique identifier of the preview.
     * @private
     */
    dispatchPageReadyEvent_: function(previewIndex, pageNumber, previewUid) {
      var pageGenEvent = new cr.Event(PreviewGenerator.EventType.PAGE_READY);
      pageGenEvent.previewIndex = previewIndex;
      pageGenEvent.previewUrl = 'chrome://print/' + previewUid.toString() +
          '/' + (pageNumber - 1) + '/print.pdf';
      this.dispatchEvent(pageGenEvent);
    },

    /**
     * Dispatches a PREVIEW_START event. Signals that the preview should be
     * reloaded.
     * @param {number} previewUid Unique identifier of the preview.
     * @param {number} index Index of the first page of the preview.
     * @private
     */
    dispatchPreviewStartEvent_: function(previewUid, index) {
      var previewStartEvent = new cr.Event(
          PreviewGenerator.EventType.PREVIEW_START);
      if (!this.printTicketStore_.isDocumentModifiable) {
        index = -1;
      }
      previewStartEvent.previewUrl = 'chrome://print/' +
          previewUid.toString() + '/' + index + '/print.pdf';
      this.dispatchEvent(previewStartEvent);
    },

    /**
     * @return {boolean} Whether the print ticket has changed sufficiently to
     *     determine whether a new preview request should be issued.
     * @private
     */
    hasPreviewChanged_: function() {
      var ticketStore = this.printTicketStore_;
      return this.inFlightRequestId_ == -1 ||
          ticketStore.isLandscapeEnabled() != this.isLandscapeEnabled_ ||
          ticketStore.isHeaderFooterEnabled() != this.isHeaderFooterEnabled_ ||
          ticketStore.isColorEnabled() != this.isColorEnabled_ ||
          ticketStore.isFitToPageEnabled() != this.isFitToPageEnabled_ ||
          !ticketStore.getPageNumberSet().equals(this.pageNumberSet_) ||
          (ticketStore.getMarginsType() != this.marginsType_ &&
              ticketStore.getMarginsType() !=
                  print_preview.ticket_items.MarginsType.Value.CUSTOM) ||
          (ticketStore.getMarginsType() ==
              print_preview.ticket_items.MarginsType.Value.CUSTOM &&
              !ticketStore.getCustomMargins().equals(
                  ticketStore.getDocumentMargins())) ||
          (ticketStore.isCssBackgroundEnabled() !=
              this.isCssBackgroundEnabled_) ||
          (this.selectedDestination_ !=
              this.destinationStore_.selectedDestination);
    },

    /**
     * Called when the page layout of the document is ready. Always occurs
     * as a result of a preview request.
     * @param {cr.Event} event Contains layout info about the document.
     * @private
     */
    onPageLayoutReady_: function(event) {
      // NOTE: A request ID is not specified, so assuming its for the current
      // in-flight request.

      var origin = new print_preview.Coordinate2d(
          event.pageLayout.printableAreaX,
          event.pageLayout.printableAreaY);
      var size = new print_preview.Size(
          event.pageLayout.printableAreaWidth,
          event.pageLayout.printableAreaHeight);

      var margins = new print_preview.Margins(
          Math.round(event.pageLayout.marginTop),
          Math.round(event.pageLayout.marginRight),
          Math.round(event.pageLayout.marginBottom),
          Math.round(event.pageLayout.marginLeft));

      var o = print_preview.ticket_items.CustomMargins.Orientation;
      var pageSize = new print_preview.Size(
          event.pageLayout.contentWidth +
              margins.get(o.LEFT) + margins.get(o.RIGHT),
          event.pageLayout.contentHeight +
              margins.get(o.TOP) + margins.get(o.BOTTOM));

      this.printTicketStore_.updateDocumentPageInfo(
          new print_preview.PrintableArea(origin, size),
          pageSize,
          event.hasCustomPageSizeStyle,
          margins);
    },

    /**
     * Called when the document page count is received from the native layer.
     * Always occurs as a result of a preview request.
     * @param {cr.Event} event Contains the document's page count.
     * @private
     */
    onPageCountReady_: function(event) {
      if (this.inFlightRequestId_ != event.previewResponseId) {
        return; // Ignore old response.
      }
      this.printTicketStore_.updatePageCount(event.pageCount);
      this.pageNumberSet_ = this.printTicketStore_.getPageNumberSet();
    },

    /**
     * Called when the print preview should be reloaded.
     * @param {cr.Event} event Contains the preview UID and request ID.
     * @private
     */
    onPreviewReload_: function(event) {
      if (this.inFlightRequestId_ != event.previewResponseId) {
        return; // Ignore old response.
      }
      var pageNumberSet = this.printTicketStore_.getPageNumberSet();
      this.dispatchPreviewStartEvent_(
          event.previewUid, pageNumberSet.getPageNumberAt(0) - 1);
      for (var i = 0; i < pageNumberSet.size; i++) {
        var pageNumber = pageNumberSet.getPageNumberAt(i);
        this.dispatchPageReadyEvent_(i, pageNumber, event.previewUid);
      }
      cr.dispatchSimpleEvent(this, PreviewGenerator.EventType.DOCUMENT_READY);
    },

    /**
     * Called when a page's preview has been generated. Dispatches a
     * PAGE_READY event.
     * @param {cr.Event} event Contains the page index and preview UID.
     * @private
     */
    onPagePreviewReady_: function(event) {
      if (this.inFlightRequestId_ != event.previewResponseId) {
        return; // Ignore old response.
      }
      var pageNumber = event.pageIndex + 1;
      if (this.printTicketStore_.getPageNumberSet().hasPageNumber(pageNumber)) {
        var previewIndex = this.printTicketStore_.getPageNumberSet()
            .getPageNumberIndex(pageNumber);
        if (previewIndex == 0) {
          this.dispatchPreviewStartEvent_(event.previewUid, event.pageIndex);
        }
        this.dispatchPageReadyEvent_(
            previewIndex, pageNumber, event.previewUid);
      }
    },

    /**
     * Called when the preview generation is complete. Dispatches a
     * DOCUMENT_READY event.
     * @param {cr.Event} event Contains the preview UID and response ID.
     * @private
     */
    onPreviewGenerationDone_: function(event) {
      if (this.inFlightRequestId_ != event.previewResponseId) {
        return; // Ignore old response.
      }
      // Dispatch a PREVIEW_START event since non-modifiable documents don't
      // trigger PAGE_READY events.
      if (!this.printTicketStore_.isDocumentModifiable) {
        this.dispatchPreviewStartEvent_(event.previewUid, 0);
      }
      cr.dispatchSimpleEvent(this, PreviewGenerator.EventType.DOCUMENT_READY);
    },

    /**
     * Called when the preview generation fails.
     * @private
     */
    onPreviewGenerationFail_: function() {
      // NOTE: No request ID is returned from Chromium so its assumed its the
      // current one.
      cr.dispatchSimpleEvent(this, PreviewGenerator.EventType.FAIL);
    }
  };

  // Export
  return {
    PreviewGenerator: PreviewGenerator
  };
});
