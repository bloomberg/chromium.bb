// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @return {number} Width of a scrollbar in pixels
 */
function getScrollbarWidth() {
  var div = document.createElement('div');
  div.style.visibility = 'hidden';
  div.style.overflow = 'scroll';
  div.style.width = '50px';
  div.style.height = '50px';
  div.style.position = 'absolute';
  document.body.appendChild(div);
  var result = div.offsetWidth - div.clientWidth;
  div.parentNode.removeChild(div);
  return result;
}

/**
 * Return the filename component of a URL.
 * @param {string} url The URL to get the filename from.
 * @return {string} The filename component.
 */
function getFilenameFromURL(url) {
  var components = url.split(/\/|\\/);
  return components[components.length - 1];
}

/**
 * Called when navigation happens in the current tab.
 * @param {string} url The url to be opened in the current tab.
 */
function onNavigateInCurrentTab(url) {
  window.location.href = url;
}

/**
 * Called when navigation happens in the new tab.
 * @param {string} url The url to be opened in the new tab.
 */
function onNavigateInNewTab(url) {
  // Prefer the tabs API because it guarantees we can just open a new tab.
  // window.open doesn't have this guarantee.
  if (chrome.tabs)
    chrome.tabs.create({ url: url});
  else
    window.open(url);
}

/**
 * The minimum number of pixels to offset the toolbar by from the bottom and
 * right side of the screen.
 */
PDFViewer.MIN_TOOLBAR_OFFSET = 15;

/**
 * Creates a new PDFViewer. There should only be one of these objects per
 * document.
 * @constructor
 * @param {Object} streamDetails The stream object which points to the data
 *     contained in the PDF.
 */
