// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var OptionsPage = options.OptionsPage;
var Preferences = options.Preferences;
var ProxyOptions = options.ProxyOptions;

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
}

document.addEventListener('DOMContentLoaded', load);

