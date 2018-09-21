// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('database_tab', function() {
  'use strict';

  /**
   * Compares two db rows by their origin.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} a The first value being
   *     compared.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} b The second value being
   *     compared.
   * @return {number} A negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   */
  function compareRowsByOrigin(a, b) {
    return a.origin.localeCompare(b.origin);
  }

  /**
   * Compares two db rows by their dirty bit.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} a The first value being
   *     compared.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} b The second value being
   *     compared.
   * @return {number} A negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   */
  function compareRowsByIsDirty(a, b) {
    return a.isDirty - b.isDirty;
  }

  /**
   * Compares two db rows by their last load time.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} a The first value being
   *     compared.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} b The second value being
   *     compared.
   * @return {number} A negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   */
  function compareRowsByLastLoaded(a, b) {
    return a.value.lastLoaded - a.value.lastLoaded;
  }

  /**
   * Compares two db rows by their CPU usage.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} a The first value being
   *     compared.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} b The second value being
   *     compared.
   * @return {number} A negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   */
  function compareRowsByCpuUsage(a, b) {
    const keyA =
        a.value.loadTimeEstimates ? a.value.loadTimeEstimates.avgCpuUsageUs : 0;
    const keyB =
        b.value.loadTimeEstimates ? b.value.loadTimeEstimates.avgCpuUsageUs : 0;
    return keyA - keyB;
  }

  /**
   * Compares two db rows by their memory usage.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} a The first value being
   *     compared.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} b The second value being
   *     compared.
   * @return {number} A negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   */
  function compareRowsByMemoryUsage(a, b) {
    const keyA = a.value.loadTimeEstimates ?
        a.value.loadTimeEstimates.avgFootprintKb :
        0;
    const keyB = b.value.loadTimeEstimates ?
        b.value.loadTimeEstimates.avgFootprintKb :
        0;
    return keyA - keyB;
  }

  /**
   * Compares two db rows by their load duration.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} a The first value being
   *     compared.
   * @param {mojom.SiteCharacteristicsDatabaseEntry} b The second value being
   *     compared.
   * @return {number} A negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   */
  function compareRowsByLoadDuration(a, b) {
    const keyA = a.value.loadTimeEstimates ?
        a.value.loadTimeEstimates.avgLoadDurationUs :
        0;
    const keyB = b.value.loadTimeEstimates ?
        b.value.loadTimeEstimates.avgLoadDurationUs :
        0;
    return keyA - keyB;
  }

  /**
   * @param {string} sortKey The sort key to get a function for.
   * @return {function(mojom.SiteCharacteristicsDatabaseEntry,
                       mojom.SiteCharacteristicsDatabaseEntry): number}
   *     A comparison function that compares two tab infos, returns
   *     negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   */
  function getSortFunctionForKey(sortKey) {
    switch (sortKey) {
      case 'origin':
        return compareRowsByOrigin;
      case 'dirty':
        return compareRowsByIsDirty;
      case 'lastLoaded':
        return compareRowsByLastLoaded;
      case 'cpuUsage':
        return compareRowsByCpuUsage;
      case 'memoryUsage':
        return compareRowsByMemoryUsage;
      case 'loadDuration':
        return compareRowsByLoadDuration;
      default:
        assertNotReached('Unknown sortKey: ' + sortKey);
    }
  }

  /**
   * @param {number} time A time in microseconds.
   * @return {string} A friendly, human readable string representing the input
   *    time with units.
   */
  function microsecondsToString(time) {
    if (time < 1000)
      return time.toString() + ' µs';
    time /= 1000;
    if (time < 1000)
      return time.toFixed(2) + ' ms';
    time /= 1000;
    return time.toFixed(2) + ' s';
  }

  /**
   * @param {number} value A memory amount in kilobytes.
   * @return {string} A friendly, human readable string representing the input
   *    time with units.
   */
  function kilobytesToString(value) {
    if (value < 1000)
      return value.toString() + ' KB';
    value /= 1000;
    if (value < 1000)
      return value.toFixed(1) + ' MB';
    value /= 1000;
    return value.toFixed(1) + ' GB';
  }

  /**
   * @param {!Object} item The item to retrieve a load time estimate for.
   * @param {string} propertyName Name of the load time estimate to retrieve.
   * @return {string} The requested load time estimate or 'N/A' if unavailable.
   */
  function formatLoadTimeEstimate(item, propertyName) {
    if (!item.value || !item.value.loadTimeEstimates)
      return 'N/A';

    const value = item.value.loadTimeEstimates[propertyName];
    if (propertyName.endsWith('Us')) {
      return microsecondsToString(value);
    } else if (propertyName.endsWith('Kb')) {
      return kilobytesToString(value);
    }
    return value.toString();
  }

  return {
    getSortFunctionForKey: getSortFunctionForKey,
    formatLoadTimeEstimate: formatLoadTimeEstimate,
  };
});

Polymer({
  is: 'database-tab',

  behaviors: [SortedTableBehavior],

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
    this.setSortKey('origin');
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
   * Returns a sort function to compare tab infos based on the provided sort key
   * and a boolean reverse flag.
   * @param {string} sortKey The sort key for the  returned function.
   * @param {boolean} sortReverse True if sorting is reversed.
   * @return {function({Object}, {Object}): number}
   *     A comparison function that compares two tab infos, returns
   *     negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   * @private
   */
  computeSortFunction_: function(sortKey, sortReverse) {
    // Polymer 2 may invoke multi-property observers before all properties
    // are defined.
    if (!sortKey)
      return (a, b) => 0;

    const sortFunction = database_tab.getSortFunctionForKey(sortKey);
    return (a, b) => {
      const comp = sortFunction(a, b);
      return sortReverse ? -comp : comp;
    };
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
    return database_tab.formatLoadTimeEstimate(item, propertyName);
  },
});
