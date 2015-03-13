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
   * @param {(RuntimeError|ManifestError)} error The error the element should
   *     represent.
   * @param {Element} boundary The boundary for the focus grid.
   * @constructor
   * @extends {cr.ui.FocusRow}
   */
  function ExtensionError(error, boundary) {
    var div = cloneTemplate('extension-error-metadata');
    div.__proto__ = ExtensionError.prototype;
    div.decorate(error, boundary);
    return div;
  }

  ExtensionError.prototype = {
    __proto__: cr.ui.FocusRow.prototype,

    /** @override */
    getEquivalentElement: function(element) {
      return assert(this.querySelector('.extension-error-view-details'));
    },

    /**
     * @param {(RuntimeError|ManifestError)} error The error the element should
     *     represent
     * @param {Element} boundary The boundary for the FocusGrid.
     * @override
     */
    decorate: function(error, boundary) {
      cr.ui.FocusRow.prototype.decorate.call(this, boundary);

      // Add an additional class for the severity level.
      if (error.type == chrome.developerPrivate.ErrorType.RUNTIME) {
        switch (error.severity) {
          case chrome.developerPrivate.ErrorLevel.LOG:
            this.classList.add('extension-error-severity-info');
            break;
          case chrome.developerPrivate.ErrorLevel.WARN:
            this.classList.add('extension-error-severity-warning');
            break;
          case chrome.developerPrivate.ErrorLevel.ERROR:
            this.classList.add('extension-error-severity-fatal');
            break;
          default:
            assertNotReached();
        }
      } else {
        // We classify manifest errors as "warnings".
        this.classList.add('extension-error-severity-warning');
      }

      var iconNode = document.createElement('img');
      iconNode.className = 'extension-error-icon';
      // TODO(hcarmona): Populate alt text with a proper description since this
      // icon conveys the severity of the error. (info, warning, fatal).
      iconNode.alt = '';
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

        this.addFocusableElement(viewDetailsLink);
      }
    },
  };

  /**
   * A variable length list of runtime or manifest errors for a given extension.
   * @param {Array<(RuntimeError|ManifestError)>} errors The list of extension
   *     errors with which to populate the list.
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

    decorate: function() {
      this.focusGrid_ = new cr.ui.FocusGrid();
      this.gridBoundary_ = this.querySelector('.extension-error-list-contents');
      this.gridBoundary_.addEventListener('focus', this.onFocus_.bind(this));
      this.gridBoundary_.addEventListener('focusin',
                                          this.onFocusin_.bind(this));
      this.errors_.forEach(function(error) {
        if (idIsValid(error.extensionId)) {
          var focusRow = new ExtensionError(error, this.gridBoundary_);
          this.gridBoundary_.appendChild(
              document.createElement('li')).appendChild(focusRow);
          this.focusGrid_.addRow(focusRow);
        }
      }, this);

      var numShowing = this.focusGrid_.rows.length;
      if (numShowing > ExtensionErrorList.MAX_ERRORS_TO_SHOW_)
        this.initShowMoreLink_();
    },

    /**
     * @return {?Element} The element that toggles between show more and show
     *     less, or null if it's hidden. Button will be hidden if there are less
     *     errors than |MAX_ERRORS_TO_SHOW_|.
     */
    getToggleElement: function() {
      return this.querySelector(
          '.extension-error-list-show-more [is="action-link"]:not([hidden])');
    },

    /** @return {!Element} The element containing the list of errors. */
    getErrorListElement: function() {
      return this.gridBoundary_;
    },

    /**
     * The grid should not be focusable once it or an element inside it is
     * focused. This is necessary to allow tabbing out of the grid in reverse.
     * @private
     */
    onFocusin_: function() {
      this.gridBoundary_.tabIndex = -1;
    },

    /**
     * Focus the first focusable row when tabbing into the grid for the
     * first time.
     * @private
     */
    onFocus_: function() {
      var activeRow = this.gridBoundary_.querySelector('.focus-row-active');
      var toggleButton = this.getToggleElement();

      if (toggleButton && !toggleButton.isShowingAll) {
        var rows = this.focusGrid_.rows;
        assert(rows.length > ExtensionErrorList.MAX_ERRORS_TO_SHOW_);

        var firstVisible = rows.length - ExtensionErrorList.MAX_ERRORS_TO_SHOW_;
        if (rows.indexOf(activeRow) < firstVisible)
          activeRow = rows[firstVisible];
      } else if (!activeRow) {
        activeRow = this.focusGrid_.rows[0];
      }

      activeRow.getEquivalentElement(null).focus();
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
        // Needs to be enabled in case the focused row is now hidden.
        this.gridBoundary_.tabIndex = 0;

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
