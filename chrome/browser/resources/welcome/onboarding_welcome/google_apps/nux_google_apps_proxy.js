// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  // The metrics name corresponding to Nux EmailProvidersInteraction histogram.
  const GOOGLE_APPS_SELECTION_METRIC_NAME =
      'FirstRun.NewUserExperience.GoogleAppsSelection';

  /**
   * NuxGoogleAppsSelections enum.
   * These values are persisted to logs and should not be renumbered or
   * re-used.
   * See tools/metrics/histograms/enums.xml.
   * @enum {number}
   */
  const NuxGoogleAppsSelections = {
    Gmail_DEPRECATED: 0,
    YouTube: 1,
    Maps: 2,
    Translate: 3,
    News: 4,
    ChromeWebStore: 5,
  };

  /** @interface */
  class NuxGoogleAppsProxy {
    /**
     * Google app IDs are local to the list of Google apps, so their icon must
     * be cached by the handler that provided the IDs.
     * @param {number} appId
     */
    cacheBookmarkIcon(appId) {}

    /**
     * Returns a promise for an array of Google apps.
     * @return {!Promise<!Array<!nux.BookmarkListItem>>}
     */
    getGoogleAppsList() {}

    /**
     * @param {number} providerId This should match one of the histogram enum
     *     value for NuxGoogleAppsSelections.
     */
    recordProviderSelected(providerId) {}
  }

  /** @implements {nux.NuxGoogleAppsProxy} */
  class NuxGoogleAppsProxyImpl {
    /** @override */
    cacheBookmarkIcon(appId) {
      chrome.send('cacheGoogleAppIcon', [appId]);
    }

    /** @override */
    getGoogleAppsList() {
      return cr.sendWithPromise('getGoogleAppsList');
    }

    /** @override */
    recordProviderSelected(providerId) {
      chrome.metricsPrivate.recordEnumerationValue(
          GOOGLE_APPS_SELECTION_METRIC_NAME, providerId,
          Object.keys(NuxGoogleAppsSelections).length);
    }
  }

  cr.addSingletonGetter(NuxGoogleAppsProxyImpl);

  return {
    NuxGoogleAppsProxy: NuxGoogleAppsProxy,
    NuxGoogleAppsProxyImpl: NuxGoogleAppsProxyImpl,
  };
});
