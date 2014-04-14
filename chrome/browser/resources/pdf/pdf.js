// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

<include src="../../../../ui/webui/resources/js/util.js">
<include src="viewport.js">
<include src="pdf_scripting_api.js">

/**
 * Creates a new PDFViewer. There should only be one of these objects per
 * document.
 */
function PDFViewer() {
  // The sizer element is placed behind the plugin element to cause scrollbars
  // to be displayed in the window. It is sized according to the document size
  // of the pdf and zoom level.
  this.sizer_ = $('sizer');
  this.toolbar_ = $('toolbar');
  this.pageIndicator_ = $('page-indicator');
  this.progressBar_ = $('progress-bar');
  this.passwordScreen_ = $('password-screen');
  this.passwordScreen_.addEventListener('password-submitted',
                                        this.onPasswordSubmitted_.bind(this));
  this.errorScreen_ = $('error-screen');
  this.errorScreen_.text = 'Failed to load PDF document';

  // Create the viewport.
  this.viewport_ = new Viewport(window,
                                this.sizer_,
                                this.viewportChangedCallback_.bind(this));

  // Create the plugin object dynamically so we can set its src. The plugin
  // element is sized to fill the entire window and is set to be fixed
  // positioning, acting as a viewport. The plugin renders into this viewport
  // according to the scroll position of the window.
  this.plugin_ = document.createElement('object');
  this.plugin_.id = 'plugin';
  this.plugin_.type = 'application/x-google-chrome-pdf';
  this.plugin_.addEventListener('message', this.handleMessage_.bind(this),
                                false);

  // If the viewer is started from a MIME type request, there will be a
  // background page and stream details object with the details of the request.
  // Otherwise, we take the query string of the URL to indicate the URL of the
  // PDF to load. This is used for print preview in particular.
  var streamDetails;
  if (chrome.extension.getBackgroundPage)
    streamDetails = chrome.extension.getBackgroundPage().popStreamDetails();

  if (!streamDetails) {
    // The URL of this page will be of the form
    // "chrome-extension://<extension id>?<pdf url>". We pull out the <pdf url>
    // part here.
    var url = window.location.search.substring(1);
    streamDetails = {
      streamUrl: url,
      originalUrl: url
    };
  }

  this.plugin_.setAttribute('src', streamDetails.streamUrl);
  document.body.appendChild(this.plugin_);

  this.messagingHost_ = new PDFMessagingHost(window, this);

  this.setupEventListeners_(streamDetails);
}

