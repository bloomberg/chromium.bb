// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * @return {!Promise} A signal that the document is ready. Need to wait for
   *     this, otherwise the custom ExtensionOptions element might not have been
   *     registered yet.
   */
  function whenDocumentReady() {
    if (document.readyState == 'complete')
      return Promise.resolve();

    return new Promise(function(resolve) {
      document.addEventListener('readystatechange', function f() {
        if (document.readyState == 'complete') {
          document.removeEventListener('readystatechange', f);
          resolve();
        }
      });
    });
  }

  const OptionsDialog = Polymer({
    is: 'extensions-options-dialog',

    behaviors: [extensions.ItemBehavior],

    properties: {
      /** @private {Object} */
      extensionOptions_: Object,

      /** @private {chrome.developerPrivate.ExtensionInfo} */
      data_: Object,
    },

    /** @private {?Function} */
    boundResizeListener_: null,

    get open() {
      return this.$$('dialog').open;
    },

    /**
     * Resizes the dialog to the given width/height, taking into account the
     * window width/height.
     * @param {number} width
     * @param {number} height
     * @private
     */
    updateDialogSize_: function(width, height) {
      const effectiveHeight = Math.min(window.innerHeight, height);
      const effectiveWidth = Math.min(window.innerWidth, width);
      this.$.dialog.style.height = effectiveHeight + 'px';
      this.$.dialog.style.width = effectiveWidth + 'px';
    },

    /** @param {chrome.developerPrivate.ExtensionInfo} data */
    show: function(data) {
      this.data_ = data;
      whenDocumentReady().then(() => {
        if (!this.extensionOptions_)
          this.extensionOptions_ = document.createElement('ExtensionOptions');
        this.extensionOptions_.extension = this.data_.id;
        this.extensionOptions_.onclose = this.close.bind(this);

        let preferredSize = null;
        const onSizeChanged = e => {
          preferredSize = e;
          this.updateDialogSize_(preferredSize.width, preferredSize.height);
          this.$$('dialog').showModal();
          this.extensionOptions_.onpreferredsizechanged = null;
        };

        this.boundResizeListener_ = () => {
          this.updateDialogSize_(preferredSize.width, preferredSize.height);
        };

        // Only handle the 1st instance of 'preferredsizechanged' event, to get
        // the preferred size.
        this.extensionOptions_.onpreferredsizechanged = onSizeChanged;

        // Add a 'resize' such that the dialog is resized when window size
        // changes.
        window.addEventListener('resize', this.boundResizeListener_);
        this.$.body.appendChild(this.extensionOptions_);
      });
    },

    close: function() {
      this.$$('dialog').close();
      this.extensionOptions_.onpreferredsizechanged = null;
    },

    /** @private */
    onClose_: function() {
      if (this.boundResizeListener_) {
        window.removeEventListener('resize', this.boundResizeListener_);
        this.boundResizeListener_ = null;
      }

      const currentPage = extensions.navigation.getCurrentPage();
      // We update the page when the options dialog closes, but only if we're
      // still on the details page. We could be on a different page if the
      // user hit back while the options dialog was visible; in that case, the
      // new page is already correct.
      if (currentPage && currentPage.page == Page.DETAILS) {
        // This will update the currentPage_ and the NavigationHelper; since
        // the active page is already the details page, no main page
        // transition occurs.
        extensions.navigation.navigateTo(
            {page: Page.DETAILS, extensionId: currentPage.extensionId});
      }
    },
  });

  return {OptionsDialog: OptionsDialog};
});