function PDFViewer(streamDetails) {
  this.streamDetails_ = streamDetails;
  this.loaded_ = false;
  this.parentWindow_ = null;

  this.delayedScriptingMessages_ = [];

  this.isPrintPreview_ =
      this.streamDetails_.originalUrl.indexOf('chrome://print') == 0;
  this.isMaterial_ = location.pathname.substring(1) === 'index-material.html';

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

  // Create the viewport.
  this.viewport_ = new Viewport(window,
                                this.sizer_,
                                this.viewportChanged_.bind(this),
                                this.beforeZoom_.bind(this),
                                this.afterZoom_.bind(this),
                                getScrollbarWidth());

  // Create the plugin object dynamically so we can set its src. The plugin
  // element is sized to fill the entire window and is set to be fixed
  // positioning, acting as a viewport. The plugin renders into this viewport
  // according to the scroll position of the window.
  this.plugin_ = document.createElement('embed');
  // NOTE: The plugin's 'id' field must be set to 'plugin' since
  // chrome/renderer/printing/print_web_view_helper.cc actually references it.
  this.plugin_.id = 'plugin';
  this.plugin_.type = 'application/x-google-chrome-pdf';
  this.plugin_.addEventListener('message', this.handlePluginMessage_.bind(this),
                                false);

  // Handle scripting messages from outside the extension that wish to interact
  // with it. We also send a message indicating that extension has loaded and
  // is ready to receive messages.
  window.addEventListener('message', this.handleScriptingMessage.bind(this),
                          false);

  document.title = getFilenameFromURL(this.streamDetails_.originalUrl);
  this.plugin_.setAttribute('src', this.streamDetails_.originalUrl);
  this.plugin_.setAttribute('stream-url', this.streamDetails_.streamUrl);
  var headers = '';
  for (var header in this.streamDetails_.responseHeaders) {
    headers += header + ': ' +
        this.streamDetails_.responseHeaders[header] + '\n';
  }
  this.plugin_.setAttribute('headers', headers);

  if (this.isMaterial_)
    this.plugin_.setAttribute('is-material', '');

  if (!this.streamDetails_.embedded)
    this.plugin_.setAttribute('full-frame', '');
  document.body.appendChild(this.plugin_);

  // Setup the button event listeners.
  if (!this.isMaterial_) {
    $('fit-to-width-button').addEventListener('click',
        this.viewport_.fitToWidth.bind(this.viewport_));
    $('fit-to-page-button').addEventListener('click',
        this.viewport_.fitToPage.bind(this.viewport_));
    $('zoom-in-button').addEventListener('click',
        this.viewport_.zoomIn.bind(this.viewport_));
    $('zoom-out-button').addEventListener('click',
        this.viewport_.zoomOut.bind(this.viewport_));
    $('save-button').addEventListener('click', this.save_.bind(this));
    $('print-button').addEventListener('click', this.print_.bind(this));
  }

  if (this.isMaterial_) {
    this.bookmarksPane_ = $('bookmarks-pane');

    this.zoomSelector_ = $('zoom-selector');
    this.zoomSelector_.zoomMin = Viewport.ZOOM_FACTOR_RANGE.min * 100;
    this.zoomSelector_.zoomMax = Viewport.ZOOM_FACTOR_RANGE.max * 100;
    this.zoomSelector_.addEventListener('zoom', function(e) {
      this.viewport_.setZoom(e.detail.zoom);
    }.bind(this));
    this.zoomSelector_.addEventListener('fit-to-width',
        this.viewport_.fitToWidth.bind(this.viewport_));
    this.zoomSelector_.addEventListener('fit-to-page',
        this.viewport_.fitToPage.bind(this.viewport_));

    this.materialToolbar_ = $('material-toolbar');
    this.materialToolbar_.docTitle = document.title;
    this.materialToolbar_.addEventListener('save', this.save_.bind(this));
    this.materialToolbar_.addEventListener('print', this.print_.bind(this));
    this.materialToolbar_.addEventListener('rotate-right',
        this.rotateClockwise_.bind(this));
    this.materialToolbar_.addEventListener('toggle-bookmarks', function() {
      this.bookmarksPane_.buttonToggle();
    }.bind(this));

    document.body.addEventListener('change-page', function(e) {
      this.viewport_.goToPage(e.detail.page);
    }.bind(this));

    this.uiManager_ = new UiManager(window, this.materialToolbar_,
                                    [this.bookmarksPane_]);
  }

  // Setup the keyboard event listener.
  document.onkeydown = this.handleKeyEvent_.bind(this);

  // Set up the zoom API.
  if (this.shouldManageZoom_()) {
    chrome.tabs.setZoomSettings(this.streamDetails_.tabId,
                                {mode: 'manual', scope: 'per-tab'},
                                this.afterZoom_.bind(this));
    chrome.tabs.onZoomChange.addListener(function(zoomChangeInfo) {
      if (zoomChangeInfo.tabId != this.streamDetails_.tabId)
        return;
      // If the zoom level is close enough to the current zoom level, don't
      // change it. This avoids us getting into an infinite loop of zoom changes
      // due to floating point error.
      var MIN_ZOOM_DELTA = 0.01;
      var zoomDelta = Math.abs(this.viewport_.zoom -
                               zoomChangeInfo.newZoomFactor);
      // We should not change zoom level when we are responsible for initiating
      // the zoom. onZoomChange() is called before setZoomComplete() callback
      // when we initiate the zoom.
      if ((zoomDelta > MIN_ZOOM_DELTA) && !this.setZoomInProgress_)
        this.viewport_.setZoom(zoomChangeInfo.newZoomFactor);
    }.bind(this));
  }

  // Parse open pdf parameters.
  this.paramsParser_ =
      new OpenPDFParamsParser(this.getNamedDestination_.bind(this));
  this.navigator_ = new Navigator(this.streamDetails_.originalUrl,
                                  this.viewport_, this.paramsParser_,
                                  onNavigateInCurrentTab, onNavigateInNewTab);
  this.viewportScroller_ =
      new ViewportScroller(this.viewport_, this.plugin_, window);
}

