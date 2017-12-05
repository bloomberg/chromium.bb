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
 * Insert a log message row to the top of the log message table.
 *
 * @param {number!} time Millisecond since Unix Epoch representation of time.
 * @param {string!} type The message event type.
 * @param {string!} description The event message description.
 * @param {string} url The URL associated with the event.
 */
function insertMessageRowToMessageLogTable(time, type, description, url) {
  let tableRow =
      $('message-logs-table').insertRow(1);  // Index 0 belongs to header row.
  tableRow.setAttribute('class', 'log-message');

  let timeTd = document.createElement('td');
  timeTd.textContent = getTimeFormat(time);
  timeTd.setAttribute('class', 'log-time');
  tableRow.appendChild(timeTd);

  let typeTd = document.createElement('td');
  typeTd.setAttribute('class', 'log-type');
  typeTd.textContent = type;
  tableRow.appendChild(typeTd);

  let descriptionTd = document.createElement('td');
  descriptionTd.setAttribute('class', 'log-description');
  descriptionTd.textContent = description;
  tableRow.appendChild(descriptionTd);

  if (url.length > 0) {
    let urlTd = createUrlElement(url);
    urlTd.setAttribute('class', 'log-url');
    tableRow.appendChild(urlTd);
  }
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
 * Create and add a copy to clipboard button to a given node.
 *
 * @param {string} text The text that will be copied to the clipboard.
 * @param {element!} node The node that will have the button appended to.
 */
function appendCopyToClipBoardButton(text, node) {
  if (!document.queryCommandSupported ||
      !document.queryCommandSupported('copy')) {
    // Don't add copy to clipboard button if not supported.
    return;
  }
  let copyButton = document.createElement('div');
  copyButton.setAttribute('class', 'copy-to-clipboard-button');
  copyButton.textContent = 'Copy';

  copyButton.addEventListener('click', () => {
    var textarea = document.createElement('textarea');
    textarea.textContent = text;
    document.body.appendChild(textarea);
    textarea.select();
    try {
      return document.execCommand('copy');  // Security exception may be thrown.
    } catch (ex) {
      console.warn('Copy to clipboard failed.', ex);
      return false;
    } finally {
      document.body.removeChild(textarea);
    }
  });
  node.appendChild(copyButton);
}

/**
 * Shorten long URL string so that it can be displayed nicely on mobile devices.
 * If |url| is longer than URL_THRESHOLD, then it will be shorten, and a tooltip
 * element will be added so that user can see the original URL.
 *
 * Add copy to clipboard button to it.
 *
 * @param {string} url The given URL string.
 * @return An DOM node with the original URL if the length is within THRESHOLD,
 * or the shorten URL with a tooltip element at the end of the string.
 */
function createUrlElement(url) {
  let urlCell = document.createElement('div');
  urlCell.setAttribute('class', 'log-url-value');
  let urlTd = document.createElement('td');
  urlTd.appendChild(urlCell);

  if (url.length <= URL_THRESHOLD) {
    urlCell.textContent = url;
  } else {
    urlCell.textContent = url.substring(0, URL_THRESHOLD - 3) + '...';
    let tooltip = document.createElement('span');
    tooltip.setAttribute('class', 'url-tooltip');
    tooltip.textContent = url;
    urlTd.appendChild(tooltip);
  }

  // Append copy to clipboard button.
  appendCopyToClipBoardButton(url, urlTd);
  return urlTd;
}

/**
 * Helper function to remove all log message from log-messages-table.
 */
function removeAllLogMessagesRows() {
  let logsTable = $('message-logs-table');
  for (let row = logsTable.rows.length - 1; row > 0; row--) {
    logsTable.deleteRow(row);
  }
}

/**
 * Initialize the button to clear out all the log messages. This button only
 * remove the logs from the UI, and does not effect any decision made.
 */
function setupLogClear() {
  $('clear-log-button').addEventListener('click', removeAllLogMessagesRows);
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
    insertMessageRowToMessageLogTable(
        log.time, log.type, log.description, log.url.url);
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

    // Remove log message from logs table.
    removeAllLogMessagesRows();
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

    let now = getTimeFormat(Date.now());

    // Log ECT changed event to ECT change log.
    let nqeRow =
        $('nqe-logs-table').insertRow(1);  // Index 0 belongs to header row.

    let timeCol = document.createElement('td');
    timeCol.textContent = now;
    timeCol.setAttribute('class', 'nqe-time-column');
    nqeRow.appendChild((timeCol));

    let nqeCol = document.createElement('td');
    nqeCol.setAttribute('class', 'nqe-value-column');
    nqeCol.textContent = type;
    nqeRow.appendChild(nqeCol);

    // Insert ECT changed message to message-logs-table.
    insertMessageRowToMessageLogTable(
        now, 'ECT Changed', 'Effective Connection Type changed to ' + type, '');
  },
};

cr.define('interventions_internals', () => {
  let pageHandler = null;

  function init(handler) {
    pageHandler = handler;
    getPreviewsEnabled();
    getPreviewsFlagsDetails();

    let ignoreButton = $('ignore-blacklist-button');
    ignoreButton.addEventListener('click', () => {
      // Whether the blacklist is currently ignored.
      let ignored = (ignoreButton.textContent == ENABLE_BLACKLIST_BUTTON);
      // Try to reverse the ignore status.
      pageHandler.setIgnorePreviewsBlacklistDecision(!ignored);
    });
  }

  /**
   * Sort keys by the value of each value by its description attribute of a
   * |mapObject|.
   *
   * @param mapObject {!Map<string, Object} A map where all values have a
   * description attribute.
   * @return A list of keys sorted by their descriptions.
   */
  function getSortedKeysByDescription(mapObject) {
    let sortedKeys = Array.from(mapObject.keys());
    sortedKeys.sort((a, b) => {
      return mapObject.get(a).description > mapObject.get(b).description;
    });
    return sortedKeys;
  }

  /**
   * Retrieves the statuses of previews (i.e. Offline, LoFi, AMP Redirection),
   * and posts them on chrome://intervention-internals.
   */
  function getPreviewsEnabled() {
    pageHandler.getPreviewsEnabled()
        .then((response) => {
          let statuses = $('previews-enabled-status');

          getSortedKeysByDescription(response.statuses).forEach((key) => {
            let value = response.statuses.get(key);
            let message = value.description + ': ';
            message += value.enabled ? 'Enabled' : 'Disabled';

            assert(!$(key), 'Component ' + key + ' already existed!');

            let node = document.createElement('div');
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

  function getPreviewsFlagsDetails() {
    pageHandler.getPreviewsFlagsDetails()
        .then((response) => {
          let flags = $('previews-flags-table');

          getSortedKeysByDescription(response.flags).forEach((key) => {
            let value = response.flags.get(key);
            assert(!$(key), 'Component ' + key + ' already existed!');

            let flagDescription = document.createElement('a');
            flagDescription.setAttribute('class', 'previews-flag-description');
            flagDescription.setAttribute('id', key + 'Description');
            flagDescription.setAttribute('href', value.link);
            flagDescription.textContent = value.description;

            let flagNameTd = document.createElement('td');
            flagNameTd.appendChild(flagDescription);

            let flagValueTd = document.createElement('td');
            flagValueTd.setAttribute('class', 'previews-flag-value');
            flagValueTd.setAttribute('id', key + 'Value');
            flagValueTd.textContent = value.value;

            let node = document.createElement('tr');
            node.setAttribute('class', 'previews-flag-container');
            node.appendChild(flagNameTd);
            node.appendChild(flagValueTd);
            flags.appendChild(node);
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
