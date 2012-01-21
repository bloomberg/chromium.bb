// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the navigation controls that are visible on the left side
// of the uber page. It exists separately from uber.js so that it may be loaded
// in an iframe. Iframes can be layered on top of each other, but not mixed in
// with page content, so all overlapping content on uber must be framed.

<include src="uber_utils.js"></include>

cr.define('uber_frame', function() {

  /**
   * Handles page initialization.
   */
  function onLoad() {
    var navigationItems = document.querySelectorAll('li');

    for (var i = 0; i < navigationItems.length; ++i) {
      navigationItems[i].addEventListener('click', onNavItemClicked);
    }

    window.addEventListener('message', handleWindowMessage);
    uber.invokeMethodOnParent('navigationControlsLoaded');
  }

  /**
   * Handles clicks on the navigation controls (switches the page and updates
   * the URL).
   * @param {Event} e The click event.
   */
  function onNavItemClicked(e) {
    uber.invokeMethodOnParent('showPage',
       {pageId: e.currentTarget.getAttribute('controls')});

    setSelection(e.currentTarget);
  }

  /**
   * Handles postMessage from chrome://chrome.
   * @param {Event} e The post data.
   */
  function handleWindowMessage(e) {
    if (e.data.method === 'changeSelection')
      changeSelection(e.data.params);
    else
      console.error('Received unexpected message: ' + e.data);
  }

  /**
   * Changes the selected nav control.
   * @param {Object} params Must contain pageId.
   */
  function changeSelection(params) {
    var navItem =
        document.querySelector('li[controls="' + params.pageId + '"]');
    setSelection(navItem);
  }

  /**
   * Sets selection on the given nav item.
   * @param {Boolean} newSelection The item to be selected.
   */
  function setSelection(newSelection) {
    var lastSelectedNavItem = document.querySelector('li.selected');
    if (lastSelectedNavItem !== newSelection) {
      newSelection.classList.add('selected');
      lastSelectedNavItem.classList.remove('selected');
    }
  }

  /**
   * @return {Object} The currently selected iframe container.
   * @private
   */
  function getSelectedIframe() {
    return document.querySelector('.iframe-container.selected');
  }

  return {
    onLoad: onLoad,
  };

});

document.addEventListener('DOMContentLoaded', uber_frame.onLoad);
