// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * Construct an ExtensionLoadError around the given |div|.
   * @param {HTMLDivElement} div The HTML div for the extension load error.
   * @constructor
   */
  function ExtensionLoadError(div) {
    div.__proto__ = ExtensionLoadError.prototype;
    div.init();
    return div;
  }

  /**
   * Construct a Failure.
   * @param {string} filePath The path to the unpacked extension.
   * @param {string} error The reason the extension failed to load.
   * @param {Object} manifest An object with three strings: beforeHighlight,
   *     afterHighlight, and highlight. These represent three portions of the
   *     file's content to display - the portion which is most relevant and
   *     should be emphasized (highlight), and the parts both before and after
   *     this portion. These may be empty.
   * @param {HTMLLIElement} listElement The HTML element used for displaying the
   *     failure path for the additional failures UI.
   * @constructor
   */
  function Failure(filePath, error, manifest, listElement) {
    this.path = filePath;
    this.error = error;
    this.manifest = manifest;
    this.listElement = listElement;
  }

  ExtensionLoadError.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Initialize the ExtensionLoadError div.
     */
    init: function() {
      /**
       * The element which displays the path of the extension.
       * @type {HTMLSpanElement}
       * @private
       */
      this.path_ = this.querySelector('#extension-load-error-path');

      /**
       * The element which displays the reason the extension failed to load.
       * @type {HTMLSpanElement}
       * @private
       */
      this.reason_ = this.querySelector('#extension-load-error-reason');

      /**
       * The element which displays the manifest code.
       * @type {ExtensionCode}
       * @private
       */
      this.manifest_ = new extensions.ExtensionCode(
          this.querySelector('#extension-load-error-manifest'));

      /**
       * The element which displays information about additional errors.
       * @type {HTMLULElement}
       * @private
       */
      this.additional_ = this.querySelector('#extension-load-error-additional');
      this.additional_.list = this.additional_.getElementsByTagName('ul')[0];

      /**
       * An array of Failures for keeping track of multiple active failures.
       * @type {Array.<Failure>}
       * @private
       */
      this.failures_ = [];

      this.querySelector('#extension-load-error-retry-button').addEventListener(
          'click', function(e) {
        chrome.send('extensionLoaderRetry');
        this.remove_();
      }.bind(this));

      this.querySelector('#extension-load-error-give-up-button').
          addEventListener('click', function(e) {
        chrome.send('extensionLoaderIgnoreFailure');
        this.remove_();
      }.bind(this));

      chrome.send('extensionLoaderDisplayFailures');
    },

    /**
     * Add a failure to failures_ array. If there is already a displayed
     * failure, display the additional failures element.
     * @param {Array.<Object>} failures Array of failures containing paths,
     *     errors, and manifests.
     * @private
     */
    add_: function(failures) {
      // If a failure is already being displayed, unhide the last item.
      if (this.failures_.length > 0)
        this.failures_[this.failures_.length - 1].listElement.hidden = false;
      failures.forEach(function(failure) {
        var listItem = document.createElement('li');
        listItem.textContent = failure.path;
        this.additional_.list.appendChild(listItem);
        this.failures_.push(new Failure(failure.path,
                                        failure.error,
                                        failure.manifest,
                                        listItem));
      }.bind(this));
      // Hide the last item because the UI is displaying its information.
      this.failures_[this.failures_.length - 1].listElement.hidden = true;
      this.show_();
    },

    /**
     * Remove a failure from |failures_| array. If this was the last failure,
     * hide the error UI.  If this was the last additional failure, hide
     * the additional failures UI.
     * @private
     */
    remove_: function() {
      this.additional_.list.removeChild(
          this.failures_[this.failures_.length - 1].listElement);
      this.failures_.pop();
      if (this.failures_.length > 0) {
        this.failures_[this.failures_.length - 1].listElement.hidden = true;
        this.show_();
      } else {
        this.hidden = true;
      }
    },

    /**
     * Display the load error to the user. The last failure gets its manifest
     * and error displayed, while additional failures have their path names
     * displayed in the additional failures element.
     * @private
     */
    show_: function() {
      assert(this.failures_.length >= 1);
      var failure = this.failures_[this.failures_.length - 1];
      this.path_.textContent = failure.path;
      this.reason_.textContent = failure.error;

      failure.manifest.message = failure.error;
      this.manifest_.populate(
          failure.manifest,
          loadTimeData.getString('extensionLoadCouldNotLoadManifest'));
      this.hidden = false;
      this.manifest_.scrollToError();

      this.additional_.hidden = this.failures_.length == 1;
    }
  };

  /**
   * The ExtensionLoader is the class in charge of loading unpacked extensions.
   * @constructor
   */
  function ExtensionLoader() {
    /**
     * The ExtensionLoadError to show any errors from loading an unpacked
     * extension.
     * @type {ExtensionLoadError}
     * @private
     */
    this.loadError_ = new ExtensionLoadError($('extension-load-error'));
  }

  cr.addSingletonGetter(ExtensionLoader);

  ExtensionLoader.prototype = {
    /**
     * Begin the sequence of loading an unpacked extension. If an error is
     * encountered, this object will get notified via notifyFailed().
     */
    loadUnpacked: function() {
      chrome.send('extensionLoaderLoadUnpacked');
    },

    /**
     * Notify the ExtensionLoader that loading an unpacked extension failed.
     * Add the failure to failures_ and show the ExtensionLoadError.
     * @param {Array.<Object>} failures Array of failures containing paths,
     *     errors, and manifests.
     */
    notifyFailed: function(failures) {
      this.loadError_.add_(failures);
    },
  };

  /*
   * A static forwarding function for ExtensionLoader.notifyFailed.
   * @param {Array.<Object>} failures Array of failures containing paths,
   *     errors, and manifests.
   * @see ExtensionLoader.notifyFailed
   */
  ExtensionLoader.notifyLoadFailed = function(failures) {
    ExtensionLoader.getInstance().notifyFailed(failures);
  };

  return {
    ExtensionLoader: ExtensionLoader
  };
});
