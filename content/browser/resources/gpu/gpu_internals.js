// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="browser_bridge.js"/>
<include src="info_view.js"/>

var browserBridge;

/**
 * Main entry point. called once the page has loaded.
 */
function onLoad() {
  browserBridge = new gpu.BrowserBridge();

  // Create the views.
  cr.ui.decorate('#info-view', gpu.InfoView);

  // Create the main tab control
  var tabs = $('main-tabs');
  cr.ui.decorate(tabs, cr.ui.TabBox);

  // Sync the main-tabs selectedTabs in-sync with the location.
  tabs.addEventListener('selectedChange', function() {
    if (tabs.selectedTab.id) {
      history.pushState('', '', '#' + tabs.selectedTab.id);
    }
  });
  window.onhashchange = function() {
    var cur = window.location.hash;
    if (cur == '#' || cur == '') {
      tabs.selectedTab = $('info-view');
    } else {
      var tab = $(window.location.hash.substr(1));
      if (tab)
        tabs.selectedTab = tab;
    }
  };
  window.onhashchange();
}

document.addEventListener('DOMContentLoaded', onLoad);
