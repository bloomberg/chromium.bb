// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Create a new PDFMessagingClient. This provides a scripting interface to
 * the PDF viewer so that it can be customized by things like print preview.
 * @param {HTMLIFrameElement} iframe an iframe containing the PDF viewer.
 * @param {Window} window the window of the page containing the iframe.
 * @param {string} extensionUrl the url of the PDF extension.
 */
function PDFMessagingClient(iframe, window, extensionUrl) {
  this.iframe_ = iframe;
  this.extensionUrl_ = extensionUrl;
  this.readyToReceive_ = false;
  this.messageQueue_ = [];
  window.addEventListener('message', function(event) {
    switch (event.data.type) {
      case 'readyToReceive':
        this.flushPendingMessages_();
        break;
      case 'viewportChanged':
        this.viewportChangedCallback_(event.data.pageX,
                                      event.data.pageY,
                                      event.data.pageWidth,
                                      event.data.viewportWidth,
                                      event.data.viewportHeight);
        break;
      case 'documentLoaded':
        this.loadCallback_();
        break;
    }
  }.bind(this), false);
}

PDFMessagingClient.prototype = {
  /**
   * @private
   * Send a message to the extension. If we try to send messages prior to the
   * extension being ready to receive messages (i.e. before it has finished
   * loading) we queue up the messages and flush them later.
   * @param {MessageObject} the message to send.
   */
  sendMessage_: function(message) {
    if (!this.readyToReceive_) {
      this.messageQueue_.push(message);
      return;
    }

    this.iframe_.contentWindow.postMessage(message, this.extensionUrl_);
  },

  /**
   * @private
   * Flushes all pending messages to the extension.
   */
  flushPendingMessages_: function() {
    this.readyToReceive_ = true;
    while (this.messageQueue_.length != 0) {
      this.iframe_.contentWindow.postMessage(this.messageQueue_.shift(),
                                             this.extensionUrl_);
    }
  },

  /**
   * Sets the callback which will be run when the PDF viewport changes.
   * @param {Function} callback the callback to be called.
   */
  setViewportChangedCallback: function(callback) {
    this.viewportChangedCallback_ = callback;
  },

  /**
   * Sets the callback which will be run when the PDF document has finished
   * loading.
   * @param {Function} callback the callback to be called.
   */
  setLoadCallback: function(callback) {
    this.loadCallback_ = callback;
  },

  /**
   * Resets the PDF viewer into print preview mode.
   * @param {string} url the url of the PDF to load.
   * @param {boolean} grayscale whether or not to display the PDF in grayscale.
   * @param {Array.<number>} pageNumbers an array of the page numbers.
   * @param {boolean} modifiable whether or not the document is modifiable.
   */
  resetPrintPreviewMode: function(url, grayscale, pageNumbers, modifiable) {
    this.sendMessage_({
      type: 'resetPrintPreviewMode',
      url: url,
      grayscale: grayscale,
      pageNumbers: pageNumbers,
      modifiable: modifiable
    });
  },

  /**
   * Load a page into the document while in print preview mode.
   * @param {string} url the url of the pdf page to load.
   * @param {number} index the index of the page to load.
   */
  loadPreviewPage: function(url, index) {
    this.sendMessage_({
      type: 'loadPreviewPage',
      url: url,
      index: index
    });
  },

  /**
   * Send a key event to the extension.
   * @param {number} keyCode the key code to send to the extension.
   */
  sendKeyEvent: function(keyCode) {
    // TODO(raymes): Figure out a way to do this. It's only used to do scrolling
    // the viewport, so probably just expose viewport controls instead.
  },
};

/**
 * Creates a PDF viewer with a scripting interface. This is basically 1) an
 * iframe which is navigated to the PDF viewer extension and 2) a scripting
 * interface which provides access to various features of the viewer for use
 * by print preview and accessbility.
 * @param {string} src the source URL of the PDF to load initially.
 * @return {HTMLIFrameElement} the iframe element containing the PDF viewer.
 */
function PDFCreateOutOfProcessPlugin(src) {
  var EXTENSION_URL = 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai';
  var iframe = window.document.createElement('iframe');
  iframe.setAttribute('src', EXTENSION_URL + '/index.html?' + src);
  var client = new PDFMessagingClient(iframe, window, EXTENSION_URL);

  // Add the functions to the iframe so that they can be called directly.
  iframe.setViewportChangedCallback =
      client.setViewportChangedCallback.bind(client);
  iframe.setLoadCallback = client.setLoadCallback.bind(client);
  iframe.resetPrintPreviewMode = client.resetPrintPreviewMode.bind(client);
  iframe.loadPreviewPage = client.loadPreviewPage.bind(client);
  iframe.sendKeyEvent = client.sendKeyEvent.bind(client);
  return iframe;
}

/**
 * Create a new PDFMessagingHost. This is the extension-side of the scripting
 * interface to the PDF viewer. It handles requests from a page which contains
 * a PDF viewer extension and translates them into actions on the viewer.
 * @param {Window} window the window containing the PDF extension.
 * @param {PDFViewer} pdfViewer the object which provides access to the viewer.
 */
function PDFMessagingHost(window, pdfViewer) {
  this.window_ = window;
  this.pdfViewer_ = pdfViewer;
  this.viewport_ = pdfViewer.viewport;

  window.addEventListener('message', function(event) {
    switch (event.data.type) {
      case 'resetPrintPreviewMode':
        this.pdfViewer_.resetPrintPreviewMode(
            event.data.url,
            event.data.grayscale,
            event.data.pageNumbers,
            event.data.modifiable);

        break;
      case 'loadPreviewPage':
        this.pdfViewer_.loadPreviewPage(event.data.url, event.data.index);
        break;
    }
  }.bind(this), false);

  if (this.window_.parent != this.window_)
    this.sendMessage_({type: 'readyToReceive'});
}

PDFMessagingHost.prototype = {
  sendMessage_: function(message) {
    if (this.window_.parent == this.window_)
      return;
    this.window_.parent.postMessage(message, '*');
  },

  viewportChanged: function() {
    var visiblePage = this.viewport_.getMostVisiblePage();
    var pageDimensions = this.viewport_.getPageScreenRect(visiblePage);
    var size = this.viewport_.size;

    this.sendMessage_({
      type: 'viewportChanged',
      pageX: pageDimensions.x,
      pageY: pageDimensions.y,
      pageWidth: pageDimensions.width,
      viewportWidth: size.width,
      viewportHeight: size.height,
    });
  },

  documentLoaded: function() {
    if (this.window_.parent == this.window_)
      return;
    this.sendMessage_({ type: 'documentLoaded' });
  }
};
