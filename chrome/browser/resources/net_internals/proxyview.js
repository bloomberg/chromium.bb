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
 *
 *  @constructor
 */
function ProxyView() {
  const mainBoxId = 'proxyTabContent';
  const originalSettingsDivId = 'proxyOriginalSettings';
  const effectiveSettingsDivId = 'proxyEffectiveSettings';
  const reloadSettingsButtonId = 'proxyReloadSettings';
  const badProxiesTbodyId = 'badProxiesTableBody';
  const clearBadProxiesButtonId = 'clearBadProxies';
  const proxyResolverLogPreId = 'proxyResolverLog';

  DivView.call(this, mainBoxId);

  this.latestProxySourceId_ = 0;

  // Hook up the UI components.
  this.originalSettingsDiv_ = $(originalSettingsDivId);
  this.effectiveSettingsDiv_ = $(effectiveSettingsDivId);
  this.proxyResolverLogPre_ = $(proxyResolverLogPreId);
  this.badProxiesTbody_ = $(badProxiesTbodyId);

  var reloadSettingsButton = $(reloadSettingsButtonId);
  var clearBadProxiesButton = $(clearBadProxiesButtonId);

  clearBadProxiesButton.onclick = g_browser.sendClearBadProxies.bind(g_browser);
  reloadSettingsButton.onclick =
      g_browser.sendReloadProxySettings.bind(g_browser);

  // Register to receive proxy information as it changes.
  g_browser.addProxySettingsObserver(this);
  g_browser.addBadProxiesObserver(this);
  g_browser.sourceTracker.addObserver(this);
}

inherits(ProxyView, DivView);

ProxyView.prototype.onLoadLogStart = function(data) {
  // Need to reset this so the latest proxy source from the dump can be
  // identified when the log entries are loaded.
  this.latestProxySourceId_ = 0;
};

ProxyView.prototype.onLoadLogFinish = function(data, tabData) {
  // It's possible that the last INIT_PROXY_RESOLVER source was deleted from the
  // log, but earlier sources remain.  When that happens, clear the list of
  // entries here, to avoid displaying misleading information.
  if (tabData != this.latestProxySourceId_)
    this.clearLog_();
  return this.onProxySettingsChanged(data.proxySettings) &&
         this.onBadProxiesChanged(data.badProxies);
};

/**
 * Save view-specific state.
 *
 * Save the greatest seen proxy source id, so we will not incorrectly identify
 * the log source associated with the current proxy configuration.
 */
ProxyView.prototype.saveState = function() {
  return this.latestProxySourceId_;
};

ProxyView.prototype.onProxySettingsChanged = function(proxySettings) {
  // Both |original| and |effective| are dictionaries describing the settings.
  this.originalSettingsDiv_.innerHTML = '';
  this.effectiveSettingsDiv_.innerHTML = '';

  if (!proxySettings)
    return false;

  var original = proxySettings.original;
  var effective = proxySettings.effective;

  if (!original || !effective)
    return false;

  addTextNode(this.originalSettingsDiv_, proxySettingsToString(original));
  addTextNode(this.effectiveSettingsDiv_, proxySettingsToString(effective));
  return true;
};

ProxyView.prototype.onBadProxiesChanged = function(badProxies) {
  this.badProxiesTbody_.innerHTML = '';

  if (!badProxies)
    return false;

  // Add a table row for each bad proxy entry.
  for (var i = 0; i < badProxies.length; ++i) {
    var entry = badProxies[i];
    var badUntilDate = convertTimeTicksToDate(entry.bad_until);

    var tr = addNode(this.badProxiesTbody_, 'tr');

    var nameCell = addNode(tr, 'td');
    var badUntilCell = addNode(tr, 'td');

    addTextNode(nameCell, entry.proxy_uri);
    addTextNode(badUntilCell, badUntilDate.toLocaleString());
  }
  return true;
};

ProxyView.prototype.onSourceEntryUpdated = function(sourceEntry) {
  if (sourceEntry.getSourceType() != LogSourceType.INIT_PROXY_RESOLVER ||
      this.latestProxySourceId_ > sourceEntry.getSourceId()) {
    return;
  }

  this.latestProxySourceId_ = sourceEntry.getSourceId();

  this.proxyResolverLogPre_.innerHTML = '';
  addTextNode(this.proxyResolverLogPre_, sourceEntry.printAsText());
};

/**
 * Clears the display of and log entries for the last proxy lookup.
 */
ProxyView.prototype.clearLog_ = function() {
  // Prevents display of partial logs.
  ++this.latestProxySourceId_;

  this.proxyResolverLogPre_.innerHTML = '';
  addTextNode(this.proxyResolverLogPre_, 'Deleted.');
};

ProxyView.prototype.onSourceEntriesDeleted = function(sourceIds) {
  if (sourceIds.indexOf(this.latestProxySourceId_) != -1)
    this.clearLog_();
};

ProxyView.prototype.onAllSourceEntriesDeleted = function() {
  this.clearLog_();
};
