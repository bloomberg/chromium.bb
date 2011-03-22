// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var OptionsPage = options.OptionsPage;
var Preferences = options.Preferences;
var ProxyOptions = options.ProxyOptions;

/**
 * UI pref change handler.
 */
function handlePrefUpdate(e) {
  ProxyOptions.getInstance().updateControls();
}

/**
 * Monitor pref change of given element.
 */
function observePrefsUI(el) {
  Preferences.getInstance().addEventListener(el.pref, handlePrefUpdate);
}

/**
 * DOMContentLoaded handler, sets up the page.
 */
function load() {
  localStrings = new LocalStrings();

  if (cr.isChromeOS)
    document.documentElement.setAttribute('os', 'chromeos');

  // Decorate the existing elements in the document.
  cr.ui.decorate('input[pref][type=checkbox]', options.PrefCheckbox);
  cr.ui.decorate('input[pref][type=number]', options.PrefNumber);
  cr.ui.decorate('input[pref][type=radio]', options.PrefRadio);
  cr.ui.decorate('input[pref][type=range]', options.PrefRange);
  cr.ui.decorate('select[pref]', options.PrefSelect);
  cr.ui.decorate('input[pref][type=text]', options.PrefTextField);
  cr.ui.decorate('input[pref][type=url]', options.PrefTextField);
  ProxyOptions.getInstance().initializePage();

  Preferences.getInstance().initialize();
  chrome.send('coreOptionsInitialize');

  ProxyOptions.getInstance().visible = true;

  // Monitor pref change on the following elements because proxy page updates
  // UI state on visibility change. This works fine as an overlay in settings
  // page. But in the splitted webui, the UI becomes visible when the page
  // loads and the pref values are not fetched at this point. So we need to
  // monitor and update UI states when the pref values are fetched.
  observePrefsUI($('directProxy'));
  observePrefsUI($('manualProxy'));
  observePrefsUI($('autoProxy'));
  observePrefsUI($('proxyAllProtocols'));
}

document.addEventListener('DOMContentLoaded', load);

