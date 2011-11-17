// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the proxy setup:
 *
 *   - Shows the current proxy settings.
 *   - Has a button to reload these settings.
 *   - Shows the log entries for the most recent INIT_PROXY_RESOLVER source
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

    this.latestProxySourceId_ = 0;

    // Hook up the UI components.
    $(ProxyView.RELOAD_SETTINGS_BUTTON_ID).onclick =
        g_browser.sendReloadProxySettings.bind(g_browser);
    $(ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID).onclick =
        g_browser.sendClearBadProxies.bind(g_browser);

    // Register to receive proxy information as it changes.
    g_browser.addProxySettingsObserver(this, true);
    g_browser.addBadProxiesObserver(this, true);
    g_browser.sourceTracker.addSourceEntryObserver(this);
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
  ProxyView.PROXY_RESOLVER_LOG_PRE_ID = 'proxy-view-resolver-log';

  cr.addSingletonGetter(ProxyView);

  ProxyView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogStart: function(data) {
      // Need to reset this so the latest proxy source from the dump can be
      // identified when the log entries are loaded.
      this.latestProxySourceId_ = 0;
    },

    onLoadLogFinish: function(data, tabData) {
      // It's possible that the last INIT_PROXY_RESOLVER source was deleted from
      // the log, but earlier sources remain.  When that happens, clear the list
      // of entries here, to avoid displaying misleading information.
      if (tabData != this.latestProxySourceId_)
        this.clearLog_();
      return this.onProxySettingsChanged(data.proxySettings) &&
             this.onBadProxiesChanged(data.badProxies);
    },

    /**
     * Save view-specific state.
     *
     * Save the greatest seen proxy source id, so we will not incorrectly
     * identify the log source associated with the current proxy configuration.
     */
    saveState: function() {
      return this.latestProxySourceId_;
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
        addTextNode(badUntilCell, badUntilDate.toLocaleString());
      }
      return true;
    },

    /**
     * Called whenever SourceEntries are updated with new log entries.  Updates
     * |proxyResolverLogPre_| with the log entries of the INIT_PROXY_RESOLVER
     * SourceEntry with the greatest id.
     */
    onSourceEntriesUpdated: function(sourceEntries) {
      for (var i = sourceEntries.length - 1; i >= 0; --i) {
        var sourceEntry = sourceEntries[i];

        if (sourceEntry.getSourceType() != LogSourceType.INIT_PROXY_RESOLVER ||
            this.latestProxySourceId_ > sourceEntry.getSourceId()) {
          continue;
        }

        this.latestProxySourceId_ = sourceEntry.getSourceId();

        $(ProxyView.PROXY_RESOLVER_LOG_PRE_ID).innerText =
            sourceEntry.printAsText();
      }
    },

    /**
     * Clears the display of and log entries for the last proxy lookup.
     */
    clearLog_: function() {
      // Prevents display of partial logs.
      ++this.latestProxySourceId_;

      $(ProxyView.PROXY_RESOLVER_LOG_PRE_ID).innerHTML = '';
      $(ProxyView.PROXY_RESOLVER_LOG_PRE_ID).innerText = 'Deleted.';
    },

    onSourceEntriesDeleted: function(sourceIds) {
      if (sourceIds.indexOf(this.latestProxySourceId_) != -1)
        this.clearLog_();
    },

    onAllSourceEntriesDeleted: function() {
      this.clearLog_();
    }
  };

  return ProxyView;
})();
