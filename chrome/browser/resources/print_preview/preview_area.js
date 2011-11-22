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

    // @type {HTMLDivElement} A layer on top of |this.pdfPlugin_| used for
    // displaying messages to the user.
    this.overlayLayer = $('overlay-layer');
    // @type {HTMLDivElement} Contains text displayed to the user followed by
    // three animated dots.
    this.customMessageWithDots_ = $('custom-message-with-dots');
    // @type {HTMLDivElement} Contains text displayed to the user.
    this.customMessage_ = $('custom-message');
    // @type {HTMLInputElement} Button associated with a displayed error
    // message.
    this.errorButton = $('error-button');
    // @type {HTMLDivElement} Contains three animated (dancing) dots.
    this.dancingDotsText = $('dancing-dots-text');

    // True if the pdf document is loaded in the preview area.
    this.pdfLoaded_ = false;

    // Contains the zoom level just before a new preview is requested so the
    // same zoom level can be restored.
    this.zoomLevel_ = null;
    // @type {{x: number, y: number}} Contains the page offset values just
    //     before a new preview is requested so that the scroll amount can be
    //     restored later.
    this.pageOffset_ = null;
    // @type {print_preview.Rect} A rectangle describing the postion of the
    // most visible page normalized with respect to the total height and width
    // of the plugin.
    this.pageLocationNormalized = null;

    // @type {EventTracker} Used to keep track of certain event listeners.
    this.eventTracker = new EventTracker();

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

    get pdfPlugin() {
      return this.pdfPlugin_;
    },

    get pdfLoaded() {
      return this.pdfLoaded_;
    },

    set pdfLoaded(pdfLoaded) {
      this.pdfLoaded_ = pdfLoaded;
    },

    /**
     * Queries the plugin for the location of the most visible page and updates
     * |this.pageLocationNormalized|.
     */
    update: function() {
      if (!this.pdfLoaded_)
        return;
      var pluginLocation =
          this.pdfPlugin_.getPageLocationNormalized().split(';');
      this.pageLocationNormalized = new print_preview.Rect(
          parseFloat(pluginLocation[0]),
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
      document.addEventListener(customEvents.PDF_LOADED,
                                this.onPDFLoaded_.bind(this));
    },

    /**
     * Listener executing when a |customEvents.PDF_LOADED| event occurs.
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
    },

    /**
     * Hides the |this.overlayLayer| and any messages currently displayed.
     */
    hideOverlayLayer: function() {
      this.eventTracker.add(this.overlayLayer, 'webkitTransitionEnd',
          this.hideOverlayLayerCleanup_.bind(this), false);
      if (this.pdfPlugin_)
        this.pdfPlugin_.classList.remove('invisible');
      this.overlayLayer.classList.add('invisible');
    },

    /**
     * Displays the "Preview loading..." animation.
     */
    showLoadingAnimation: function() {
      this.showCustomMessage(localStrings.getString('loading'));
    },

    /**
     * Necessary cleanup so that the dancing dots animation is not being
     * rendered in the background when not displayed.
     */
    hideOverlayLayerCleanup_: function() {
      this.customMessageWithDots_.hidden = true;
      this.eventTracker.remove(this.overlayLayer, 'webkitTransitionEnd');
    },

    /**
     * Displays |message| followed by three dancing dots animation.
     * @param {string} message The message to be displayed.
     */
    showCustomMessage: function(message) {
      this.customMessageWithDots_.innerHTML = message +
          this.dancingDotsText.innerHTML;
      this.customMessageWithDots_.hidden = false;
      if (this.pdfPlugin_)
        this.pdfPlugin_.classList.add('invisible');
      this.overlayLayer.classList.remove('invisible');
    },

    /**
     * Clears the custom message with dots animation.
     */
    clearCustomMessageWithDots: function() {
      this.customMessageWithDots_.innerHTML = '';
      this.customMessageWithDots_.hidden = true;
    },

    /**
     * Display an error message in the center of the preview area.
     * @param {string} errorMessage The error message to be displayed.
     */
    displayErrorMessageAndNotify: function(errorMessage) {
      this.overlayLayer.classList.remove('invisible');
      this.customMessage_.textContent = errorMessage;
      this.customMessage_.hidden = false;
      this.customMessageWithDots_.innerHTML = '';
      this.customMessageWithDots_.hidden = true;
      if (this.pdfPlugin_) {
        $('mainview').removeChild(this.pdfPlugin_);
        this.pdfPlugin_ = null;
      }
      cr.dispatchSimpleEvent(document, customEvents.PDF_GENERATION_ERROR);
    },

    /**
     * Display an error message in the center of the preview area followed by a
     * button.
     * @param {string} errorMessage The error message to be displayed.
     * @param {string} buttonText The text to be displayed within the button.
     * @param {string} buttonListener The listener to be executed when the
     *     button is clicked.
     */
    displayErrorMessageWithButtonAndNotify: function(
        errorMessage, buttonText, buttonListener) {
      this.errorButton.disabled = false;
      this.errorButton.textContent = buttonText;
      this.errorButton.onclick = buttonListener;
      this.errorButton.hidden = false;
      $('system-dialog-throbber').hidden = true;
      $('native-print-dialog-throbber').hidden = true;
      if (cr.isMac)
        $('open-preview-app-throbber').hidden = true;
      this.displayErrorMessageAndNotify(errorMessage);
    }
  };

  return {
    PreviewArea: PreviewArea,
  };
});
