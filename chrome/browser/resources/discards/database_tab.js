// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'database-tab',

  properties: {
    /**
     * List of database rows.
     * @private {?Array<!mojom.SiteCharacteristicsDatabaseEntry>}
     */
    rows_: {
      type: Array,
    },
  },

  /** @private {number} */
  updateTimer_: 0,

  /** @private {!Object} */
  requestedOrigins_: {},

  /** @private {?mojom.DiscardsDetailsProviderPtr} */
  uiHandler_: null,

  /** @override */
  ready: function() {
    this.requestedOrigins_ = {};
    this.uiHandler_ = discards.getOrCreateUiHandler();

    // Specifies the update interval of the table, in ms.
    const UPDATE_INTERVAL_MS = 1000;

    // Update immediately.
    this.updateDbRows_();

    // Set an interval timer to update periodically.
    this.updateTimer_ =
        setInterval(this.updateDbRows_.bind(this), UPDATE_INTERVAL_MS);
  },

  /**
   * Issues a request for the data and renders on response.
   * @private
   */
  updateDbRows_: function() {
    this.uiHandler_
        .getSiteCharacteristicsDatabase(Object.keys(this.requestedOrigins_))
        .then(response => {
          // Bail if the SiteCharacteristicsDatabase is turned off.
          if (!response.result)
            return;

          // Add any new origins to the (monotonically increasing)
          // set of requested origins.
          const dbRows = response.result.dbRows;
          for (let dbRow of dbRows)
            this.requestedOrigins_[dbRow.origin] = true;
          this.rows_ = dbRows;
        });
  },

  /**
   * Compares two db rows by their origin.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} a The first value being
   *     compared.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} b The second value being
   *     compared.
   * @return {number} A negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   * @private
   */
  compareRows_: function(a, b) {
    return a.origin.localeCompare(b.origin);
  },


  /**
   * @param {boolean} value The value to convert.
   * @return {string} A display string representing value.
   * @private
   */
  boolToString_: function(value) {
    return discards.boolToString(value);
  },

  /**
   * @param {number} time Time in seconds since epoch.
   *     in question.
   * @return {string} A user-friendly string explaining how long ago time
   *     occurred.
   * @private
   */
  lastUseToString_: function(time) {
    const nowSecondsFromEpoch = Math.round(Date.now() / 1000);
    return discards.durationToString(nowSecondsFromEpoch - time);
  },

  /**
   * @param {?mojom.SiteCharacteristicsFeature} feature The feature
   *     in question.
   * @return {string} A human-readable string representing the feature.
   * @private
   */
  featureToString_: function(feature) {
    if (!feature)
      return 'N/A';

    if (feature.useTimestamp) {
      const nowSecondsFromEpoch = Math.round(Date.now() / 1000);
      return 'Used ' +
          discards.durationToString(nowSecondsFromEpoch - feature.useTimestamp);
    }

    if (feature.observationDuration) {
      return discards.secondsToString(feature.observationDuration);
    }

    return 'N/A';
  },

  /**
   * @param {!Object} item The item to retrieve a load time estimate for.
   * @param {string} propertyName Name of the load time estimate to retrieve.
   * @return {string} The requested load time estimate or 'N/A' if unavailable.
   * @private
   */
  getLoadTimeEstimate_: function(item, propertyName) {
    if (!item.value || !item.value.loadTimeEstimates)
      return 'N/A';

    return item.value.loadTimeEstimates[propertyName].toString();
  },
});
