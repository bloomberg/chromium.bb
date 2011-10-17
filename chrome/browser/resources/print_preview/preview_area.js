// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'strict';

  /**
   * Creates a PreviewArea object. It represents the area where the preview
   * document is displayed.
   * @constructor
   */
  function PreviewArea() {
    // The embedded pdf plugin object.
    this.pdfPlugin_ = null;

    // True if the pdf document is loaded in the preview area.
    this.pdfLoaded_ = false;

    // Contains the zoom level just before a new preview is requested so the
    // same zoom level can be restored.
    this.zoomLevel_ = null;
    // @type {{x: number, y: number}} Contains the page offset values just
    //     before a new preview is requested so that the scroll amount can be
    //     restored later.
    this.pageOffset_ = null;
    this.addEventListeners_();
  }

  cr.addSingletonGetter(PreviewArea);

  PreviewArea.prototype = {
    /**
     * The width of the plugin area in pixels, excluding any visible scrollbars,
     * @type {number}
     */
    get width() {
      return this.widthPercent * this.pdfPlugin_.offsetWidth;
    },

    /**
     * The height of the plugin area in pixels, excluding any visible
     *     scrollbars.
     * @type {number}
     */
    get height() {
      return this.heightPercent * this.pdfPlugin_.offsetHeight;
    },

    /**
     * The width of the plugin area in percent, excluding any visible
     *     scrollbars.
     * @type {number}
     */
    get widthPercent() {
      var width = this.pdfPlugin_.getWidth();
      var scrollbarWidth = this.pdfPlugin_.getVerticalScrollbarThickness();
      return (width - scrollbarWidth) / width;
    },

    /**
     * The height of the plugin area in percent, excluding any visible
     *     scrollbars.
     * @type {number}
     */
    get heightPercent() {
      var height = this.pdfPlugin_.getHeight();
      var scrollbarHeight = this.pdfPlugin_.getHorizontalScrollbarThickness();
      return (height - scrollbarHeight) / height;
    },

    get pdfLoaded() {
      return this.pdfLoaded_;
    },

    set pdfLoaded(pdfLoaded) {
      this.pdfLoaded_ = pdfLoaded;
    },

    /**
     * @return {print_preview.Rect} A rectangle describing the postion of the
     *   most visible page normalized with respect to the total height and width
     *   of the plugin.
     */
    getPageLocationNormalized: function() {
      var pluginLocation =
          this.pdfPlugin_.getPageLocationNormalized().split(';');
      return new print_preview.Rect(parseFloat(pluginLocation[0]),
                                    parseFloat(pluginLocation[1]),
                                    parseFloat(pluginLocation[2]),
                                    parseFloat(pluginLocation[3]));
    },

    /**
     * Resets the state variables of |this|.
     */
    resetState: function() {
      if (this.pdfPlugin_) {
        this.zoomLevel_ = this.pdfPlugin_.getZoomLevel();
        this.pageOffset_ = {
            x: this.pdfPlugin_.pageXOffset(),
            y: this.pdfPlugin_.pageYOffset()
        };
      }
      this.pdfLoaded_ = false;
    },

    /**
     * Adds event listeners for various events.
     * @private
     */
    addEventListeners_: function() {
      document.addEventListener('PDFLoaded',
                                this.onPDFLoaded_.bind(this));
    },

    /**
     * Listener executing when a PDFLoaded event occurs.
     * @private
     */
    onPDFLoaded_: function() {
      this.pdfPlugin_ = $('pdf-viewer');
      this.pdfLoaded_ = true;
      if (this.zoomLevel_ != null && this.pageOffset_ != null) {
        this.pdfPlugin_.setZoomLevel(this.zoomLevel_);
        this.pdfPlugin_.setPageXOffset(this.pageOffset_.x);
        this.pdfPlugin_.setPageYOffset(this.pageOffset_.y);
      } else {
        this.pdfPlugin_.fitToHeight();
      }
    }
  };

  return {
    PreviewArea: PreviewArea,
  };
});