PDFViewer.prototype = {
  /**
   * @private
   * Sets up event listeners for key shortcuts and also the UI buttons.
   * @param {Object} streamDetails the details of the original HTTP request for
   *     the PDF.
   */
  setupEventListeners_: function(streamDetails) {
    // Setup the button event listeners.
    $('fit-to-width-button').addEventListener('click',
        this.viewport_.fitToWidth.bind(this.viewport_));
    $('fit-to-page-button').addEventListener('click',
        this.viewport_.fitToPage.bind(this.viewport_));
    $('zoom-in-button').addEventListener('click',
        this.viewport_.zoomIn.bind(this.viewport_));
    $('zoom-out-button').addEventListener('click',
        this.viewport_.zoomOut.bind(this.viewport_));
    $('save-button-link').href = streamDetails.originalUrl;
    $('print-button').addEventListener('click', this.print_.bind(this));

    // Setup keyboard event listeners.
    document.onkeydown = function(e) {
      switch (e.keyCode) {
        case 37:  // Left arrow key.
          // Go to the previous page if there are no horizontal scrollbars.
          if (!this.viewport_.documentHasScrollbars().x) {
            this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
            // Since we do the movement of the page.
            e.preventDefault();
          }
          return;
        case 33:  // Page up key.
          // Go to the previous page if we are fit-to-page.
          if (isFitToPageEnabled()) {
            this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
            // Since we do the movement of the page.
            e.preventDefault();
          }
          return;
        case 39:  // Right arrow key.
          // Go to the next page if there are no horizontal scrollbars.
          if (!this.viewport_.documentHasScrollbars().x) {
            this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
            // Since we do the movement of the page.
            e.preventDefault();
          }
          return;
        case 34:  // Page down key.
          // Go to the next page if we are fit-to-page.
          if (isFitToPageEnabled()) {
            this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
            // Since we do the movement of the page.
            e.preventDefault();
          }
          return;
        case 187:  // +/= key.
        case 107:  // Numpad + key.
          if (e.ctrlKey || e.metaKey) {
            this.viewport_.zoomIn();
            // Since we do the zooming of the page.
            e.preventDefault();
          }
          return;
        case 189:  // -/_ key.
        case 109:  // Numpad - key.
          if (e.ctrlKey || e.metaKey) {
            this.viewport_.zoomOut();
            // Since we do the zooming of the page.
            e.preventDefault();
          }
          return;
        case 83:  // s key.
          if (e.ctrlKey || e.metaKey) {
            // Simulate a click on the button so that the <a download ...>
            // attribute is used.
            $('save-button-link').click();
            // Since we do the saving of the page.
            e.preventDefault();
          }
          return;
        case 80:  // p key.
          if (e.ctrlKey || e.metaKey) {
            this.print_();
            // Since we do the printing of the page.
            e.preventDefault();
          }
          return;
      }
    }.bind(this);
  },

  /**
   * @private
   * Notify the plugin to print.
   */
  print_: function() {
    this.plugin_.postMessage({
      type: 'print',
    });
  },

  /**
   * @private
   * Update the loading progress of the document in response to a progress
   * message being received from the plugin.
   * @param {number} progress the progress as a percentage.
   */
  updateProgress_: function(progress) {
    this.progressBar_.progress = progress;
    if (progress == -1) {
      // Document load failed.
      this.errorScreen_.style.visibility = 'visible';
      this.sizer_.style.display = 'none';
      this.toolbar_.style.visibility = 'hidden';
      if (this.passwordScreen_.active) {
        this.passwordScreen_.deny();
        this.passwordScreen_.active = false;
      }
    } else if (progress == 100) {
      // Document load complete.
      this.messagingHost_.documentLoaded();
      if (this.lastViewportPosition_)
        this.viewport_.position = this.lastViewportPosition_;
    }
  },

  /**
   * @private
   * An event handler for handling password-submitted events. These are fired
   * when an event is entered into the password screen.
   * @param {Object} event a password-submitted event.
   */
  onPasswordSubmitted_: function(event) {
    this.plugin_.postMessage({
      type: 'getPasswordComplete',
      password: event.detail.password
    });
  },

  /**
   * @private
   * An event handler for handling message events received from the plugin.
   * @param {MessageObject} message a message event.
   */
  handleMessage_: function(message) {
    switch (message.data.type.toString()) {
      case 'documentDimensions':
        this.documentDimensions_ = message.data;
        this.viewport_.setDocumentDimensions(this.documentDimensions_);
        this.toolbar_.style.visibility = 'visible';
        // If we received the document dimensions, the password was good so we
        // can dismiss the password screen.
        if (this.passwordScreen_.active)
          this.passwordScreen_.accept();

        this.pageIndicator_.initialFadeIn();
        this.toolbar_.initialFadeIn();
        break;
      case 'loadProgress':
        this.updateProgress_(message.data.progress);
        break;
      case 'goToPage':
        this.viewport_.goToPage(message.data.page);
        break;
      case 'getPassword':
        // If the password screen isn't up, put it up. Otherwise we're
        // responding to an incorrect password so deny it.
        if (!this.passwordScreen_.active)
          this.passwordScreen_.active = true;
        else
          this.passwordScreen_.deny();
    }
  },

  /**
   * @private
   * A callback that's called when the viewport changes.
   */
  viewportChangedCallback_: function() {
    if (!this.documentDimensions_)
      return;

    // Update the buttons selected.
    $('fit-to-page-button').classList.remove('polymer-selected');
    $('fit-to-width-button').classList.remove('polymer-selected');
    if (this.viewport_.fittingType == Viewport.FittingType.FIT_TO_PAGE) {
      $('fit-to-page-button').classList.add('polymer-selected');
    } else if (this.viewport_.fittingType ==
               Viewport.FittingType.FIT_TO_WIDTH) {
      $('fit-to-width-button').classList.add('polymer-selected');
    }

    var hasScrollbars = this.viewport_.documentHasScrollbars();
    var scrollbarWidth = this.viewport_.scrollbarWidth;
    // Offset the toolbar position so that it doesn't move if scrollbars appear.
    var toolbarRight = hasScrollbars.vertical ? 0 : scrollbarWidth;
    var toolbarBottom = hasScrollbars.horizontal ? 0 : scrollbarWidth;
    this.toolbar_.style.right = toolbarRight + 'px';
    this.toolbar_.style.bottom = toolbarBottom + 'px';

    // Update the page indicator.
    this.pageIndicator_.index = this.viewport_.getMostVisiblePage() + 1;
    if (this.documentDimensions_.pageDimensions.length > 1 && hasScrollbars.y)
      this.pageIndicator_.style.visibility = 'visible';
    else
      this.pageIndicator_.style.visibility = 'hidden';

    this.messagingHost_.viewportChanged();

    var position = this.viewport_.position;
    var zoom = this.viewport_.zoom;
    // Notify the plugin of the viewport change.
    this.plugin_.postMessage({
      type: 'viewport',
      zoom: zoom,
      xOffset: position.x,
      yOffset: position.y
    });
  },

  /**
   * Resets the viewer into print preview mode, which is used for Chrome print
   * preview.
   * @param {string} url the url of the pdf to load.
   * @param {boolean} grayscale true if the pdf should be displayed in
   *     grayscale, false otherwise.
   * @param {Array.<number>} pageNumbers an array of the number to label each
   *     page in the document.
   * @param {boolean} modifiable whether the PDF is modifiable or not.
   */
  resetPrintPreviewMode: function(url,
                                  grayscale,
                                  pageNumbers,
                                  modifiable) {
    if (!this.inPrintPreviewMode_) {
      this.inPrintPreviewMode_ = true;
      this.viewport_.fitToPage();
    }

    // Stash the scroll location so that it can be restored when the new
    // document is loaded.
    this.lastViewportPosition_ = this.viewport_.position;

    // TODO(raymes): Disable these properly in the plugin.
    var printButton = $('print-button');
    if (printButton)
      printButton.parentNode.removeChild(printButton);
    var saveButton = $('save-button');
    if (saveButton)
      saveButton.parentNode.removeChild(saveButton);

    this.pageIndicator_.pageLabels = pageNumbers;

    this.plugin_.postMessage({
      type: 'resetPrintPreviewMode',
      url: url,
      grayscale: grayscale,
      // If the PDF isn't modifiable we send 0 as the page count so that no
      // blank placeholder pages get appended to the PDF.
      pageCount: (modifiable ? pageNumbers.length : 0)
    });
  },

  /**
   * Load a page into the document while in print preview mode.
   * @param {string} url the url of the pdf page to load.
   * @param {number} index the index of the page to load.
   */
  loadPreviewPage: function(url, index) {
    this.plugin_.postMessage({
      type: 'loadPreviewPage',
      url: url,
      index: index
    });
  },

  /**
   * @type {Viewport} the viewport of the PDF viewer.
   */
  get viewport() {
    return this.viewport_;
  }

}

new PDFViewer();

})();
