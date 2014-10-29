// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * Clone a template within the extension error template collection.
   * @param {string} templateName The class name of the template to clone.
   * @return {HTMLElement} The clone of the template.
   */
  function cloneTemplate(templateName) {
    return /** @type {HTMLElement} */($('template-collection-extension-error').
        querySelector('.' + templateName).cloneNode(true));
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

    /**
     * @param {RuntimeError} error
     * @override
     */
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
      var viewDetailsLink = this.querySelector('.extension-error-view-details');

      // If we cannot open the file source and there are no external frames in
      // the stack, then there are no details to display.
      if (!extensions.ExtensionErrorOverlay.canShowOverlayForError(
              error, extensionUrl)) {
        viewDetailsLink.hidden = true;
      } else {
        var stringId = extensionUrl.toLowerCase() == 'manifest.json' ?
            'extensionErrorViewManifest' : 'extensionErrorViewDetails';
        viewDetailsLink.textContent = loadTimeData.getString(stringId);

        viewDetailsLink.addEventListener('click', function(e) {
          extensions.ExtensionErrorOverlay.getInstance().setErrorAndShowOverlay(
              error, extensionUrl);
        });
      }
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

  /**
   * @private
   * @const
   * @type {number}
   */
  ExtensionErrorList.MAX_ERRORS_TO_SHOW_ = 3;

  ExtensionErrorList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      this.contents_ = this.querySelector('.extension-error-list-contents');
      this.errors_.forEach(function(error) {
        if (idIsValid(error.extensionId)) {
          this.contents_.appendChild(document.createElement('li')).appendChild(
              new ExtensionError(error));
        }
      }, this);

      var numShowing = this.contents_.children.length;
      if (numShowing > ExtensionErrorList.MAX_ERRORS_TO_SHOW_)
        this.initShowMoreLink_();
    },

    /**
     * Initialize the "Show More" link for the error list. If there are more
     * than |MAX_ERRORS_TO_SHOW_| errors in the list.
     * @private
     */
    initShowMoreLink_: function() {
      var link = this.querySelector(
          '.extension-error-list-show-more [is="action-link"]');
      link.hidden = false;
      link.isShowingAll = false;

      var listContents = this.querySelector('.extension-error-list-contents');

      // TODO(dbeam/kalman): trade all this transition voodoo for .animate()?
      listContents.addEventListener('webkitTransitionEnd', function(e) {
        if (listContents.classList.contains('deactivating'))
          listContents.classList.remove('deactivating', 'active');
        else
          listContents.classList.add('scrollable');
      });

      link.addEventListener('click', function(e) {
        link.isShowingAll = !link.isShowingAll;

        var message = link.isShowingAll ? 'extensionErrorsShowFewer' :
                                          'extensionErrorsShowMore';
        link.textContent = loadTimeData.getString(message);

        // Disable scrolling while transitioning. If the element is active,
        // scrolling is enabled when the transition ends.
        listContents.classList.remove('scrollable');

        if (link.isShowingAll) {
          listContents.classList.add('active');
          listContents.classList.remove('deactivating');
        } else {
          listContents.classList.add('deactivating');
        }
      }.bind(this));
    }
  };

  return {
    ExtensionErrorList: ExtensionErrorList
  };
});
