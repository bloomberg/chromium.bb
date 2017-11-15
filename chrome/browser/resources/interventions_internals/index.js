// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** The columns that are used to find rows that contain the keyword. */
const KEY_COLUMNS = ['log-type', 'log-description', 'log-url'];
const ENABLE_BLACKLIST_BUTTON = 'Enable Blacklist';
const IGNORE_BLACKLIST_BUTTON = 'Ignore Blacklist';
const IGNORE_BLACKLIST_MESSAGE = 'Blacklist decisions are ignored.';
const URL_THRESHOLD = 40;  // Maximum URL length

/**
 * Convert milliseconds to human readable date/time format.
 * The return format will be "MM/dd/YYYY hh:mm:ss.sss"
 * @param {number} time Time in millisecond since Unix Epoch.
 * @return The converted string format.
 */
function getTimeFormat(time) {
  let date = new Date(time);
  let options = {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
  };

  let dateString = date.toLocaleDateString('en-US', options);
  return dateString + ' ' + date.getHours() + ':' + date.getMinutes() + ':' +
      date.getSeconds() + '.' + date.getMilliseconds();
}

/**
 * Switch the selected tab to 'selected-tab' class.
 */
function setSelectedTab() {
  let selected = document.querySelector('input[type=radio][name=tabs]:checked');
  let selectedTab = document.querySelector('#' + selected.value);

  selectedTab.className =
      selectedTab.className.replace('hidden-tab', 'selected-tab');
  selected.parentElement.className =
      selected.parentElement.className.replace('inactive-tab', 'active-tab');
}

/**
 * Change the previously selected element to 'hidden-tab' class, and switch the
 * selected element to 'selected-tab' class.
 */
function changeTab() {
  let lastSelected = document.querySelector('.selected-tab');
  let lastTab = document.querySelector('.active-tab');
  lastSelected.className =
      lastSelected.className.replace('selected-tab', 'hidden-tab');
  lastTab.className = lastTab.className.replace('active-tab', 'inactive-tab');

  setSelectedTab();
}

/**
 * Initialize the navigation bar, and setup OnChange listeners for the tabs.
 */
function setupTabControl() {
  // Initialize on change listeners.
  let tabs = document.querySelectorAll('input[type=radio][name=tabs]');
  tabs.forEach((tab) => {
    tab.addEventListener('change', changeTab);
  });

  let tabContents = document.querySelectorAll('.tab-content');
  tabContents.forEach((tab) => {
    tab.className += ' hidden-tab';
  });

  // Turn on the default selected tab.
  setSelectedTab();
}

/**
 * Initialize the search functionality of the search bar on the log tab.
 * Searching will hide any rows that don't contain the keyword in the search
 * bar.
 */
function setupLogSearch() {
  $('log-search-bar').addEventListener('keyup', () => {
    let keyword = $('log-search-bar').value.toUpperCase();
    let rows = document.querySelectorAll('.log-message');

    rows.forEach((row) => {
      let found = KEY_COLUMNS.some((column) => {
        return (row.querySelector('.' + column)
                    .textContent.toUpperCase()
                    .includes(keyword));
      });
      row.style.display = found ? '' : 'none';
    });
  });
}

/**
 * Shorten long URL string so that it can be displayed nicely on mobile devices.
 * If |url| is longer than URL_THRESHOLD, then it will be shorten, and a tooltip
 * element will be added so that user can see the original URL.
 *
 * @param {string} url The given URL string.
 * @return An DOM node with the original URL if the length is within THRESHOLD,
 * or the shorten URL with a tooltip element at the end of the string.
 */
function createUrlElement(url) {
  let urlTd = document.createElement('td');
  urlTd.setAttribute('class', 'log-url');

  if (url.length <= URL_THRESHOLD) {
    urlTd.textContent = url;
  } else {
    urlTd.textContent = url.substring(0, URL_THRESHOLD - 3) + '...';
    let tooltip = document.createElement('span');
    tooltip.setAttribute('class', 'url-tooltip');
    tooltip.textContent = url;
    urlTd.appendChild(tooltip);
  }
  return urlTd;
}

