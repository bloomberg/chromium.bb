// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the proxy setup:
 *
 *   - Shows the current proxy settings.
 *   - Has a button to reload these settings.
 *   - Shows the list of proxy hostnames that are cached as "bad".
 *   - Has a button to clear the cached bad proxies.
 */
var ProxyView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function ProxyView() {
    assertFirstConstructorCall(ProxyView);

    // Call superclass's constructor.
    superClass.call(this, ProxyView.MAIN_BOX_ID);

    // Hook up the UI components.
    $(ProxyView.RELOAD_SETTINGS_BUTTON_ID).onclick =
        g_browser.sendReloadProxySettings.bind(g_browser);
    $(ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID).onclick =
        g_browser.sendClearBadProxies.bind(g_browser);

    // Register to receive proxy information as it changes.
    g_browser.addProxySettingsObserver(this, true);
    g_browser.addBadProxiesObserver(this, true);
  }

  // ID for special HTML element in category_tabs.html
  ProxyView.TAB_HANDLE_ID = 'tab-handle-proxy';

  // IDs for special HTML elements in proxy_view.html
  ProxyView.MAIN_BOX_ID = 'proxy-view-tab-content';
  ProxyView.ORIGINAL_SETTINGS_DIV_ID = 'proxy-view-original-settings';
  ProxyView.EFFECTIVE_SETTINGS_DIV_ID = 'proxy-view-effective-settings';
  ProxyView.RELOAD_SETTINGS_BUTTON_ID = 'proxy-view-reload-settings';
  ProxyView.BAD_PROXIES_TBODY_ID = 'proxy-view-bad-proxies-tbody';
  ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID = 'proxy-view-clear-bad-proxies';

  cr.addSingletonGetter(ProxyView);

  ProxyView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onProxySettingsChanged(data.proxySettings) &&
             this.onBadProxiesChanged(data.badProxies);
    },

    onProxySettingsChanged: function(proxySettings) {
      // Both |original| and |effective| are dictionaries describing the
      // settings.
      $(ProxyView.ORIGINAL_SETTINGS_DIV_ID).innerHTML = '';
      $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).innerHTML = '';

      if (!proxySettings)
        return false;

      var original = proxySettings.original;
      var effective = proxySettings.effective;

      $(ProxyView.ORIGINAL_SETTINGS_DIV_ID).innerText =
          proxySettingsToString(original);
      $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).innerText =
          proxySettingsToString(effective);
      return true;
    },

    onBadProxiesChanged: function(badProxies) {
      $(ProxyView.BAD_PROXIES_TBODY_ID).innerHTML = '';

      if (!badProxies)
        return false;

      // Add a table row for each bad proxy entry.
      for (var i = 0; i < badProxies.length; ++i) {
        var entry = badProxies[i];
        var badUntilDate = timeutil.convertTimeTicksToDate(entry.bad_until);

        var tr = addNode($(ProxyView.BAD_PROXIES_TBODY_ID), 'tr');

        var nameCell = addNode(tr, 'td');
        var badUntilCell = addNode(tr, 'td');

        addTextNode(nameCell, entry.proxy_uri);
        timeutil.addNodeWithDate(badUntilCell, badUntilDate);
      }
      return true;
    }
  };

  return ProxyView;
})();
