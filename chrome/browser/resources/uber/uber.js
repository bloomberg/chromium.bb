// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('uber', function() {

  /**
   * Handles page initialization.
   */
  function onLoad() {
    var navigationItems = document.querySelectorAll('#navigation li');
    var iframes = document.querySelectorAll('.iframe-container');

    for (var i = 0; i < navigationItems.length; ++i) {
      var navItem = navigationItems[i];
      navItem.associatedIframe = iframes[i];
      navItem.addEventListener('click', onNavItemClicked);
    }
  }

  function onNavItemClicked(e) {
    var iframe = e.currentTarget.associatedIframe;
    var currentIframe =
        document.querySelector('.iframe-container.selected');
    if (currentIframe != iframe) {
      currentIframe.classList.remove('selected');
      iframe.classList.add('selected');
    }

    var currentNavItem = document.querySelector('li.selected');
    if (currentNavItem != e.currentTarget) {
      currentNavItem.classList.remove('selected');
      e.currentTarget.classList.add('selected');
    }
  }

  return {
    onLoad: onLoad
  };
});

document.addEventListener('DOMContentLoaded', uber.onLoad);
