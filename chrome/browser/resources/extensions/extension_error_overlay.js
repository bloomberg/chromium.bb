// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * The RuntimeErrorContent manages all content specifically associated with
   * runtime errors; this includes stack frames and the context url.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function RuntimeErrorContent() {
    var contentArea = $('template-collection-extension-error-overlay').
        querySelector('.extension-error-overlay-runtime-content').
        cloneNode(true);
    contentArea.__proto__ = RuntimeErrorContent.prototype;
    contentArea.init();
    return contentArea;
  }

  /**
   * The name of the "active" class specific to extension errors (so as to
   * not conflict with other rules).
   * @type {string}
   * @const
   */
  RuntimeErrorContent.ACTIVE_CLASS_NAME = 'extension-error-active';

  /**
   * Determine whether or not we should display the url to the user. We don't
   * want to include any of our own code in stack traces.
   * @param {string} url The url in question.
   * @return {boolean} True if the url should be displayed, and false
   *     otherwise (i.e., if it is an internal script).
   */
  RuntimeErrorContent.shouldDisplayForUrl = function(url) {
    // All our internal scripts are in the 'extensions::' namespace.
    return !/^extensions::/.test(url);
  };

  RuntimeErrorContent.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * The underlying error whose details are being displayed.
     * @type {Object}
     * @private
     */
    error_: undefined,

    /**
     * The URL associated with this extension, i.e. chrome-extension://<id>.
     * @type {string}
     * @private
     */
    extensionUrl_: undefined,

    /**
     * The node of the stack trace which is currently active.
     * @type {HTMLElement}
     * @private
     */
    currentFrameNode_: undefined,

    /**
     * Initialize the RuntimeErrorContent for the first time.
     */
    init: function() {
      /**
       * The stack trace element in the overlay.
       * @type {HTMLElement}
       * @private
       */
      this.stackTrace_ =
          this.querySelector('.extension-error-overlay-stack-trace-list');
      assert(this.stackTrace_);

      /**
       * The context URL element in the overlay.
       * @type {HTMLElement}
       * @private
       */
      this.contextUrl_ =
          this.querySelector('.extension-error-overlay-context-url');
      assert(this.contextUrl_);
    },

    /**
     * Sets the error for the content.
     * @param {Object} error The error whose content should be displayed.
     */
    setError: function(error) {
      this.error_ = error;
      this.extensionUrl_ = 'chrome-extension://' + error.extensionId + '/';
      this.contextUrl_.textContent = error.contextUrl ?
          this.getRelativeUrl_(error.contextUrl) :
          loadTimeData.getString('extensionErrorOverlayContextUnknown');
      this.initStackTrace_();
    },

    /**
     * Wipe content associated with a specific error.
     */
    clearError: function() {
      this.error_ = undefined;
      this.extensionUrl_ = undefined;
      this.currentFrameNode_ = undefined;
      this.stackTrace_.innerHTML = '';
    },

    /**
     * Returns whether or not a given |url| is associated with the current
     * extension.
     * @param {string} url The url to examine.
     * @return {boolean} Whether or not the url is associated with the extension
     * @private
     */
    isRelatedUrl_: function(url) {
      return url.substring(0, this.extensionUrl_.length) == this.extensionUrl_;
    },

    /**
     * Get the url relative to the main extension url. If the url is
     * unassociated with the extension, this will be the full url.
     * @param {string} url The url to make relative.
     * @return {string} The url relative to the host.
     * @private
     */
    getRelativeUrl_: function(url) {
      return this.isRelatedUrl_(url, this.extensionUrl_) ?
          url.substring(this.extensionUrl_.length) : url;
    },

    /**
     * Makes |frame| active and deactivates the previously active frame (if
     * there was one).
     * @param {HTMLElement} frame The frame to activate.
     * @private
     */
    setActiveFrame_: function(frameNode) {
      if (this.currentFrameNode_) {
        this.currentFrameNode_.classList.remove(
            RuntimeErrorContent.ACTIVE_CLASS_NAME);
      }

      this.currentFrameNode_ = frameNode;
      this.currentFrameNode_.classList.add(
          RuntimeErrorContent.ACTIVE_CLASS_NAME);
    },

    /**
     * Initialize the stack trace element of the overlay.
     * @private
     */
    initStackTrace_: function() {
      for (var i = 0; i < this.error_.stackTrace.length; ++i) {
        var frame = this.error_.stackTrace[i];
        // Don't include any internal calls (e.g., schemaBindings) in the
        // stack trace.
        if (!RuntimeErrorContent.shouldDisplayForUrl(frame.url))
          continue;

        var frameNode = document.createElement('li');
        // Attach the index of the frame to which this node refers (since we
        // may skip some, this isn't a 1-to-1 match).
        frameNode.indexIntoTrace = i;

        // The description is a human-readable summation of the frame, in the
        // form "<relative_url>:<line_number> (function)", e.g.
        // "myfile.js:25 (myFunction)".
        var description =
            this.getRelativeUrl_(frame.url) + ':' + frame.lineNumber;
        if (frame.functionName) {
          var functionName = frame.functionName == '(anonymous function)' ?
              loadTimeData.getString('extensionErrorOverlayAnonymousFunction') :
              frame.functionName;
          description += ' (' + functionName + ')';
        }
        frameNode.textContent = description;

        // When the user clicks on a frame in the stack trace, we should
        // highlight that overlay in the list, display the appropriate source
        // code with the line highlighted, and link the "Open DevTools" button
        // with that frame.
        frameNode.addEventListener('click', function(frame, frameNode, e) {
          if (this.currStackFrame_ == frameNode)
            return;

          this.setActiveFrame_(frameNode);

          // Request the file source with the section highlighted; this will
          // call ExtensionErrorOverlay.requestFileSourceResponse() when
          // completed, which in turn calls setCode().
          chrome.send('extensionErrorRequestFileSource',
                      [{extensionId: this.error_.extensionId,
                        message: this.error_.message,
                        pathSuffix: this.getRelativeUrl_(frame.url),
                        lineNumber: frame.lineNumber}]);
        }.bind(this, frame, frameNode));

        this.stackTrace_.appendChild(frameNode);
      }

      // Set the current stack frame to the first stack frame.
      this.setActiveFrame_(this.stackTrace_.firstChild);
    },

    /**
     * Open the developer tools for the active stack frame.
     */
    openDevtools: function() {
      var stackFrame =
          this.error_.stackTrace[this.currentFrameNode_.indexIntoTrace];

      chrome.send('extensionErrorOpenDevTools',
                  [{renderProcessId: this.error_.renderProcessId,
                    renderViewId: this.error_.renderViewId,
                    url: stackFrame.url,
                    lineNumber: stackFrame.lineNumber || 0,
                    columnNumber: stackFrame.columnNumber || 0}]);
    }
  };

  /**
   * The ExtensionErrorOverlay will show the contents of a file which pertains
   * to the ExtensionError; this is either the manifest file (for manifest
   * errors) or a source file (for runtime errors). If possible, the portion
   * of the file which caused the error will be highlighted.
   * @constructor
   */
  function ExtensionErrorOverlay() {
    /**
     * The content section for runtime errors; this is re-used for all
     * runtime errors and attached/detached from the overlay as needed.
     * @type {RuntimeErrorContent}
     * @private
     */
    this.runtimeErrorContent_ = new RuntimeErrorContent();
  }

  /**
   * Value of ExtensionError::RUNTIME_ERROR enum.
   * @see extensions/browser/extension_error.h
   * @type {number}
   * @const
   * @private
   */
  ExtensionErrorOverlay.RUNTIME_ERROR_TYPE_ = 1;

  cr.addSingletonGetter(ExtensionErrorOverlay);

  ExtensionErrorOverlay.prototype = {
    /**
     * The underlying error whose details are being displayed.
     * @type {Object}
     * @private
     */
    error_: undefined,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      var overlay = $('overlay');
      cr.ui.overlay.setupOverlay(overlay);
      cr.ui.overlay.globalInitialization();
      overlay.addEventListener('cancelOverlay', this.handleDismiss_.bind(this));

      $('extension-error-overlay-dismiss').addEventListener(
          'click', this.handleDismiss_.bind(this));

      /**
       * The element of the full overlay.
       * @type {HTMLDivElement}
       * @private
       */
      this.overlayDiv_ = $('extension-error-overlay');

      /**
       * The portion of the overlay which shows the code relating to the error.
       * @type {HTMLElement}
       * @private
       */
      this.codeDiv_ = $('extension-error-overlay-code');

      /**
       * The button to open the developer tools (only available for runtime
       * errors).
       * @type {HTMLButtonElement}
       * @private
       */
      this.openDevtoolsButton_ = $('extension-error-overlay-devtools-button');
      this.openDevtoolsButton_.addEventListener('click', function() {
          this.runtimeErrorContent_.openDevtools();
      }.bind(this));
    },

    /**
     * Handles a click on the dismiss ("OK" or close) buttons.
     * @param {Event} e The click event.
     * @private
     */
    handleDismiss_: function(e) {
      extensions.ExtensionSettings.showOverlay(null);

      // There's a chance that the overlay receives multiple dismiss events; in
      // this case, handle it gracefully and return (since all necessary work
      // will already have been done).
      if (!this.error_)
        return;

      this.codeDiv_.innerHTML = '';
      this.openDevtoolsButton_.hidden = true;

      if (this.error_.type == ExtensionErrorOverlay.RUNTIME_ERROR_TYPE_) {
        this.overlayDiv_.querySelector('.content-area').removeChild(
            this.runtimeErrorContent_);
        this.runtimeErrorContent_.clearError();
      }

      this.error_ = undefined;
    },

    /**
     * Associate an error with the overlay. This will set the error for the
     * overlay, and, if possible, will populate the code section of the overlay
     * with the relevant file, load the stack trace, and generate links for
     * opening devtools (the latter two only happen for runtime errors).
     * @param {Object} error The error to show in the overlay.
     */
    setError: function(error) {
      this.error_ = error;

      if (this.error_.type == ExtensionErrorOverlay.RUNTIME_ERROR_TYPE_) {
        this.runtimeErrorContent_.setError(this.error_);
        this.overlayDiv_.querySelector('.content-area').insertBefore(
            this.runtimeErrorContent_,
            this.codeDiv_.nextSibling);
        this.openDevtoolsButton_.hidden = false;
        this.openDevtoolsButton_.disabled = !error.canInspect;
      }
    },

    /**
     * Set the code to be displayed in the code portion of the overlay.
     * See also ExtensionErrorOverlay.requestFileSourceResponse().
     * @param {Object} code The code to be displayed.
     */
    setCode: function(code) {
      document.querySelector(
          '#extension-error-overlay .extension-error-overlay-title').
              textContent = code.title;
      this.codeDiv_.innerHTML = '';

      var createSpan = function(source, isHighlighted) {
        var span = document.createElement('span');
        span.className = isHighlighted ? 'highlighted-source' : 'normal-source';
        source = source.replace(/ /g, '&nbsp;').replace(/\n|\r/g, '<br>');
        span.innerHTML = source;
        return span;
      };

      if (code.beforeHighlight)
        this.codeDiv_.appendChild(createSpan(code.beforeHighlight, false));

      if (code.highlight) {
        var highlightSpan = createSpan(code.highlight, true);
        highlightSpan.title = code.message;
        this.codeDiv_.appendChild(highlightSpan);
      }

      if (code.afterHighlight)
        this.codeDiv_.appendChild(createSpan(code.afterHighlight, false));
    },
  };

  /**
   * Called by the ExtensionErrorHandler responding to the request for a file's
   * source. Populate the content area of the overlay and display the overlay.
   * @param {Object} result An object with four strings - the title,
   *     beforeHighlight, afterHighlight, and highlight. The three 'highlight'
   *     strings represent three portions of the file's content to display - the
   *     portion which is most relevant and should be emphasized (highlight),
   *     and the parts both before and after this portion. These may be empty.
   */
  ExtensionErrorOverlay.requestFileSourceResponse = function(result) {
    extensions.ExtensionErrorOverlay.getInstance().setCode(result);
    extensions.ExtensionSettings.showOverlay($('extension-error-overlay'));
  };

  // Export
  return {
    ExtensionErrorOverlay: ExtensionErrorOverlay
  };
});
