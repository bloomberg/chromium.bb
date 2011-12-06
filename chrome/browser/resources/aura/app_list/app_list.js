// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App list UI.
 * For now, most of its logic is borrowed from ntp4.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome
cr.define('appList', function() {
  'use strict';

  /**
   * AppsView instance.
   * @type {!Object|undefined}
   */
  var appsView;

  /**
   * Invoked at startup once the DOM is available to initialize the app.
   */
  function load() {
    appsView = new appList.AppsView();

    document.addEventListener('click', onDocClick);
  }

  /**
   * Document click event handler.
   */
  function onDocClick(e) {
    // Close if click is on body, or not on app, paging dot or its children.
    if (e.target == document.body ||
        (!e.target.classList.contains('app') &&
         !e.target.classList.contains('dot') &&
         !findAncestorByClass(e.target, 'dot'))) {
      chrome.send('close');
    }
  }

  /**
   * Wrappers to forward the callback to corresponding PageListView member.
   */
  function appAdded(appData, opt_highlight) {
    appsView.appAdded(appData, opt_highlight);
  }

  function appRemoved(appData, isUninstall) {
    appsView.appRemoved(appData, isUninstall);
  }

  function appsPrefChangeCallback(data) {
    appsView.appsPrefChangedCallback(data);
  }

  function enterRearrangeMode() {
    appsView.enterRearrangeMode();
  }

  function getAppsCallback(data) {
    appsView.getAppsCallback(data);
    chrome.send('onAppsLoaded');
  }

  function getAppsPageIndex(page) {
    return appsView.getAppsPageIndex(page);
  }

  function getCardSlider() {
    return appsView.cardSlider;
  }

  function leaveRearrangeMode(e) {
    appsView.leaveRearrangeMode(e);
  }

  function saveAppPageName(appPage, name) {
    appsView.saveAppPageName(appPage, name);
  }

  function setAppToBeHighlighted(appId) {
    appsView.highlightAppId = appId;
  }

  // Return an object with all the exports
  return {
    appAdded: appAdded,
    appRemoved: appRemoved,
    appsPrefChangeCallback: appsPrefChangeCallback,
    enterRearrangeMode: enterRearrangeMode,
    getAppsPageIndex: getAppsPageIndex,
    getAppsCallback: getAppsCallback,
    getCardSlider: getCardSlider,
    leaveRearrangeMode: leaveRearrangeMode,
    load: load,
    saveAppPageName: saveAppPageName,
    setAppToBeHighlighted: setAppToBeHighlighted
  };
});

// publish ntp globals
// TODO(xiyuan): update the following when content handlers are changed to use
// ntp namespace.
var appsPrefChangeCallback = appList.appsPrefChangeCallback;
var getAppsCallback = appList.getAppsCallback;

var ntp4 = ntp4 || {};
ntp4.appAdded = appList.appAdded;
ntp4.appRemoved = appList.appRemoved;
ntp4.enterRearrangeMode = appList.enterRearrangeMode;
ntp4.getAppsPageIndex = appList.getAppsPageIndex;
ntp4.getCardSlider = appList.getCardSlider;
ntp4.leaveRearrangeMode = appList.leaveRearrangeMode;
ntp4.saveAppPageName = appList.saveAppPageName;
ntp4.setAppToBeHighlighted = appList.setAppToBeHighlighted;

document.addEventListener('DOMContentLoaded', appList.load);
