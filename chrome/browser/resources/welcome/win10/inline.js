// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'welcome-win10-inline',

  properties: {
    // Determines if the combined variant should be displayed. The combined
    // variant includes instructions on how to pin Chrome to the taskbar.
    isCombined: Boolean
  },

  receivePinnedState: function(isPinnedToTaskbar) {
    this.isCombined = !isPinnedToTaskbar;
    // Allow overriding of the result via a query parameter.
    // TODO(pmonette): Remove these checks when they are no longer needed.
    /** @const */
    var VARIANT_KEY = 'variant';
    var VariantType = {
      DEFAULT_ONLY: 'defaultonly',
      COMBINED: 'combined'
    };
    var params = new URLSearchParams(location.search.slice(1));
    if (params.has(VARIANT_KEY)) {
      if (params.get(VARIANT_KEY) === VariantType.DEFAULT_ONLY)
        this.isCombined = false;
      else if (params.get(VARIANT_KEY) === VariantType.COMBINED)
        this.isCombined = true;
    }
  },

  ready: function() {
    this.isCombined = false;
    // Asynchronously check if Chrome is pinned to the taskbar.
    cr.sendWithPromise('getPinnedToTaskbarState').then(
      this.receivePinnedState.bind(this));
  },

  computeClasses: function(isCombined) {
    if (isCombined)
      return 'section expandable expanded';
    return 'section';
  },

  onContinue: function() {
    chrome.send('handleContinue');
  },

  onOpenSettings: function() {
    chrome.send('handleSetDefaultBrowser');
  },

  onToggle: function() {
    if (this.isCombined) {
      var sections = this.shadowRoot.querySelectorAll('.section.expandable');
      sections.forEach(function(section) {
        section.classList.toggle('expanded');
      });
    }
  }
});
