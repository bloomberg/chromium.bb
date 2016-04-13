// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-startup-url-entry represents a UI component that
 * displayes a URL that is loaded during startup. It includes a menu that allows
 * the user to edit/remove the entry.
 */
Polymer({
  is: 'settings-startup-url-entry',

  properties: {
    /** @type {!StartupPageInfo} */
    model: Object,
  },

  /**
   * @param {string} url Location of an image to get a set of icons for.
   * @return {string} A set of icon URLs.
   * @private
   */
  getIconSet_: function(url) {
    return getFaviconImageSet(url);
  },

  /** @private */
  onRemoveTap_: function() {
    settings.StartupUrlsPageBrowserProxyImpl.getInstance().removeStartupPage(
        this.model.modelIndex);
  },
});