/**
 * Initialize the button to clear out all the log messages. This button only
 * remove the logs from the UI, and does not effect any decision made.
 */
function setupLogClear() {
  $('clear-log-button').addEventListener('click', () => {
    // Remove hosts from table.
    let logsTable = $('message-logs-table');
    for (let row = logsTable.rows.length - 1; row > 0; row--) {
      logsTable.deleteRow(row);
    }
  });
}

/** @constructor */
let InterventionsInternalPageImpl = function(request) {
  this.binding_ =
      new mojo.Binding(mojom.InterventionsInternalsPage, this, request);
};

InterventionsInternalPageImpl.prototype = {
  /**
   * Post a new log message to the web page.
   *
   * @override
   * @param {!MessageLog} log The new log message recorded by
   * PreviewsLogger.
   */
  logNewMessage: function(log) {
    let logsTable = $('message-logs-table');

    let tableRow = logsTable.insertRow(1);  // Index 0 belongs to header row.
    tableRow.setAttribute('class', 'log-message');

    let timeTd = document.createElement('td');
    timeTd.textContent = getTimeFormat(log.time);
    timeTd.setAttribute('class', 'log-time');
    tableRow.appendChild(timeTd);

    let typeTd = document.createElement('td');
    typeTd.setAttribute('class', 'log-type');
    typeTd.textContent = log.type;
    tableRow.appendChild(typeTd);

    let descriptionTd = document.createElement('td');
    descriptionTd.setAttribute('class', 'log-description');
    descriptionTd.textContent = log.description;
    tableRow.appendChild(descriptionTd);

    let urlTd = createUrlElement(log.url.url);
    tableRow.appendChild(urlTd);
  },

  /**
   * Update new blacklisted host to the web page.
   *
   * @override
   * @param {!string} host The blacklisted host.
   * @param {number} time The time when the host was blacklisted in milliseconds
   * since Unix epoch.
   */
  onBlacklistedHost: function(host, time) {
    let row = document.createElement('tr');
    row.setAttribute('class', 'blacklisted-host-row');

    let hostTd = document.createElement('td');
    hostTd.setAttribute('class', 'host-blacklisted');
    hostTd.textContent = host;
    row.appendChild(hostTd);

    let timeTd = document.createElement('td');
    timeTd.setAttribute('class', 'host-blacklisted-time');
    timeTd.textContent = getTimeFormat(time);
    row.appendChild(timeTd);

    // TODO(thanhdle): Insert row at correct index. crbug.com/776105.
    $('blacklisted-hosts-table').appendChild(row);
  },

  /**
   * Update to the page that the user blacklisted status has changed.
   *
   * @override
   * @param {boolean} blacklisted The time of the event in milliseconds since
   * Unix epoch.
   */
  onUserBlacklistedStatusChange: function(blacklisted) {
    let userBlacklistedStatus = $('user-blacklisted-status-value');
    userBlacklistedStatus.textContent =
        (blacklisted ? 'Blacklisted' : 'Not blacklisted');
  },

  /**
   * Update the blacklist cleared status on the page.
   *
   * @override
   * @param {number} time The time of the event in milliseconds since Unix
   * epoch.
   */
  onBlacklistCleared: function(time) {
    let blacklistClearedStatus = $('blacklist-last-cleared-time');
    blacklistClearedStatus.textContent = getTimeFormat(time);

    // Remove hosts from table.
    let blacklistedHostsTable = $('blacklisted-hosts-table');
    for (let row = blacklistedHostsTable.rows.length - 1; row > 0; row--) {
      blacklistedHostsTable.deleteRow(row);
    }
  },

  /**
   * Update the page with the new value of ignored blacklist decision status.
   *
   * @override
   * @param {boolean} ignored The new status of whether the previews blacklist
   * decisions is blacklisted or not.
   */
  onIgnoreBlacklistDecisionStatusChanged: function(ignored) {
    let ignoreButton = $('ignore-blacklist-button');
    ignoreButton.textContent =
        ignored ? ENABLE_BLACKLIST_BUTTON : IGNORE_BLACKLIST_BUTTON;

    // Update the status of blacklist ignored on the page.
    $('blacklist-ignored-status').textContent =
        ignored ? IGNORE_BLACKLIST_MESSAGE : '';
  },

  /**
   * Update the page with the new value of estimated Effective Connection Type
   * (ECT). Log the ECT to the ECT logs table.
   *
   * @override
   * @param {string} type The string representation of estimated ECT.
   */
  onEffectiveConnectionTypeChanged: function(type) {
    // Change the current ECT.
    let ectType = $('nqe-type');
    ectType.textContent = type;

    // Log ECT changed event.
    let nqeRow = document.createElement('tr');

    let timeCol = document.createElement('td');
    timeCol.textContent = getTimeFormat(Date.now());
    timeCol.setAttribute('class', 'nqe-time-column');
    nqeRow.appendChild((timeCol));

    let nqeCol = document.createElement('td');
    nqeCol.setAttribute('class', 'nqe-value-column');
    nqeCol.textContent = type;
    nqeRow.appendChild(nqeCol);
    $('nqe-logs-table').appendChild(nqeRow);
  },
};

