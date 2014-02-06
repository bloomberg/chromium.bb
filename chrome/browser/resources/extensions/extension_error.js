// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="extension_error_overlay.js"></include>

cr.define('extensions', function() {
  'use strict';

  /**
   * Clone a template within the extension error template collection.
   * @param {string} templateName The class name of the template to clone.
   * @return {HTMLElement} The clone of the template.
   */
  function cloneTemplate(templateName) {
    return $('template-collection-extension-error').
        querySelector('.' + templateName).cloneNode(true);
  }

  /**
   * Checks that an Extension ID follows the proper format (i.e., is 32
   * characters long, is lowercase, and contains letters in the range [a, p]).
   * @param {string} id The Extension ID to test.
   * @return {boolean} Whether or not the ID is valid.
   */
  function idIsValid(id) {
    return /^[a-p]{32}$/.test(id);
  }

  /**
   * Creates a new ExtensionError HTMLElement; this is used to show a
   * notification to the user when an error is caused by an extension.
   * @param {Object} error The error the element should represent.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionError(error) {
    var div = cloneTemplate('extension-error-metadata');
    div.__proto__ = ExtensionError.prototype;
    div.decorate(error);
    return div;
  }

  ExtensionError.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function(error) {
      // Add an additional class for the severity level.
      if (error.level == 0)
        this.classList.add('extension-error-severity-info');
      else if (error.level == 1)
        this.classList.add('extension-error-severity-warning');
      else
        this.classList.add('extension-error-severity-fatal');

      var iconNode = document.createElement('img');
      iconNode.className = 'extension-error-icon';
      this.insertBefore(iconNode, this.firstChild);

      var messageSpan = this.querySelector('.extension-error-message');
      messageSpan.textContent = error.message;
      messageSpan.title = error.message;

      var extensionUrl = 'chrome-extension://' + error.extensionId + '/';

      // The relative url is the url without the preceeding
      // "chrome-extension://<id>"; this is the format which the
      // requestFileSource call expects.
      var relativeUrl =
          error.source.substring(0, extensionUrl.length) == extensionUrl ?
              error.source.substring(extensionUrl.length) : error.source;

      var requestFileSourceArgs = {extensionId: error.extensionId,
                                   message: error.message,
                                   pathSuffix: relativeUrl};

      var viewDetailsLink = this.querySelector('.extension-error-view-details');
      var viewDetailsStringId;
      if (relativeUrl.toLowerCase() == 'manifest.json') {
        requestFileSourceArgs.manifestKey = error.manifestKey;
        requestFileSourceArgs.manifestSpecific = error.manifestSpecific;
        viewDetailsStringId = 'extensionErrorViewManifest';
      } else {
        requestFileSourceArgs.lineNumber =
            error.stackTrace && error.stackTrace[0] ?
                error.stackTrace[0].lineNumber : 0;
        viewDetailsStringId = 'extensionErrorViewDetails';
      }
      viewDetailsLink.textContent = loadTimeData.getString(viewDetailsStringId);

      viewDetailsLink.addEventListener('click', function(e) {
        extensions.ExtensionErrorOverlay.getInstance().setError(error);
        chrome.send('extensionErrorRequestFileSource', [requestFileSourceArgs]);
      }.bind(this));
    },
  };

  /**
   * A variable length list of runtime or manifest errors for a given extension.
   * @param {Array.<Object>} errors The list of extension errors with which
   *     to populate the list.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionErrorList(errors) {
    var div = cloneTemplate('extension-error-list');
    div.__proto__ = ExtensionErrorList.prototype;
    div.errors_ = errors;
    div.decorate();
    return div;
  }

  ExtensionErrorList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * @private
     * @const
     * @type {number}
     */
    MAX_ERRORS_TO_SHOW_: 3,

    /** @override */
    decorate: function() {
      this.contents_ = this.querySelector('.extension-error-list-contents');
      this.errors_.forEach(function(error) {
        if (idIsValid(error.extensionId)) {
          this.contents_.appendChild(document.createElement('li')).appendChild(
              new ExtensionError(error));
        }
      }, this);

      if (this.contents_.children.length > this.MAX_ERRORS_TO_SHOW_) {
        for (var i = this.MAX_ERRORS_TO_SHOW_;
             i < this.contents_.children.length; ++i) {
          this.contents_.children[i].hidden = true;
        }
        this.initShowMoreButton_();
      }
    },

    /**
     * Initialize the "Show More" button for the error list. If there are more
     * than |MAX_ERRORS_TO_SHOW_| errors in the list.
     * @private
     */
    initShowMoreButton_: function() {
      var button = this.querySelector('.extension-error-list-show-more a');
      button.hidden = false;
      button.isShowingAll = false;
      button.addEventListener('click', function(e) {
        for (var i = this.MAX_ERRORS_TO_SHOW_;
             i < this.contents_.children.length; ++i) {
          this.contents_.children[i].hidden = button.isShowingAll;
        }
        var message = button.isShowingAll ? 'extensionErrorsShowMore' :
                                            'extensionErrorsShowFewer';
        button.textContent = loadTimeData.getString(message);
        button.isShowingAll = !button.isShowingAll;
      }.bind(this));
    }
  };

  return {
    ExtensionErrorList: ExtensionErrorList
  };
});
