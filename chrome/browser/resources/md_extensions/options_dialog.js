// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  const MAX_HEIGHT = 600;
  const MAX_WIDTH = 600;
  const MIN_HEIGHT = 300;
  const MIN_WIDTH = 300;
  const HEADER_EXTRA_SPACING = 50;  // 40 from x-button + 10 from img margin.
  const DIALOG_PADDING = 32;        // Padding from cr-dialog's .body styling.

  const OptionsDialog = Polymer({
    is: 'extensions-options-dialog',

    behaviors: [extensions.ItemBehavior],

    properties: {
      /** @private {Object} */
      extensionOptions_: Object,

      /** @private {chrome.developerPrivate.ExtensionInfo} */
      data_: Object,
    },

    get open() {
      return this.$$('dialog').open;
    },

    /** @param {chrome.developerPrivate.ExtensionInfo} data */
    show: function(data) {
      this.data_ = data;
      if (!this.extensionOptions_)
        this.extensionOptions_ = document.createElement('ExtensionOptions');
      this.extensionOptions_.extension = this.data_.id;
      this.extensionOptions_.onclose = this.close.bind(this);
      const bounded = function(min, max, val) {
        return Math.min(Math.max(min, val), max);
      };

      const onSizeChanged = e => {
        const minHeaderWidth =
            this.$$('#icon-and-name-wrapper img').offsetWidth +
            this.$$('#icon-and-name-wrapper span').offsetWidth +
            HEADER_EXTRA_SPACING;
        const minWidth = Math.max(minHeaderWidth, MIN_WIDTH);
        this.extensionOptions_.style.height =
            bounded(MIN_HEIGHT, MAX_HEIGHT, e.height) + 'px';
        this.extensionOptions_.style.width =
            bounded(minWidth, MAX_WIDTH, e.width) + 'px';
        this.$.dialog.style.width =
            (bounded(minWidth, MAX_WIDTH, e.width) + DIALOG_PADDING) + 'px';
      };

      this.extensionOptions_.onpreferredsizechanged = onSizeChanged;
      this.$.body.appendChild(this.extensionOptions_);
      this.$$('dialog').showModal();
      onSizeChanged({height: 0, width: 0});
    },

    close: function() {
      this.$$('dialog').close();
    },

    /** @private */
    onClose_: function() {
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
