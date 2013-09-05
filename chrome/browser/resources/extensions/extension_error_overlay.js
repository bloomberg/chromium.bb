// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * The ExtensionErrorOverlay will show the contents of a file which pertains
   * to the ExtensionError; this is either the manifest file (for manifest
   * errors) or a source file (for runtime errors). If possible, the portion
   * of the file which caused the error will be highlighted.
   * @constructor
   */
  function ExtensionErrorOverlay() {
  }

  cr.addSingletonGetter(ExtensionErrorOverlay);

  ExtensionErrorOverlay.prototype = {
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
    },

    /**
     * Handles a click on the dismiss button.
     * @param {Event} e The click event.
     * @private
     */
    handleDismiss_: function(e) {
      $('extension-error-overlay-content').innerHTML = '';
      extensions.ExtensionSettings.showOverlay(null);
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
    var content = $('extension-error-overlay-content');
    document.querySelector(
        '#extension-error-overlay .extension-error-overlay-title').
            innerText = result.title;

    var createSpan = function(source, isHighlighted) {
      var span = document.createElement('span');
      span.className = isHighlighted ? 'highlighted-source' : 'normal-source';
      source = source.replace(/ /g, '&nbsp;').replace(/\n|\r/g, '<br>');
      span.innerHTML = source;
      return span;
    };

    if (result.beforeHighlight)
      content.appendChild(createSpan(result.beforeHighlight, false));
    if (result.highlight) {
      var highlightSpan = createSpan(result.highlight, true);
      highlightSpan.title = result.message;
      content.appendChild(highlightSpan);
    }
    if (result.afterHighlight)
      content.appendChild(createSpan(result.afterHighlight, false));

    extensions.ExtensionSettings.showOverlay($('extension-error-overlay'));
  };

  // Export
  return {
    ExtensionErrorOverlay: ExtensionErrorOverlay
  };
});
