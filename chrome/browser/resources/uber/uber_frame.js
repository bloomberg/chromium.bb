// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the navigation controls that are visible on the left side
// of the uber page. It exists separately from uber.js so that it may be loaded
// in an iframe. Iframes can be layered on top of each other, but not mixed in
// with page content, so all overlapping content on uber must be framed.

// <include src="../../../../ui/webui/resources/js/util.js">
// <include src="uber_utils.js">

cr.define('uber_frame', function() {

  /**
   * Handles page initialization.
   */
  function onLoad() {
    var navigationItems = document.querySelectorAll('li');

    for (var i = 0; i < navigationItems.length; ++i) {
      navigationItems[i].addEventListener('click', onNavItemClicked);
    }

    cr.ui.FocusOutlineManager.forDocument(this);

    window.addEventListener('message', handleWindowMessage);
    uber.invokeMethodOnParent('navigationControlsLoaded');

    document.documentElement.addEventListener('mousewheel', onMouseWheel);
    document.documentElement.addEventListener('mousedown', onMouseDown);
  }

  /**
   * Handles clicks on the navigation controls (switches the page and updates
   * the URL).
   * @param {Event} e The click event.
   */
  function onNavItemClicked(e) {
    // Though pointer-event: none; is applied to the .selected nav item, users
    // can still tab to them and press enter/space which simulates a click.
    if (e.target.classList.contains('selected'))
      return;

    uber.invokeMethodOnParent('showPage',
       {pageId: e.currentTarget.getAttribute('controls')});

    setSelection(/** @type {Element} */(e.currentTarget));
  }

  /**
   * Handles postMessage from chrome://chrome.
   * @param {Event} e The post data.
   */
  function handleWindowMessage(e) {
    if (e.data.method === 'changeSelection')
      changeSelection(e.data.params);
    else if (e.data.method === 'adjustToScroll')
      adjustToScroll(e.data.params);
    else if (e.data.method === 'setContentChanging')
      setContentChanging(e.data.params);
    else
      console.error('Received unexpected message', e.data);
  }

  /**
   * Changes the selected nav control.
   * @param {Object} params Must contain pageId.
   */
  function changeSelection(params) {
    var navItem =
        document.querySelector('li[controls="' + params.pageId + '"]');
    setSelection(navItem);
    showNavItems();
  }

  /**
   * @return {Element} The currently selected nav item, if any.
   */
  function getSelectedNavItem() {
    return document.querySelector('li.selected');
  }

  /**
   * Sets selection on the given nav item.
   * @param {Element} newSelection The item to be selected.
   */
  function setSelection(newSelection) {
    var items = document.querySelectorAll('li');
    for (var i = 0; i < items.length; ++i) {
      items[i].classList.toggle('selected', items[i] == newSelection);
      items[i].setAttribute('aria-selected', items[i] == newSelection);
    }
  }

  /**
   * Shows nav items belonging to the same group as the selected item.
   */
  function showNavItems() {
    var hideSettingsAndHelp = loadTimeData.getBoolean('hideSettingsAndHelp');
    $('settings').hidden = hideSettingsAndHelp;
    $('help').hidden = hideSettingsAndHelp;
    $('extensions').hidden = loadTimeData.getBoolean('hideExtensions');
  }

  /**
   * Adjusts this frame's content to scrolls from the outer frame. This is done
   * to obscure text in RTL as a user scrolls over the content of this frame (as
   * currently RTL scrollbars still draw on the right).
   * @param {number} scrollLeft document.body.scrollLeft of the content frame.
   */
  function adjustToScroll(scrollLeft) {
    assert(isRTL());
    document.body.style.transform = 'translateX(' + -scrollLeft + 'px)';
  }

  /**
   * Enable/disable an animation to ease the nav bar back into view when
   * changing content while horizontally scrolled.
   * @param {boolean} enabled Whether easing should be enabled.
   */
  function setContentChanging(enabled) {
    assert(isRTL());
    document.documentElement.classList.toggle('changing-content', enabled);
  }

  /**
   * Handles mouse wheels on the top level element. Forwards them to uber.js.
   * @param {Event} e The mouse wheel event.
   */
  function onMouseWheel(e) {
    uber.invokeMethodOnParent('mouseWheel',
        {deltaX: e.wheelDeltaX, deltaY: e.wheelDeltaY});
  }

  /**
   * Handles mouse presses on the top level element. Forwards them to uber.js.
   * @param {Event} e The mouse down event.
   */
  function onMouseDown(e) {
    uber.invokeMethodOnParent('mouseDown');
  }

  /**
   * @return {Element} The currently selected iframe container.
   * @private
   */
  function getSelectedIframe() {
    return document.querySelector('.iframe-container.selected');
  }

  return {onLoad: onLoad};
});

document.addEventListener('DOMContentLoaded', uber_frame.onLoad);