cr.define('interventions_internals', () => {
  let pageHandler = null;

  function init(handler) {
    pageHandler = handler;
    getPreviewsEnabled();

    let ignoreButton = $('ignore-blacklist-button');
    ignoreButton.addEventListener('click', () => {
      // Whether the blacklist is currently ignored.
      let ignored = (ignoreButton.textContent == ENABLE_BLACKLIST_BUTTON);
      // Try to reverse the ignore status.
      pageHandler.setIgnorePreviewsBlacklistDecision(!ignored);
    });
  }

  /**
   * Retrieves the statuses of previews (i.e. Offline, LoFi, AMP Redirection),
   * and posts them on chrome://intervention-internals.
   */
  function getPreviewsEnabled() {
    pageHandler.getPreviewsEnabled()
        .then((response) => {
          let statuses = $('previews-statuses');

          // Sorting the keys by the status's description.
          let sortedKeys = Array.from(response.statuses.keys());
          sortedKeys.sort((a, b) => {
            return response.statuses.get(a).description >
                response.statuses.get(b).description;
          });

          sortedKeys.forEach((key) => {
            let value = response.statuses.get(key);
            let message = value.description + ': ';
            message += value.enabled ? 'Enabled' : 'Disabled';

            assert(!$(key), 'Component ' + key + ' already existed!');

            let node = document.createElement('p');
            node.setAttribute('class', 'previews-status-value');
            node.setAttribute('id', key);
            node.textContent = message;
            statuses.appendChild(node);
          });
        })
        .catch((error) => {
          console.error(error.message);
        });
  }

  return {
    init: init,
  };
});

window.setupFn = window.setupFn || function() {
  return Promise.resolve();
};

document.addEventListener('DOMContentLoaded', () => {
  setupTabControl();
  setupLogSearch();
  setupLogClear();
  let pageHandler = null;
  let pageImpl = null;

  window.setupFn().then(() => {
    if (window.testPageHandler) {
      pageHandler = window.testPageHandler;
    } else {
      pageHandler = new mojom.InterventionsInternalsPageHandlerPtr;
      Mojo.bindInterface(
          mojom.InterventionsInternalsPageHandler.name,
          mojo.makeRequest(pageHandler).handle);

      // Set up client side mojo interface.
      let client = new mojom.InterventionsInternalsPagePtr;
      pageImpl = new InterventionsInternalPageImpl(mojo.makeRequest(client));
      pageHandler.setClientPage(client);
    }

    interventions_internals.init(pageHandler);
  });
});
