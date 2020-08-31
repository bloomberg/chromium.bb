/* Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

cr.ui.decorate('tabbox', cr.ui.TabBox);

cr.define('safe_browsing', function() {
  'use strict';
  /**
   * Asks the C++ SafeBrowsingUIHandler to get the lists of Safe Browsing
   * ongoing experiments and preferences.
   * The SafeBrowsingUIHandler should reply to addExperiment() and
   * addPreferences() (below).
   */
  function initialize() {
    cr.sendWithPromise('getExperiments', [])
        .then((experiments) => addExperiments(experiments));
    cr.sendWithPromise('getPrefs', []).then((prefs) => addPrefs(prefs));
    cr.sendWithPromise('getCookie', []).then((cookie) => addCookie(cookie));
    cr.sendWithPromise('getSavedPasswords', []).then((passwords) =>
        addSavedPasswords(passwords));
    cr.sendWithPromise('getDatabaseManagerInfo', []).then(
      function(databaseState) {
        const fullHashCacheState = databaseState.splice(-1,1);
        addDatabaseManagerInfo(databaseState);
        addFullHashCacheInfo(fullHashCacheState);
    });

    cr.sendWithPromise('getSentClientDownloadRequests', [])
        .then((sentClientDownloadRequests) => {
          sentClientDownloadRequests.forEach(function(cdr) {
            addSentClientDownloadRequestsInfo(cdr);
          });
        });
    cr.addWebUIListener(
        'sent-client-download-requests-update', function(result) {
          addSentClientDownloadRequestsInfo(result);
        });

    cr.sendWithPromise('getReceivedClientDownloadResponses', [])
        .then((receivedClientDownloadResponses) => {
          receivedClientDownloadResponses.forEach(function(cdr) {
            addReceivedClientDownloadResponseInfo(cdr);
          });
        });
    cr.addWebUIListener(
        'received-client-download-responses-update', function(result) {
          addReceivedClientDownloadResponseInfo(result);
        });

    cr.sendWithPromise('getSentCSBRRs', []).then((sentCSBRRs) => {
      sentCSBRRs.forEach(function(csbrr) {
        addSentCSBRRsInfo(csbrr);
      });
    });
    cr.addWebUIListener('sent-csbrr-update', function(result) {
      addSentCSBRRsInfo(result);
    });

    cr.sendWithPromise('getPGEvents', []).then((pgEvents) => {
      pgEvents.forEach(function(pgEvent) {
        addPGEvent(pgEvent);
      });
    });
    cr.addWebUIListener('sent-pg-event', function(result) {
      addPGEvent(result);
    });

    cr.sendWithPromise('getSecurityEvents', []).then((securityEvents) => {
      securityEvents.forEach(function(securityEvent) {
        addSecurityEvent(securityEvent);
      });
    });
    cr.addWebUIListener('sent-security-event', function(result) {
      addSecurityEvent(result);
    });

    cr.sendWithPromise('getPGPings', []).then((pgPings) => {
      pgPings.forEach(function(pgPing) {
        addPGPing(pgPing);
      });
    });
    cr.addWebUIListener('pg-pings-update', function(result) {
      addPGPing(result);
    });

    cr.sendWithPromise('getPGResponses', []).then((pgResponses) => {
      pgResponses.forEach(function(pgResponse) {
        addPGResponse(pgResponse);
      });
    });
    cr.addWebUIListener('pg-responses-update', function(result) {
      addPGResponse(result);
    });

    cr.sendWithPromise('getRTLookupPings', []).then((rtLookupPings) => {
      rtLookupPings.forEach(function(rtLookupPing) {
        addRTLookupPing(rtLookupPing);
      });
    });
    cr.addWebUIListener('rt-lookup-pings-update', function(result) {
      addRTLookupPing(result);
    });

    cr.sendWithPromise('getRTLookupResponses', []).then((rtLookupResponses) => {
      rtLookupResponses.forEach(function(rtLookupResponse) {
        addRTLookupResponse(rtLookupResponse);
      });
    });
    cr.addWebUIListener('rt-lookup-responses-update', function(result) {
      addRTLookupResponse(result);
    });

    cr.sendWithPromise('getRTLookupExperimentEnabled', [])
        .then((enabled) => addRTLookupExperimentEnabled(enabled));

    cr.sendWithPromise('getLogMessages', []).then((logMessages) => {
      logMessages.forEach(function(message) {
        addLogMessage(message);
      });
    });
    cr.addWebUIListener('log-messages-update', function(message) {
      addLogMessage(message);
    });

    cr.sendWithPromise('getReportingEvents', []).then((reportingEvents) => {
      reportingEvents.forEach(function(reportingEvent) {
        addReportingEvent(reportingEvent);
      });
    });
    cr.addWebUIListener('reporting-events-update', function(reportingEvent) {
      addReportingEvent(reportingEvent);
    });

    cr.sendWithPromise('getDeepScans', []).then((requests) => {
      requests.forEach(function(request) {
        addDeepScan(request);
      });
    });
    cr.addWebUIListener('deep-scan-request-update', function(result) {
      addDeepScan(result);
    });

    $('get-referrer-chain-form').addEventListener('submit', addReferrerChain);

    // Allow tabs to be navigated to by fragment. The fragment with be of the
    // format "#tab-<tab id>"
    showTab(window.location.hash.substr(5));
    window.onhashchange = function () {
      showTab(window.location.hash.substr(5));
    };

    // When the tab updates, update the anchor
    $('tabbox').addEventListener('selectedChange', function() {
      const tabbox = $('tabbox');
      const tabs = tabbox.querySelector('tabs').children;
      const selectedTab = tabs[tabbox.selectedIndex];
      window.location.hash = 'tab-' + selectedTab.id;
    }, true);
  }

  function addExperiments(result) {
    const resLength = result.length;
    let experimentsListFormatted = '';

    for (let i = 0; i < resLength; i += 2) {
      experimentsListFormatted += "<div><b>" + result[i + 1] +
          "</b>: " + result[i] + "</div>";
    }
    $('experiments-list').innerHTML = experimentsListFormatted;
  }

  function addPrefs(result) {
    const resLength = result.length;
    let preferencesListFormatted = "";

    for (let i = 0; i < resLength; i += 2) {
      preferencesListFormatted += "<div><b>" + result[i + 1] + "</b>: " +
          result[i] + "</div>";
    }
    $('preferences-list').innerHTML = preferencesListFormatted;
  }

  function addCookie(result) {
    const cookieFormatted = '<b>Value:</b> ' + result[0] + '\n' +
        '<b>Created:</b> ' + (new Date(result[1])).toLocaleString();
    $('cookie-panel').innerHTML = cookieFormatted;
  }

  function addSavedPasswords(result) {
    const resLength = result.length;
    let savedPasswordFormatted = "";

    for (let i = 0; i < resLength; i += 2) {
      savedPasswordFormatted += "<div>" + result[i];
      if (result[i+1]) {
        savedPasswordFormatted += " (GAIA password)";
      } else {
        savedPasswordFormatted += " (Enterprise password)";
      }
      savedPasswordFormatted += "</div>";
    }

    $('saved-passwords').innerHTML = savedPasswordFormatted;
  }

  function addDatabaseManagerInfo(result) {
    const resLength = result.length;
    let preferencesListFormatted = "";

    for (let i = 0; i < resLength; i += 2) {
      preferencesListFormatted += "<div><b>" + result[i] + "</b>: " +
          result[i + 1] + "</div>";
    }
    $('database-info-list').innerHTML = preferencesListFormatted;
  }

  function addFullHashCacheInfo(result) {
    $('full-hash-cache-info').innerHTML = result;
  }

  function addSentClientDownloadRequestsInfo(result) {
    const logDiv = $('sent-client-download-requests-list');
    appendChildWithInnerText(logDiv, result);
  }

  function addReceivedClientDownloadResponseInfo(result) {
    const logDiv = $('received-client-download-response-list');
    appendChildWithInnerText(logDiv, result);
  }

  function addSentCSBRRsInfo(result) {
    const logDiv = $('sent-csbrrs-list');
    appendChildWithInnerText(logDiv, result);
  }

  function addPGEvent(result) {
    const logDiv = $('pg-event-log');
    const eventFormatted = "[" + (new Date(result["time"])).toLocaleString() +
        "] " + result['message'];
    appendChildWithInnerText(logDiv, eventFormatted);
  }

  function addSecurityEvent(result) {
    const logDiv = $('security-event-log');
    const eventFormatted = "[" + (new Date(result["time"])).toLocaleString() +
        "] " + result['message'];
    appendChildWithInnerText(logDiv, eventFormatted);
  }

  function insertTokenToTable(tableId, token) {
    const row = $(tableId).insertRow();
    row.className = 'content';
    row.id = tableId + '-' + token;
    row.insertCell().className = 'content';
    row.insertCell().className = 'content';
  }

  function addResultToTable(tableId, token, result, position) {
    if ($(tableId + '-' + token) === null) {
      insertTokenToTable(tableId, token);
    }

    const cell = $(tableId + '-' + token).cells[position];
    cell.innerText = result;
  }

  function addPGPing(result) {
    addResultToTable('pg-ping-list', result[0], result[1], 0);
  }

  function addPGResponse(result) {
    addResultToTable('pg-ping-list', result[0], result[1], 1);
  }

  function addRTLookupPing(result) {
    addResultToTable('rt-lookup-ping-list', result[0], result[1], 0);
  }

  function addRTLookupResponse(result) {
    addResultToTable('rt-lookup-ping-list', result[0], result[1], 1);
  }

  function addDeepScan(result) {
    if (result['request_time'] != null) {
      const requestFormatted = '[' +
          (new Date(result['request_time'])).toLocaleString() + ']\n' +
          result['request'];
      addResultToTable('deep-scan-list', result['token'], requestFormatted, 0);
    }

    if (result['response_time'] != null) {
      if (result['response_status'] == 'SUCCESS') {
        // Display the response instead
        const resultFormatted = '[' +
            (new Date(result['response_time'])).toLocaleString() + ']\n' +
            result['response'];
        addResultToTable('deep-scan-list', result['token'], resultFormatted, 1);
      } else {
        // Display the error
        const resultFormatted = '[' +
            (new Date(result['response_time'])).toLocaleString() + ']\n' +
            result['response_status'];
        addResultToTable('deep-scan-list', result['token'], resultFormatted, 1);
      }
    }
  }

  function addRTLookupExperimentEnabled(enabled) {
    const enabledFormatted = '<b>RT Lookup Experiment Enabled:</b> ' + enabled;
    $('rt-lookup-experiment-enabled').innerHTML = enabledFormatted;
  }

  function addLogMessage(result) {
    const logDiv = $('log-messages');
    const eventFormatted = "[" + (new Date(result["time"])).toLocaleString() +
        "] " + result['message'];
    appendChildWithInnerText(logDiv, eventFormatted);
  }

  function addReportingEvent(result) {
    const logDiv = $('reporting-events');
    const eventFormatted = result['message'];
    appendChildWithInnerText(logDiv, eventFormatted);
  }

  function appendChildWithInnerText(logDiv, text) {
    if (!logDiv) {
      return;
    }
    const textDiv = document.createElement('div');
    textDiv.innerText = text;
    logDiv.appendChild(textDiv);
  }

  function addReferrerChain(ev) {
    // Don't navigate
    ev.preventDefault();

    cr.sendWithPromise('getReferrerChain', $('referrer-chain-url').value)
        .then((response) => {
          $('referrer-chain-content').innerHTML = response;
        });
  }

  function showTab(tabId) {
    if ($(tabId)) {
      $(tabId).selected = "selected";
    }
  }

  return {
    addSentCSBRRsInfo: addSentCSBRRsInfo,
    addSentClientDownloadRequestsInfo: addSentClientDownloadRequestsInfo,
    addReceivedClientDownloadResponseInfo:
        addReceivedClientDownloadResponseInfo,
    addPGEvent: addPGEvent,
    addSecurityEvent: addSecurityEvent,
    addPGPing: addPGPing,
    addPGResponse: addPGResponse,
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', safe_browsing.initialize);
