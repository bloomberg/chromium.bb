// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

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

      const onSizeChanged = e => {
        this.extensionOptions_.style.height = e.height + 'px';
        this.extensionOptions_.style.width = e.width + 'px';

        if (!this.$$('dialog').open)
          this.$$('dialog').showModal();
      };

      this.extensionOptions_.onpreferredsizechanged = onSizeChanged;
      this.$.body.appendChild(this.extensionOptions_);
    },

    close: function() {
      this.$$('dialog').close();
      this.extensionOptions_.onpreferredsizechanged = null;
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
