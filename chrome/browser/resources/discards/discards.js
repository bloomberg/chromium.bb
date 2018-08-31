// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('discards', function() {
  'use strict';

  // The following variables are initialized by 'initialize'.
  // Points to the Mojo WebUI handler.
  let uiHandler;
  // After initialization this points to the discard info table body.
  let tabDiscardsInfoTableBody;
  // After initialization this points to the database info table body.
  let dbInfoTableBody;

  // This holds the sorted db infos as retrieved from the uiHandler.
  let dbInfos;
  // This holds the sorted tab discard tabInfos as retrieved from the uiHandler.
  let tabInfos;
  // Holds information about the current sorting of the table.
  let sortKey;
  let sortReverse;
  // Points to the timer that refreshes the table content.
  let updateTimer;

  // Specifies the update interval of the page, in ms.
  const UPDATE_INTERVAL_MS = 1000;

  /**
   * Ensures the info table table body has the appropriate length. Decorates
   * newly created rows with a 'row-index' attribute to enable event listeners
   * to quickly determine the index of the row.
   */
  function ensureInfoTableLength(infoTableBody, infos, rowCreator) {
    let rows = infoTableBody.querySelectorAll('tr');
    if (rows.length < infos.length) {
      for (let i = rows.length; i < infos.length; ++i) {
        let row = rowCreator();
        row.setAttribute('data-row-index', i.toString());
        infoTableBody.appendChild(row);
      }
    } else if (rows.length > infos.length) {
      for (let i = infos.length; i < rows.length; ++i) {
        infoTableBody.removeChild(rows[i]);
      }
    }
  }

  /**
   * Determines if the provided state is related to discarding.
   * @param {state} The discard state.
   * @return {boolean} True if the state is related to discarding, false
   *     otherwise.
   */
  function isDiscardRelatedState(state) {
    return state == mojom.LifecycleUnitState.PENDING_DISCARD ||
        state == mojom.LifecycleUnitState.DISCARDED;
  }

  /**
   * Compares two TabDiscardsInfos based on the data in the provided sort-key.
   * @param {string} sortKey The key of the sort. See the "data-sort-key"
   *     attribute of the table headers for valid sort-keys.
   * @param {boolean|number|string} a The first value being compared.
   * @param {boolean|number|string} b The second value being compared.
   * @return {number} A negative number if a < b, 0 if a == b, and a positive
   *     number if a > b.
   */
  function compareTabDiscardsInfos(sortKey, a, b) {
    let val1 = a[sortKey];
    let val2 = b[sortKey];

    // Compares strings.
    if (sortKey == 'title' || sortKey == 'tabUrl') {
      val1 = val1.toLowerCase();
      val2 = val2.toLowerCase();
      if (val1 == val2)
        return 0;
      return val1 > val2 ? 1 : -1;
    }

    // Compares boolean fields.
    if (['canFreeze', 'canDiscard', 'isAutoDiscardable'].includes(sortKey)) {
      if (val1 == val2)
        return 0;
      return val1 ? 1 : -1;
    }

    // Compare lifecycle state. This is actually a compound key.
    if (sortKey == 'state') {
      // If the keys are discarding state, then break ties using the discard
      // reason.
      if (val1 == val2 && isDiscardRelatedState(val1)) {
        val1 = a['discardReason'];
        val2 = b['discardReason'];
      }
      return val1 - val2;
    }

    // Compares numeric fields.
    // NOTE: visibility, loadingState and state are represented as a numeric
    // value.
    if ([
          'visibility',
          'loadingState',
          'discardCount',
          'utilityRank',
          'reactivationScore',
          'lastActiveSeconds',
          'siteEngagementScore',
        ].includes(sortKey)) {
      return val1 - val2;
    }

    assertNotReached('Unsupported sort key: ' + sortKey);
    return 0;
  }

  /**
   * Sorts the tab discards info data in |tabInfos| according to the current
   * |sortKey|.
   */
  function sortTabDiscardsInfoTable() {
    tabInfos = tabInfos.sort((a, b) => {
      return (sortReverse ? -1 : 1) * compareTabDiscardsInfos(sortKey, a, b);
    });
  }

  /**
   * Pluralizes a string according to the given count. Assumes that appending an
   * 's' is sufficient to make a string plural.
   * @param {string} s The string to be made plural if necessary.
   * @param {number} n The count of the number of ojects.
   * @return {string} The plural version of |s| if n != 1, otherwise |s|.
   */
  function maybeMakePlural(s, n) {
    return n == 1 ? s : s + 's';
  }

  /**
   * Converts a |seconds| interval to a user friendly string.
   * @param {number} seconds The interval to render.
   * @return {string} An English string representing the interval.
   */
  function secondsToString(seconds) {
    // These constants aren't perfect, but close enough.
    const SECONDS_PER_MINUTE = 60;
    const MINUTES_PER_HOUR = 60;
    const SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;
    const HOURS_PER_DAY = 24;
    const SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY;
    const DAYS_PER_WEEK = 7;
    const SECONDS_PER_WEEK = SECONDS_PER_DAY * DAYS_PER_WEEK;
    const SECONDS_PER_MONTH = SECONDS_PER_DAY * 30.5;
    const SECONDS_PER_YEAR = SECONDS_PER_DAY * 365;

    // Seconds.
    if (seconds < SECONDS_PER_MINUTE)
      return seconds.toString() + maybeMakePlural(' second', seconds);

    // Minutes.
    let minutes = Math.floor(seconds / SECONDS_PER_MINUTE);
    if (minutes < MINUTES_PER_HOUR) {
      return minutes.toString() + maybeMakePlural(' minute', minutes);
    }

    // Hours and minutes.
    let hours = Math.floor(seconds / SECONDS_PER_HOUR);
    minutes = minutes % MINUTES_PER_HOUR;
    if (hours < HOURS_PER_DAY) {
      let s = hours.toString() + maybeMakePlural(' hour', hours);
      if (minutes > 0) {
        s += ' and ' + minutes.toString() + maybeMakePlural(' minute', minutes);
      }
      return s;
    }

    // Days.
    let days = Math.floor(seconds / SECONDS_PER_DAY);
    if (days < DAYS_PER_WEEK) {
      return days.toString() + maybeMakePlural(' day', days);
    }

    // Weeks. There's an awkward gap to bridge where 4 weeks can have
    // elapsed but not quite 1 month. Be sure to use weeks to report that.
    let weeks = Math.floor(seconds / SECONDS_PER_WEEK);
    let months = Math.floor(seconds / SECONDS_PER_MONTH);
    if (months < 1) {
      return 'over ' + weeks.toString() + maybeMakePlural(' week', weeks);
    }

    // Months.
    let years = Math.floor(seconds / SECONDS_PER_YEAR);
    if (years < 1) {
      return 'over ' + months.toString() + maybeMakePlural(' month', months);
    }

    // Years.
    return 'over ' + years.toString() + maybeMakePlural(' year', years);
  }

  /**
   * Converts a |secondsAgo| duration to a user friendly string.
   * @param {number} secondsAgo The duration to render.
   * @return {string} An English string representing the duration.
   */
  function durationToString(secondsAgo) {
    let ret = secondsToString(secondsAgo);

    if (ret.endsWith(' seconds') || ret.endsWith(' second'))
      return 'just now';

    return ret + ' ago';
  }

  /**
   * Returns a string representation of a boolean value for display in a table.
   * @param {boolean} bool A boolean value.
   * @return {string} A string representing the bool.
   */
  function boolToString(bool) {
    return bool ? '✔' : '✘️';
  }

  /**
   * Returns a string representation of a visibility enum value for display in
   * a table.
   * @param {int} visibility A value in LifecycleUnitVisibility.
   * @return {string} A string representation of the visibility.
   */
  function visibilityToString(visibility) {
    switch (visibility) {
      case mojom.LifecycleUnitVisibility.HIDDEN:
        return 'hidden';
      case mojom.LifecycleUnitVisibility.OCCLUDED:
        return 'occluded';
      case mojom.LifecycleUnitVisibility.VISIBLE:
        return 'visible';
    }
    assertNotReached('Unknown visibility: ' + visibility);
  }

  /**
   * Returns a string representation of a loading state enum value for display
   * in a table.
   * @param {int} loadingState A value in LifecycleUnitLoadingState enum.
   * @return {string} A string representation of the loading state.
   */
  function loadingStateToString(loadingState) {
    switch (loadingState) {
      case mojom.LifecycleUnitLoadingState.UNLOADED:
        return 'unloaded';
      case mojom.LifecycleUnitLoadingState.LOADING:
        return 'loading';
      case mojom.LifecycleUnitLoadingState.LOADED:
        return 'loaded';
    }
    assertNotReached('Unknown loadingState: ' + loadingState);
  }

  /**
   * Returns a string representation of a discard reason.
   * @param {mojom.LifecycleUnitDiscardReason} reason The discard reason.
   * @return {string} A string representation of the discarding reason.
   */
  function discardReasonToString(reason) {
    switch (reason) {
      case mojom.LifecycleUnitDiscardReason.EXTERNAL:
        return 'external';
      case mojom.LifecycleUnitDiscardReason.PROACTIVE:
        return 'proactive';
      case mojom.LifecycleUnitDiscardReason.URGENT:
        return 'urgent';
    }
    assertNotReached('Unknown discard reason: ' + reason);
  }

  /**
   * Returns a string representation of a lifecycle state.
   * @param {mojom.LifecycleUnitState} state The lifecycle state.
   * @param {mojom.LifecycleUnitDiscardReason} reason The discard reason. This
   *     is only used if the state is discard related.
   * @param {int} visibility A value in LifecycleUnitVisibility.
   * @param {boolean} hasFocus Whether or not the tab has input focus.
   * @return {string} A string representation of the lifecycle state, augmented
   *     with the discard reason if appropriate.
   */
  function lifecycleStateToString(state, reason, visibility, hasFocus) {
    let pageLifecycleStateFromVisibilityAndFocus = function() {
      switch (visibility) {
        case mojom.LifecycleUnitVisibility.HIDDEN:
        case mojom.LifecycleUnitVisibility.OCCLUDED:
          // An occluded page is also considered hidden.
          return 'hidden';
        case mojom.LifecycleUnitVisibility.VISIBLE:
          return hasFocus ? 'active' : 'passive';
      }
      assertNotReached('Unknown visibility: ' + visibility);
    };

    switch (state) {
      case mojom.LifecycleUnitState.ACTIVE:
        return pageLifecycleStateFromVisibilityAndFocus();
      case mojom.LifecycleUnitState.THROTTLED:
        return pageLifecycleStateFromVisibilityAndFocus() + ' (throttled)';
      case mojom.LifecycleUnitState.PENDING_FREEZE:
        return pageLifecycleStateFromVisibilityAndFocus() + ' (pending frozen)';
      case mojom.LifecycleUnitState.FROZEN:
        return 'frozen';
      case mojom.LifecycleUnitState.PENDING_DISCARD:
        return pageLifecycleStateFromVisibilityAndFocus() +
            ' (pending discard (' + discardReasonToString(reason) + '))';
      case mojom.LifecycleUnitState.DISCARDED:
        return 'discarded (' + discardReasonToString(reason) + ')';
      case mojom.LifecycleUnitState.PENDING_UNFREEZE:
        return 'frozen (pending unfreeze)';
    }
    assertNotReached('Unknown lifecycle state: ' + state);
  }

  /**
   * Returns the index of the row in the table that houses the given |element|.
   * @param {HTMLElement} element Any element in the DOM.
   */
  function getRowIndex(element) {
    let row = element.closest('tr');
    return parseInt(row.getAttribute('data-row-index'), 10);
  }

  /**
   * Creates an empty tab discards table row with action-link listeners, etc.
   * By default the links are inactive.
   */
  function createEmptyTabDiscardsInfoTableRow() {
    let template = $('tab-discard-info-row');
    let content = document.importNode(template.content, true);
    let row = content.querySelector('tr');

    // Set up the listener for the auto-discardable toggle action.
    let isAutoDiscardable = row.querySelector('.is-auto-discardable-link');
    isAutoDiscardable.setAttribute('disabled', '');
    isAutoDiscardable.addEventListener('click', (e) => {
      // Get the info backing this row.
      let info = tabInfos[getRowIndex(e.target)];
      // Disable the action. The update function is responsible for
      // re-enabling actions if necessary.
      e.target.setAttribute('disabled', '');
      // Perform the action.
      uiHandler.setAutoDiscardable(info.id, !info.isAutoDiscardable)
          .then(updateTables());
    });

    let loadListener = function(e) {
      // Get the info backing this row.
      let info = tabInfos[getRowIndex(e.target)];
      // Perform the action.
      uiHandler.loadById(info.id);
    };
    let loadLink = row.querySelector('.load-link');
    loadLink.addEventListener('click', loadListener);

    // Set up the listeners for freeze links.
    let freezeListener = function(e) {
      // Get the info backing this row.
      let info = tabInfos[getRowIndex(e.target)];
      // Perform the action.
      uiHandler.freezeById(info.id);
    };
    let freezeLink = row.querySelector('.freeze-link');
    freezeLink.addEventListener('click', freezeListener);

    // Set up the listeners for discard links.
    let discardListener = function(e) {
      // Get the info backing this row.
      let info = tabInfos[getRowIndex(e.target)];
      // Determine whether this is urgent or not.
      let urgent = e.target.classList.contains('discard-urgent-link');
      // Disable the action. The update function is responsible for
      // re-enabling actions if necessary.
      e.target.setAttribute('disabled', '');
      // Perform the action.
      uiHandler.discardById(info.id, urgent).then((response) => {
        updateTables();
      });
    };
    let discardLink = row.querySelector('.discard-link');
    let discardUrgentLink = row.querySelector('.discard-urgent-link');
    discardLink.addEventListener('click', discardListener);
    discardUrgentLink.addEventListener('click', discardListener);

    return row;
  }

  /**
   * Given an "action-link" element, enables or disables it.
   */
  function setActionLinkEnabled(element, enabled) {
    if (enabled)
      element.removeAttribute('disabled');
    else
      element.setAttribute('disabled', '');
  }

  /**
   * Updates a tab discards info table row in place. Sets/unsets 'disabled'
   * attributes on action-links as necessary, and populates all contents.
   */
  function updateTabDiscardsInfoTableRow(row, info) {
    // Update the content.
    row.querySelector('.utility-rank-cell').textContent =
        info.utilityRank.toString();
    row.querySelector('.reactivation-score-cell').textContent =
        info.hasReactivationScore ? info.reactivationScore.toFixed(4) : 'N/A';
    row.querySelector('.site-engagement-score-cell').textContent =
        info.siteEngagementScore.toFixed(1);
    row.querySelector('.favicon-div').style.backgroundImage =
        cr.icon.getFavicon(info.tabUrl);
    row.querySelector('.title-div').textContent = info.title;
    row.querySelector('.tab-url-cell').textContent = info.tabUrl;
    row.querySelector('.visibility-cell').textContent =
        visibilityToString(info.visibility);
    row.querySelector('.loading-state-cell').textContent =
        loadingStateToString(info.loadingState);
    row.querySelector('.can-freeze-div').textContent =
        boolToString(info.canFreeze);
    row.querySelector('.can-discard-div').textContent =
        boolToString(info.canDiscard);
    // The lifecycle state is meaningless for tabs that have never been loaded.
    row.querySelector('.state-cell').textContent =
        (info.loadingState != mojom.LifecycleUnitLoadingState.UNLOADED ||
         info.discardCount > 0) ?
        lifecycleStateToString(
            info.state, info.discardReason, info.visibility, info.hasFocus) :
        '';
    row.querySelector('.discard-count-cell').textContent =
        info.discardCount.toString();
    row.querySelector('.is-auto-discardable-div').textContent =
        boolToString(info.isAutoDiscardable);
    row.querySelector('.last-active-cell').textContent =
        durationToString(info.lastActiveSeconds);

    // Update the tooltips with 'Can Freeze/Discard?' reasons.
    row.querySelector('.can-freeze-tooltip').innerHTML =
        info.cannotFreezeReasons.join('<br />');
    row.querySelector('.can-discard-tooltip').innerHTML =
        info.cannotDiscardReasons.join('<br />');

    row.querySelector('.is-auto-discardable-link').removeAttribute('disabled');
    setActionLinkEnabled(
        row.querySelector('.can-freeze-link'),
        (!info.canFreeze && info.cannotFreezeReasons.length > 0));
    setActionLinkEnabled(
        row.querySelector('.can-discard-link'),
        (!info.canDiscard && info.cannotDiscardReasons.length > 0));
    let loadLink = row.querySelector('.load-link');
    let freezeLink = row.querySelector('.freeze-link');
    let discardLink = row.querySelector('.discard-link');
    let discardUrgentLink = row.querySelector('.discard-urgent-link');

    // Determine which action links should be enabled/disabled. By default
    // everything is disabled and links are selectively enabled depending on the
    // tab state.
    let loadEnabled = false;
    let freezeEnabled = false;
    let discardEnabled = false;
    let discardUrgentEnabled = false;
    if (info.loadingState == mojom.LifecycleUnitLoadingState.UNLOADED) {
      loadEnabled = true;
    } else if (
        info.visibility == mojom.LifecycleUnitVisibility.HIDDEN ||
        info.visibility == mojom.LifecycleUnitVisibility.OCCLUDED) {
      // Only tabs that aren't visible can be frozen or discarded for now.
      freezeEnabled = true;
      discardEnabled = true;
      discardUrgentEnabled = true;
      switch (info.state) {
        case mojom.LifecycleUnitState.DISCARDED:
        case mojom.LifecycleUnitState.PENDING_DISCARD:
          discardUrgentEnabled = false;
          discardEnabled = false;
        // Deliberately fall through.

        case mojom.LifecycleUnitState.FROZEN:
        case mojom.LifecycleUnitState.PENDING_FREEZE:
          freezeEnabled = false;
        // Deliberately fall through.

        case mojom.LifecycleUnitState.THROTTLED:
        case mojom.LifecycleUnitState.ACTIVE:
          // Everything stays enabled,
      }
    }

    setActionLinkEnabled(loadLink, loadEnabled);
    setActionLinkEnabled(freezeLink, freezeEnabled);
    setActionLinkEnabled(discardLink, discardEnabled);
    setActionLinkEnabled(discardUrgentLink, discardUrgentEnabled);
  }

  /**
   * Causes the discards info table to be rendered. Reuses existing table rows
   * in place to minimize disruption to the page.
   */
  function renderTabDiscardsInfoTable() {
    ensureInfoTableLength(
        tabDiscardsInfoTableBody, tabInfos, createEmptyTabDiscardsInfoTableRow);
    let rows = tabDiscardsInfoTableBody.querySelectorAll('tr');
    for (let i = 0; i < tabInfos.length; ++i)
      updateTabDiscardsInfoTableRow(rows[i], tabInfos[i]);
  }

  function createEmptyDbInfoTableRow() {
    let template = $('database-info-row');
    let content = document.importNode(template.content, true);
    let row = content.querySelector('tr');

    return row;
  }

  /**
   * Returns a string representing the state of a feature.
   */
  function featureToString(now, feature) {
    if (feature.useTimestamp) {
      return 'Last Used: ' + durationToString(now - feature.useTimestamp);
    } else {
      // TODO(siggi): This should note that the feature has been deemed to
      //     be unused after a finch-controlled duration.
      return 'Total Observation: ' +
          secondsToString(feature.observationDuration);
    }
  }
  /**
   * Updates a db info table row in place.
   */
  function updateDbInfoTableRow(row, info) {
    row.querySelector('.origin-cell').textContent = info.origin;
    row.querySelector('.dirty-cell').textContent = boolToString(info.isDirty);
    let value = info.value;
    let lastLoaded = 'N/A';
    let updatesFaviconInBackground = 'N/A';
    let updatesTitleInBackground = 'N/A';
    let usesAudioInBackground = 'N/A';
    let usesNotificationsInBackground = 'N/A';
    let avgCpuUsage = 'N/A';
    let avgMemoryFootprint = 'N/A';
    if (value) {
      let nowSecondsFromEpoch = Math.round((new Date()).getTime() / 1000);
      lastLoaded = durationToString(nowSecondsFromEpoch - value.lastLoaded);

      updatesFaviconInBackground = featureToString(
          nowSecondsFromEpoch, value.updatesFaviconInBackground);
      updatesTitleInBackground =
          featureToString(nowSecondsFromEpoch, value.updatesTitleInBackground);
      usesAudioInBackground =
          featureToString(nowSecondsFromEpoch, value.usesAudioInBackground);
      usesNotificationsInBackground = featureToString(
          nowSecondsFromEpoch, value.usesNotificationsInBackground);

      let loadTimeEstimates = value.loadTimeEstimates;
      if (loadTimeEstimates) {
        avgCpuUsage = loadTimeEstimates.avgCpuUsageUs.toString();
        avgMemoryFootprint = loadTimeEstimates.avgFootprintKb.toString();
      }
    }
    row.querySelector('.last-loaded-cell').textContent = lastLoaded;

    row.querySelector('.updates-favicon-in-background-cell').textContent =
        updatesFaviconInBackground;
    row.querySelector('.updates-title-in-background-cell').textContent =
        updatesTitleInBackground;
    row.querySelector('.uses-audio-in-background-cell').textContent =
        usesAudioInBackground;
    row.querySelector('.uses-notifications-in-background-cell').textContent =
        usesNotificationsInBackground;
    row.querySelector('.avg-cpu-cell').textContent = avgCpuUsage;
    row.querySelector('.avg-memory-cell').textContent = avgMemoryFootprint;
  }

  function renderDbInfoTable() {
    ensureInfoTableLength(dbInfoTableBody, dbInfos, createEmptyDbInfoTableRow);
    let rows = dbInfoTableBody.querySelectorAll('tr');
    for (let i = 0; i < dbInfos.length; ++i)
      updateDbInfoTableRow(rows[i], dbInfos[i]);
  }

  function stableUpdateDatabaseInfoTableImpl() {
    // Add all the origins we've seen so far to requestedOrigins, which means
    // the table will grow monotonically until the page is reloaded.
    let requestedOrigins = [];
    for (let i = 0; i < dbInfos.length; ++i)
      requestedOrigins.push(dbInfos[i].origin);

    uiHandler.getSiteCharacteristicsDatabase(requestedOrigins)
        .then((response) => {
          // Bail if the SiteCharacteristicsDatabase is turned off.
          if (!response.result)
            return;

          let newInfos = response.result.dbRows;
          let stableInfos = [];

          // Update existing dbInfos in place, remove old ones, and append new
          // ones. This tries to keep the existing ordering stable so that
          // clicking links is minimally disruptive.
          for (let i = 0; i < dbInfos.length; ++i) {
            let oldInfo = dbInfos[i];
            let newInfo = null;
            for (let j = 0; j < newInfos.length; ++j) {
              if (newInfos[j].origin == oldInfo.origin) {
                newInfo = newInfos[j];
                break;
              }
            }

            // Old dbInfos that have corresponding new dbInfos are pushed first,
            // in the current order of the old dbInfos.
            if (newInfo != null)
              stableInfos.push(newInfo);
          }

          // Make sure info about new tabs is appended to the end, in the order
          // they were originally returned.
          for (let i = 0; i < newInfos.length; ++i) {
            let newInfo = newInfos[i];
            let oldInfo = null;
            for (let j = 0; j < dbInfos.length; ++j) {
              if (dbInfos[j].origin == newInfo.origin) {
                oldInfo = dbInfos[j];
                break;
              }
            }

            // Entirely new information (has no corresponding old info) is
            // appended to the end.
            if (oldInfo == null)
              stableInfos.push(newInfo);
          }

          // Swap out the current info with the new stably sorted information.
          dbInfos = stableInfos;

          // Render the content in place.
          renderDbInfoTable();
        });
  }

  /**
   * Causes the discard info table to be updated in as stable a manner as
   * possible. That is, rows will stay in their relative positions, even if the
   * current sort order is violated. Only the addition or removal of rows (tabs)
   * can cause the layout to change.
   */
  function stableUpdateTabDiscardsInfoTableImpl() {
    uiHandler.getTabDiscardsInfo().then((response) => {
      let newInfos = response.infos;
      let stableInfos = [];

      // Update existing tabInfos in place, remove old ones, and append new
      // ones. This tries to keep the existing ordering stable so that clicking
      // links is minimally disruptive.
      for (let i = 0; i < tabInfos.length; ++i) {
        let oldInfo = tabInfos[i];
        let newInfo = null;
        for (let j = 0; j < newInfos.length; ++j) {
          if (newInfos[j].id == oldInfo.id) {
            newInfo = newInfos[j];
            break;
          }
        }

        // Old tabInfos that have corresponding new tabInfos are pushed first,
        // in the current order of the old tabInfos.
        if (newInfo != null)
          stableInfos.push(newInfo);
      }

      // Make sure info about new tabs is appended to the end, in the order they
      // were originally returned.
      for (let i = 0; i < newInfos.length; ++i) {
        let newInfo = newInfos[i];
        let oldInfo = null;
        for (let j = 0; j < tabInfos.length; ++j) {
          if (tabInfos[j].id == newInfo.id) {
            oldInfo = tabInfos[j];
            break;
          }
        }

        // Entirely new information (has no corresponding old info) is appended
        // to the end.
        if (oldInfo == null)
          stableInfos.push(newInfo);
      }

      // Swap out the current info with the new stably sorted information.
      tabInfos = stableInfos;

      // Render the content in place.
      renderTabDiscardsInfoTable();
    });
  }

  /**
   * Initiates table updates, called on a timer as well as explicitly on
   * user action.
   */
  function updateTablesImpl() {
    stableUpdateTabDiscardsInfoTableImpl();
    stableUpdateDatabaseInfoTableImpl();
  }

  /**
   * A wrapper to updateTablesImpl that is called due to user action and not
   * due to the automatic timer. Cancels the existing timer  and reschedules it
   * after rendering instantaneously.
   */
  function updateTables() {
    if (updateTimer)
      clearInterval(updateTimer);
    updateTablesImpl();
    updateTimer = setInterval(updateTablesImpl, UPDATE_INTERVAL_MS);
  }

  /**
   * Initializes the navigation bar with buttons for each content header
   * in the content element.
   */
  function initNavBar() {
    const tabContents = document.querySelectorAll('#content > div');
    for (let i = 0; i != tabContents.length; i++) {
      const tabContent = tabContents[i];
      const tabName = tabContent.querySelector('.content-header').textContent;

      const tabHeader = document.createElement('div');
      tabHeader.className = 'tab-header';
      const button = document.createElement('button');
      button.textContent = tabName;
      tabHeader.appendChild(button);
      tabHeader.addEventListener('click', selectTab.bind(null, tabContent.id));
      $('navigation').appendChild(tabHeader);
    }
  }

  /**
   * Event handler that selects the tab indicated by the window location hash.
   * Invoked on hashchange events and initialization.
   */
  function onHashChange() {
    const hash = window.location.hash.slice(1).toLowerCase();
    if (!selectTab(hash))
      selectTab('discards');
  }

  /**
   * @param {string} id Tab id.
   * @return {boolean} True if successful.
   */
  function selectTab(id) {
    const tabContents = document.querySelectorAll('#content > div');
    const tabHeaders = $('navigation').querySelectorAll('.tab-header');
    let found = false;
    for (let i = 0; i != tabContents.length; i++) {
      const tabContent = tabContents[i];
      const tabHeader = tabHeaders[i];
      if (tabContent.id == id) {
        tabContent.classList.add('selected');
        tabHeader.classList.add('selected');
        found = true;
      } else {
        tabContent.classList.remove('selected');
        tabHeader.classList.remove('selected');
      }
    }
    if (!found)
      return false;
    window.location.hash = id;
    return true;
  }

  /**
   * Initializes this page. Invoked by the DOMContentLoaded event.
   */
  function initialize() {
    let importLinks = document.querySelectorAll('link[rel=import]');
    let contentNode = $('content');
    for (let i = 0; i < importLinks.length; ++i) {
      let importLink = /** @type {!HTMLLinkElement} */ (importLinks[i]);
      if (!importLink.import) {
        // Happens when a <link rel=import> is inside a <template>.
        continue;
      }
      let tabContentsNode = importLink.import.querySelector('#tab_contents');
      contentNode.appendChild(
          document.importNode(tabContentsNode.firstElementChild, true));
    }

    initNavBar();
    onHashChange();

    uiHandler = new mojom.DiscardsDetailsProviderPtr;
    Mojo.bindInterface(
        mojom.DiscardsDetailsProvider.name, mojo.makeRequest(uiHandler).handle);

    dbInfoTableBody = $('database-info-table-body');
    dbInfos = [];

    tabDiscardsInfoTableBody = $('tab-discards-info-table-body');
    tabInfos = [];
    sortKey = 'utilityRank';
    sortReverse = false;
    updateTimer = null;

    // Set the column sort handlers.
    let tabDiscardsInfoTableHeader = $('tab-discards-info-table-header');
    let headers = tabDiscardsInfoTableHeader.children;
    for (let header of headers) {
      header.addEventListener('click', (e) => {
        let newSortKey = e.target.dataset.sortKey;

        // Skip columns that aren't explicitly labeled with a sort-key
        // attribute.
        if (newSortKey == null)
          return;

        // Reverse the sort key if the key itself hasn't changed.
        if (sortKey == newSortKey) {
          sortReverse = !sortReverse;
        } else {
          sortKey = newSortKey;
          sortReverse = false;
        }

        // Undecorate the old sort column, and decorate the new one.
        let oldSortColumn = document.querySelector('.sort-column');
        oldSortColumn.classList.remove('sort-column');
        e.target.classList.add('sort-column');
        if (sortReverse)
          e.target.setAttribute('data-sort-reverse', '');
        else
          e.target.removeAttribute('data-sort-reverse');

        sortTabDiscardsInfoTable();
        renderTabDiscardsInfoTable();
      });
    }

    // Setup the "Discard a tab now" links.
    let discardNow = $('discard-now-link');
    let discardNowUrgent = $('discard-now-urgent-link');
    let discardListener = function(e) {
      e.target.setAttribute('disabled', '');
      let urgent = e.target.id.includes('urgent');
      uiHandler.discard(urgent).then(() => {
        updateTables();
        e.target.removeAttribute('disabled');
      });
    };
    discardNow.addEventListener('click', discardListener);
    discardNowUrgent.addEventListener('click', discardListener);

    updateTables();
  }

  document.addEventListener('DOMContentLoaded', initialize);
  window.addEventListener('hashchange', onHashChange);

  // These functions are exposed on the 'discards' object created by
  // cr.define. This allows unittesting of these functions.
  return {
    compareTabDiscardsInfos: compareTabDiscardsInfos,
    durationToString: durationToString,
    maybeMakePlural: maybeMakePlural
  };
});