PDFViewer.prototype = {
  /**
   * @private
   * Handle key events. These may come from the user directly or via the
   * scripting API.
   * @param {KeyboardEvent} e the event to handle.
   */
  handleKeyEvent_: function(e) {
    var position = this.viewport_.position;
    // Certain scroll events may be sent from outside of the extension.
    var fromScriptingAPI = e.fromScriptingAPI;

    var pageUpHandler = function() {
      // Go to the previous page if we are fit-to-page.
      if (this.viewport_.fittingType == Viewport.FittingType.FIT_TO_PAGE) {
        this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
        // Since we do the movement of the page.
        e.preventDefault();
      } else if (fromScriptingAPI) {
        position.y -= this.viewport.size.height;
        this.viewport.position = position;
      }
    }.bind(this);
    var pageDownHandler = function() {
      // Go to the next page if we are fit-to-page.
      if (this.viewport_.fittingType == Viewport.FittingType.FIT_TO_PAGE) {
        this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
        // Since we do the movement of the page.
        e.preventDefault();
      } else if (fromScriptingAPI) {
        position.y += this.viewport.size.height;
        this.viewport.position = position;
      }
    }.bind(this);

    switch (e.keyCode) {
      case 32:  // Space key.
        if (e.shiftKey)
          pageUpHandler();
        else
          pageDownHandler();
        return;
      case 33:  // Page up key.
        pageUpHandler();
        return;
      case 34:  // Page down key.
        pageDownHandler();
        return;
      case 37:  // Left arrow key.
        if (!(e.altKey || e.ctrlKey || e.metaKey || e.shiftKey)) {
          // Go to the previous page if there are no horizontal scrollbars.
          if (!this.viewport_.documentHasScrollbars().horizontal) {
            this.viewport_.goToPage(this.viewport_.getMostVisiblePage() - 1);
            // Since we do the movement of the page.
            e.preventDefault();
          } else if (fromScriptingAPI) {
            position.x -= Viewport.SCROLL_INCREMENT;
            this.viewport.position = position;
          }
        }
        return;
      case 38:  // Up arrow key.
        if (fromScriptingAPI) {
          position.y -= Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 39:  // Right arrow key.
        if (!(e.altKey || e.ctrlKey || e.metaKey || e.shiftKey)) {
          // Go to the next page if there are no horizontal scrollbars.
          if (!this.viewport_.documentHasScrollbars().horizontal) {
            this.viewport_.goToPage(this.viewport_.getMostVisiblePage() + 1);
            // Since we do the movement of the page.
            e.preventDefault();
          } else if (fromScriptingAPI) {
            position.x += Viewport.SCROLL_INCREMENT;
            this.viewport.position = position;
          }
        }
        return;
      case 40:  // Down arrow key.
        if (fromScriptingAPI) {
          position.y += Viewport.SCROLL_INCREMENT;
          this.viewport.position = position;
        }
        return;
      case 65:  // a key.
        if (e.ctrlKey || e.metaKey) {
          this.plugin_.postMessage({
            type: 'selectAll'
          });
          // Since we do selection ourselves.
          e.preventDefault();
        }
        return;
      case 71: // g key.
        if (this.isMaterial_ && (e.ctrlKey || e.metaKey)) {
          this.materialToolbar_.selectPageNumber();
          // To prevent the default "find text" behaviour in Chrome.
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
      case 219:  // left bracket.
        if (e.ctrlKey)
          this.rotateCounterClockwise_();
        return;
      case 221:  // right bracket.
        if (e.ctrlKey)
          this.rotateClockwise_();
        return;
    }

    // Give print preview a chance to handle the key event.
    if (!fromScriptingAPI && this.isPrintPreview_) {
      this.sendScriptingMessage_({
        type: 'sendKeyEvent',
        keyEvent: SerializeKeyEvent(e)
      });
    }
  },

  /**
   * @private
   * Rotate the plugin clockwise.
   */
  rotateClockwise_: function() {
    this.plugin_.postMessage({
      type: 'rotateClockwise'
    });
  },

  /**
   * @private
   * Rotate the plugin counter-clockwise.
   */
  rotateCounterClockwise_: function() {
    this.plugin_.postMessage({
      type: 'rotateCounterclockwise'
    });
  },

  /**
   * @private
   * Notify the plugin to print.
   */
  print_: function() {
    this.plugin_.postMessage({
      type: 'print'
    });
  },

  /**
   * @private
   * Notify the plugin to save.
   */
  save_: function() {
    this.plugin_.postMessage({
      type: 'save'
    });
  },

  /**
   * Fetches the page number corresponding to the given named destination from
   * the plugin.
   * @param {string} name The namedDestination to fetch page number from plugin.
   */
  getNamedDestination_: function(name) {
    this.plugin_.postMessage({
      type: 'getNamedDestination',
      namedDestination: name
    });
  },

  /**
   * @private
   * Handle open pdf parameters. This function updates the viewport as per
   * the parameters mentioned in the url while opening pdf. The order is
   * important as later actions can override the effects of previous actions.
   * @param {Object} viewportPosition The initial position of the viewport to be
   *     displayed.
   */
  handleURLParams_: function(viewportPosition) {
    if (viewportPosition.page != undefined)
      this.viewport_.goToPage(viewportPosition.page);
    if (viewportPosition.position) {
      // Make sure we don't cancel effect of page parameter.
      this.viewport_.position = {
        x: this.viewport_.position.x + viewportPosition.position.x,
        y: this.viewport_.position.y + viewportPosition.position.y
      };
    }
    if (viewportPosition.zoom)
      this.viewport_.setZoom(viewportPosition.zoom);
  },

  /**
   * @private
   * Update the loading progress of the document in response to a progress
   * message being received from the plugin.
   * @param {number} progress the progress as a percentage.
   */
  updateProgress_: function(progress) {
    if (this.isMaterial_)
      this.materialToolbar_.loadProgress = progress;
    else
      this.progressBar_.progress = progress;

    if (progress == -1) {
      // Document load failed.
      this.errorScreen_.style.visibility = 'visible';
      this.sizer_.style.display = 'none';
      if (!this.isMaterial_)
        this.toolbar_.style.visibility = 'hidden';
      if (this.passwordScreen_.active) {
        this.passwordScreen_.deny();
        this.passwordScreen_.active = false;
      }
    } else if (progress == 100) {
      // Document load complete.
      if (this.lastViewportPosition_)
        this.viewport_.position = this.lastViewportPosition_;
      this.paramsParser_.getViewportFromUrlParams(
          this.streamDetails_.originalUrl, this.handleURLParams_.bind(this));
      this.loaded_ = true;
      this.sendScriptingMessage_({
        type: 'documentLoaded'
      });
      while (this.delayedScriptingMessages_.length > 0)
        this.handleScriptingMessage(this.delayedScriptingMessages_.shift());

      if (this.isMaterial_)
        this.uiManager_.hideUiAfterTimeout();
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
  handlePluginMessage_: function(message) {
    switch (message.data.type.toString()) {
      case 'documentDimensions':
        this.documentDimensions_ = message.data;
        this.viewport_.setDocumentDimensions(this.documentDimensions_);
        // If we received the document dimensions, the password was good so we
        // can dismiss the password screen.
        if (this.passwordScreen_.active)
          this.passwordScreen_.accept();

        if (this.isMaterial_) {
          this.materialToolbar_.docLength =
              this.documentDimensions_.pageDimensions.length;
        } else {
          this.pageIndicator_.initialFadeIn();
          this.toolbar_.initialFadeIn();
        }

        break;
      case 'email':
        var href = 'mailto:' + message.data.to + '?cc=' + message.data.cc +
            '&bcc=' + message.data.bcc + '&subject=' + message.data.subject +
            '&body=' + message.data.body;
        window.location.href = href;
        break;
      case 'getAccessibilityJSONReply':
        this.sendScriptingMessage_(message.data);
        break;
      case 'getPassword':
        // If the password screen isn't up, put it up. Otherwise we're
        // responding to an incorrect password so deny it.
        if (!this.passwordScreen_.active)
          this.passwordScreen_.active = true;
        else
          this.passwordScreen_.deny();
        break;
      case 'getSelectedTextReply':
        this.sendScriptingMessage_(message.data);
        break;
      case 'goToPage':
        this.viewport_.goToPage(message.data.page);
        break;
      case 'loadProgress':
        this.updateProgress_(message.data.progress);
        break;
      case 'navigate':
        // If in print preview, always open a new tab.
        if (this.isPrintPreview_)
          this.navigator_.navigate(message.data.url, true);
        else
          this.navigator_.navigate(message.data.url, message.data.newTab);
        break;
      case 'setScrollPosition':
        var position = this.viewport_.position;
        if (message.data.x !== undefined)
          position.x = message.data.x;
        if (message.data.y !== undefined)
          position.y = message.data.y;
        this.viewport_.position = position;
        break;
      case 'setTranslatedStrings':
        this.passwordScreen_.text = message.data.getPasswordString;
        if (!this.isMaterial_) {
          this.progressBar_.text = message.data.loadingString;
          if (!this.isPrintPreview_)
            this.progressBar_.style.visibility = 'visible';
        }
        this.errorScreen_.text = message.data.loadFailedString;
        break;
      case 'cancelStreamUrl':
        chrome.mimeHandlerPrivate.abortStream();
        break;
      case 'bookmarks':
        this.bookmarks_ = message.data.bookmarks;
        if (this.isMaterial_ && this.bookmarks_.length !== 0) {
          $('bookmarks-container').bookmarks = this.bookmarks;
          this.materialToolbar_.hasBookmarks = true;
        }
        break;
      case 'setIsSelecting':
        this.viewportScroller_.setEnableScrolling(message.data.isSelecting);
        break;
      case 'getNamedDestinationReply':
        this.paramsParser_.onNamedDestinationReceived(
            message.data.pageNumber);
        break;
    }
  },

  /**
   * @private
   * A callback that's called before the zoom changes. Notify the plugin to stop
   * reacting to scroll events while zoom is taking place to avoid flickering.
   */
  beforeZoom_: function() {
    this.plugin_.postMessage({
      type: 'stopScrolling'
    });
  },

  /**
   * @private
   * A callback that's called after the zoom changes. Notify the plugin of the
   * zoom change and to continue reacting to scroll events.
   */
  afterZoom_: function() {
    var position = this.viewport_.position;
    var zoom = this.viewport_.zoom;
    if (this.isMaterial_)
      this.zoomSelector_.zoomValue = 100 * zoom;
    if (this.shouldManageZoom_() && !this.setZoomInProgress_) {
      this.setZoomInProgress_ = true;
      chrome.tabs.setZoom(this.streamDetails_.tabId, zoom,
                          this.setZoomComplete_.bind(this, zoom));
    }
    this.plugin_.postMessage({
      type: 'viewport',
      zoom: zoom,
      xOffset: position.x,
      yOffset: position.y
    });
  },

  /**
   * @private
   * A callback that's called after chrome.tabs.setZoom is complete. This will
   * call chrome.tabs.setZoom again if the zoom level has changed since it was
   * last called.
   * @param {number} lastZoom the zoom level that chrome.tabs.setZoom was called
   *     with.
   */
  setZoomComplete_: function(lastZoom) {
    var zoom = this.viewport_.zoom;
    if (zoom !== lastZoom) {
      chrome.tabs.setZoom(this.streamDetails_.tabId, zoom,
                          this.setZoomComplete_.bind(this, zoom));
    } else {
      this.setZoomInProgress_ = false;
    }
  },

  /**
   * @private
   * A callback that's called after the viewport changes.
   */
  viewportChanged_: function() {
    if (!this.documentDimensions_)
      return;

    // Update the buttons selected.
    if (!this.isMaterial_) {
      $('fit-to-page-button').classList.remove('polymer-selected');
      $('fit-to-width-button').classList.remove('polymer-selected');
      if (this.viewport_.fittingType == Viewport.FittingType.FIT_TO_PAGE) {
        $('fit-to-page-button').classList.add('polymer-selected');
      } else if (this.viewport_.fittingType ==
                 Viewport.FittingType.FIT_TO_WIDTH) {
        $('fit-to-width-button').classList.add('polymer-selected');
      }
    }

    // Offset the toolbar position so that it doesn't move if scrollbars appear.
    var hasScrollbars = this.viewport_.documentHasScrollbars();
    var scrollbarWidth = this.viewport_.scrollbarWidth;
    var verticalScrollbarWidth = hasScrollbars.vertical ? scrollbarWidth : 0;
    var horizontalScrollbarWidth =
        hasScrollbars.horizontal ? scrollbarWidth : 0;
    var toolbarRight = Math.max(PDFViewer.MIN_TOOLBAR_OFFSET, scrollbarWidth);
    var toolbarBottom = Math.max(PDFViewer.MIN_TOOLBAR_OFFSET, scrollbarWidth);
    toolbarRight -= verticalScrollbarWidth;
    toolbarBottom -= horizontalScrollbarWidth;
    if (!this.isMaterial_) {
      this.toolbar_.style.right = toolbarRight + 'px';
      this.toolbar_.style.bottom = toolbarBottom + 'px';
      // Hide the toolbar if it doesn't fit in the viewport.
      if (this.toolbar_.offsetLeft < 0 || this.toolbar_.offsetTop < 0)
        this.toolbar_.style.visibility = 'hidden';
      else
        this.toolbar_.style.visibility = 'visible';
    }

    // Update the page indicator.
    var visiblePage = this.viewport_.getMostVisiblePage();
    if (this.isMaterial_) {
      this.materialToolbar_.pageIndex = visiblePage;
    } else {
      this.pageIndicator_.index = visiblePage;
      if (this.documentDimensions_.pageDimensions.length > 1 &&
          hasScrollbars.vertical) {
        this.pageIndicator_.style.visibility = 'visible';
      } else {
        this.pageIndicator_.style.visibility = 'hidden';
      }
    }

    var visiblePageDimensions = this.viewport_.getPageScreenRect(visiblePage);
    var size = this.viewport_.size;
    this.sendScriptingMessage_({
      type: 'viewport',
      pageX: visiblePageDimensions.x,
      pageY: visiblePageDimensions.y,
      pageWidth: visiblePageDimensions.width,
      viewportWidth: size.width,
      viewportHeight: size.height
    });
  },

  /**
   * Handle a scripting message from outside the extension (typically sent by
   * PDFScriptingAPI in a page containing the extension) to interact with the
   * plugin.
   * @param {MessageObject} message the message to handle.
   */
  handleScriptingMessage: function(message) {
    if (this.parentWindow_ != message.source) {
      this.parentWindow_ = message.source;
      // Ensure that we notify the embedder if the document is loaded.
      if (this.loaded_) {
        this.sendScriptingMessage_({
          type: 'documentLoaded'
        });
      }
    }

    if (this.handlePrintPreviewScriptingMessage_(message))
      return;

    // Delay scripting messages from users of the scripting API until the
    // document is loaded. This simplifies use of the APIs.
    if (!this.loaded_) {
      this.delayedScriptingMessages_.push(message);
      return;
    }

    switch (message.data.type.toString()) {
      case 'getAccessibilityJSON':
      case 'getSelectedText':
      case 'print':
      case 'selectAll':
        this.plugin_.postMessage(message.data);
        break;
    }
  },

  /**
   * @private
   * Handle scripting messages specific to print preview.
   * @param {MessageObject} message the message to handle.
   * @return {boolean} true if the message was handled, false otherwise.
   */
  handlePrintPreviewScriptingMessage_: function(message) {
    if (!this.isPrintPreview_)
      return false;

    switch (message.data.type.toString()) {
      case 'loadPreviewPage':
        this.plugin_.postMessage(message.data);
        return true;
      case 'resetPrintPreviewMode':
        this.loaded_ = false;
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

        if (!this.isMaterial_)
          this.pageIndicator_.pageLabels = message.data.pageNumbers;

        this.plugin_.postMessage({
          type: 'resetPrintPreviewMode',
          url: message.data.url,
          grayscale: message.data.grayscale,
          // If the PDF isn't modifiable we send 0 as the page count so that no
          // blank placeholder pages get appended to the PDF.
          pageCount: (message.data.modifiable ?
                      message.data.pageNumbers.length : 0)
        });
        return true;
      case 'sendKeyEvent':
        this.handleKeyEvent_(DeserializeKeyEvent(message.data.keyEvent));
        return true;
    }

    return false;
  },

  /**
   * @private
   * Send a scripting message outside the extension (typically to
   * PDFScriptingAPI in a page containing the extension).
   * @param {Object} message the message to send.
   */
  sendScriptingMessage_: function(message) {
    if (this.parentWindow_)
      this.parentWindow_.postMessage(message, '*');
  },

  /**
   * @private
   * Return whether this PDFViewer should manage zoom for its containing page.
   * @return {boolean} Whether this PDFViewer should manage zoom for its
   *     containing page.
   */
  shouldManageZoom_: function() {
    return !!(chrome.tabs && !this.streamDetails_.embedded &&
              this.streamDetails_.tabId != -1);
  },

  /**
   * @type {Viewport} the viewport of the PDF viewer.
   */
  get viewport() {
    return this.viewport_;
  },

  /**
   * Each bookmark is an Object containing a:
   * - title
   * - page (optional)
   * - array of children (themselves bookmarks)
   * @type {Array} the top-level bookmarks of the PDF.
   */
  get bookmarks() {
    return this.bookmarks_;
  }
};
