// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * Returns whether or not a given |url| is associated with an extension.
   * @param {string} url The url to examine.
   * @param {string} extensionUrl The url of the extension.
   * @return {boolean} Whether or not the url is associated with the extension.
   */
  function isExtensionUrl(url, extensionUrl) {
    return url.substring(0, extensionUrl.length) == extensionUrl;
  }

  /**
   * Get the url relative to the main extension url. If the url is
   * unassociated with the extension, this will be the full url.
   * @param {string} url The url to make relative.
   * @param {string} extensionUrl The host for which the url is relative.
   * @return {string} The url relative to the host.
   */
  function getRelativeUrl(url, extensionUrl) {
    return isExtensionUrl(url, extensionUrl) ?
        url.substring(extensionUrl.length) : url;
  }

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
   * Creates a new ExtensionError HTMLElement; this is used to show a
   * notification to the user when an error is caused by an extension.
   * @param {Object} error The error the element should represent.
   * @param {string} templateName The name of the template to clone for the
   *     error ('extension-error-[detailed|simple]-wrapper').
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionError(error, templateName) {
    var div = cloneTemplate(templateName);
    div.__proto__ = ExtensionError.prototype;
    div.error_ = error;
    div.decorate();
    return div;
  }

  ExtensionError.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      var metadata = cloneTemplate('extension-error-metadata');

      // Add an additional class for the severity level.
      if (this.error_.level == 0)
        metadata.className += ' extension-error-severity-info';
      else if (this.error_.level == 1)
        metadata.className += ' extension-error-severity-warning';
      else
        metadata.className += ' extension-error-severity-fatal';

      var iconNode = document.createElement('img');
      iconNode.className = 'extension-error-icon';
      metadata.insertBefore(iconNode, metadata.firstChild);

      // Add a property for the extension's base url in order to determine if
      // a url belongs to the extension.
      this.extensionUrl_ =
          'chrome-extension://' + this.error_.extensionId + '/';

      metadata.querySelector('.extension-error-message').textContent =
          this.error_.message;

      metadata.appendChild(this.getViewSourceOrPlain_(
          getRelativeUrl(this.error_.source, this.extensionUrl_),
          this.error_.source));

      // The error template may specify a <summary> to put template metadata in.
      // If not, just append it to the top-level element.
      var metadataContainer = this.querySelector('summary') || this;
      metadataContainer.appendChild(metadata);

      var detailsNode = this.querySelector('.extension-error-details');
      if (detailsNode && this.error_.contextUrl)
        detailsNode.appendChild(this.getContextNode_());
      if (detailsNode && this.error_.stackTrace)
        detailsNode.appendChild(this.getStackNode_());
    },

    /**
     * Return a div with text |description|. If it's possible to view the source
     * for |url|, linkify the div to do so.
     * @param {string} description a human-friendly description the location
     *     (e.g., filename, line).
     * @param {string} url The url of the resource to view.
     * @param {?number} line An optional line number of the resource.
     * @return {HTMLElement} The created node, either a link or plaintext.
     * @private
     */
    getViewSourceOrPlain_: function(description, url, line) {
      if (this.canViewSource_(url))
        var node = this.getViewSourceLink_(url, line);
      else
        var node = document.createElement('div');
      node.className = 'extension-error-view-source';
      node.textContent = description;
      return node;
    },

    /**
     * Determine whether we can view the source of a given url.
     * @param {string} url The url of the resource to view.
     * @return {boolean} Whether or not we can view the source for the url.
     * @private
     */
    canViewSource_: function(url) {
      return isExtensionUrl(url, this.extensionUrl_) || url == 'manifest.json';
    },

    /**
     * Create a clickable node to view the source for the given url.
     * @param {string} url The url to the resource to view.
     * @param {?number} line An optional line number of the resource (for
     *     source files).
     * @return {HTMLElement} The clickable node to view the source.
     * @private
     */
    getViewSourceLink_: function(url, line) {
      var node = document.createElement('a');
      var relativeUrl = getRelativeUrl(url, this.extensionUrl_);
      var requestFileSourceArgs = { 'extensionId': this.error_.extensionId,
                                    'message': this.error_.message,
                                    'pathSuffix': relativeUrl };
      if (relativeUrl == 'manifest.json') {
        requestFileSourceArgs.manifestKey = this.error_.manifestKey;
        requestFileSourceArgs.manifestSpecific = this.error_.manifestSpecific;
      } else {
        // Prefer |line| if available, or default to the line of the last stack
        // frame.
        if (line) {
          requestFileSourceArgs.lineNumber = line;
        } else if (this.error_.stackTrace) {
          requestFileSourceArgs.lineNumber =
              this.error_.stackTrace[0].lineNumber;
        }
      }

      node.addEventListener('click', function(e) {
        chrome.send('extensionErrorRequestFileSource', [requestFileSourceArgs]);
      });
      node.title = loadTimeData.getString('extensionErrorViewSource');
      return node;
    },

    /**
     * Get the context node for this error. This will attempt to link to the
     * context in which the error occurred, and can be either an extension page
     * or an external page.
     * @return {HTMLDivElement} The context node for the error, including the
     *     label and a link to the context.
     * @private
     */
    getContextNode_: function() {
      var node = cloneTemplate('extension-error-context-wrapper');
      var linkNode = node.querySelector('a');
      if (isExtensionUrl(this.error_.contextUrl, this.extensionUrl_)) {
        linkNode.textContent = getRelativeUrl(this.error_.contextUrl,
                                              this.extensionUrl_);
      } else {
        linkNode.textContent = this.error_.contextUrl;
      }
      linkNode.href = this.error_.contextUrl;
      linkNode.target = '_blank';
      return node;
    },

    /**
     * Get a node for the stack trace for this error. Each stack frame will
     * include a resource url, line number, and function name (possibly
     * anonymous). If possible, these frames will also be linked for viewing the
     * source.
     * @return {HTMLDetailsElement} The stack trace node for this error, with
     *     all stack frames nested in a details-summary object.
     * @private
     */
    getStackNode_: function() {
      var node = cloneTemplate('extension-error-stack-trace');
      var listNode = node.querySelector('.extension-error-stack-trace-list');
      this.error_.stackTrace.forEach(function(frame) {
        var frameNode = document.createElement('div');
        var description = getRelativeUrl(frame.url, this.extensionUrl_) +
                          ':' + frame.lineNumber;
        if (frame.functionName) {
          var functionName = frame.functionName == '(anonymous function)' ?
              loadTimeData.getString('extensionErrorAnonymousFunction') :
              frame.functionName;
          description += ' (' + functionName + ')';
        }
        frameNode.appendChild(this.getViewSourceOrPlain_(
            description, frame.url, frame.lineNumber));
        listNode.appendChild(
            document.createElement('li')).appendChild(frameNode);
      }, this);

      return node;
    },
  };

  /**
   * A variable length list of runtime or manifest errors for a given extension.
   * @param {Array.<Object>} errors The list of extension errors with which
   *     to populate the list.
   * @param {string} title The i18n key for the title of the error list, i.e.
   *     'extensionErrors[Manifest,Runtime]Errors'.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionErrorList(errors, title) {
    var div = cloneTemplate('extension-error-list');
    div.__proto__ = ExtensionErrorList.prototype;
    div.errors_ = errors;
    div.title_ = title;
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
      this.querySelector('.extension-error-list-title').textContent =
          loadTimeData.getString(this.title_);

      this.contents_ = this.querySelector('.extension-error-list-contents');
      this.errors_.forEach(function(error) {
        this.contents_.appendChild(document.createElement('li')).appendChild(
            new ExtensionError(error,
                               error.contextUrl || error.stackTrace ?
                                   'extension-error-detailed-wrapper' :
                                   'extension-error-simple-wrapper'));
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
