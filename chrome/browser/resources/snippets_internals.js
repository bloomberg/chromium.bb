// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chrome.SnippetsInternals', function() {
  'use strict';

  // Stores the list of suggestions we received in receiveContentSuggestions.
  var lastSuggestions = [];
  var lastDebugLog = '';

  function initialize() {
    $('submit-download').addEventListener('click', function(event) {
      chrome.send('download');
      event.preventDefault();
    });

    $('submit-dump').addEventListener('click', function(event) {
      downloadJson(JSON.stringify(lastSuggestions, null, 2));
      event.preventDefault();
    });

    $('debug-log-dump').addEventListener('click', function(event) {
      downloadDebugLog(lastDebugLog);
      event.preventDefault();
    });

    $('last-json-button').addEventListener('click', function(event) {
      $('last-json-container').classList.toggle('hidden');
    });

    $('last-json-dump').addEventListener('click', function(event) {
      downloadJson($('last-json-text').innerText);
      event.preventDefault();
    });

    $('clear-classification').addEventListener('click', function(event) {
      chrome.send('clearClassification');
      event.preventDefault();
    });

    $('background-fetch-button').addEventListener('click', function(event) {
      chrome.send('fetchRemoteSuggestionsInTheBackgroundIn2Seconds');
      event.preventDefault();
    });

    $('push-dummy-suggestion-10-seconds-button')
        .addEventListener('click', function(event) {
          chrome.send('pushDummySuggestionIn10Seconds');
          event.preventDefault();
        });

    if (loadTimeData.getBoolean('contextualSuggestionsEnabled')) {
      $('contextual-suggestions-section').classList.remove('hidden');
    }

    $('fetch-contextual-suggestions-button')
        .addEventListener('click', function(event) {
          let url = $('contextual-url').value;
          $('contextual-suggestions-request-result').textContent =
              'Fetching contextual suggestions for ' + url;
          chrome.send('fetchContextualSuggestions', [url]);
          event.preventDefault();
        });

    $('reset-notifications-state-button')
        .addEventListener('click', function(event) {
          chrome.send('resetNotificationsState');
          event.preventDefault();
        });

    window.addEventListener('focus', refreshContent);
    window.setInterval(refreshContent, 1000);

    chrome.send('initializationCompleted');
    refreshContent();
  }

  function receiveProperty(propertyId, value) {
    $(propertyId).textContent = value;
  }

  function receiveContextualSuggestions(suggestions, status_msg) {
    $('contextual-suggestions-request-result').textContent = status_msg;
    displayList(
        suggestions, 'contextual-suggestions', 'contextual-hidden-toggler');
  }

  function receiveContentSuggestions(categoriesList) {
    lastSuggestions = categoriesList;
    displayList(categoriesList, 'content-suggestions', 'hidden-toggler');

    var clearCachedButtons =
        document.getElementsByClassName('submit-clear-cached-suggestions');
    for (var button of clearCachedButtons) {
      button.addEventListener('click', onClearCachedButtonClicked);
    }

    var clearDismissedButtons =
        document.getElementsByClassName('submit-clear-dismissed-suggestions');
    for (var button of clearDismissedButtons) {
      button.addEventListener('click', onClearDismissedButtonClicked);
    }

    var toggleDismissedButtons =
        document.getElementsByClassName('toggle-dismissed-suggestions');
    for (var button of toggleDismissedButtons) {
      button.addEventListener('click', onToggleDismissedButtonClicked);
    }
  }

  function onClearCachedButtonClicked(event) {
    event.preventDefault();
    var id = parseInt(event.currentTarget.getAttribute('category-id'), 10);
    chrome.send('clearCachedSuggestions', [id]);
  }

  function onClearDismissedButtonClicked(event) {
    event.preventDefault();
    var id = parseInt(event.currentTarget.getAttribute('category-id'), 10);
    chrome.send('clearDismissedSuggestions', [id]);
  }

  function onToggleDismissedButtonClicked(event) {
    event.preventDefault();
    var id = parseInt(event.currentTarget.getAttribute('category-id'), 10);
    var table = $('dismissed-suggestions-' + id);
    table.classList.toggle('hidden');
    chrome.send(
        'toggleDismissedSuggestions',
        [id, !table.classList.contains('hidden')]);
  }

  function receiveJson(json) {
    var trimmed = json.trim();
    var hasContent = (trimmed && trimmed != '{}');

    if (hasContent) {
      receiveProperty('last-json-text', trimmed);
      $('last-json').classList.remove('hidden');
    } else {
      $('last-json').classList.add('hidden');
    }
  }

  function receiveDebugLog(debugLog) {
    if (!debugLog) {
      lastDebugLog = 'empty';
    } else {
      lastDebugLog = debugLog;
    }
  }

  function receiveClassification(
      userClass, timeToOpenNTP, timeToShow, timeToUse) {
    receiveProperty('user-class', userClass);
    receiveProperty('avg-time-to-open-ntp', timeToOpenNTP);
    receiveProperty('avg-time-to-show', timeToShow);
    receiveProperty('avg-time-to-use', timeToUse);
  }

  function receiveRankerDebugData(itemsList) {
    displayList(itemsList, 'ranker', 'no-togler');
  }

  function receiveLastRemoteSuggestionsBackgroundFetchTime(
      lastRemoteSuggestionsBackgroundFetchTime) {
    receiveProperty(
        'last-background-fetch-time-label',
        lastRemoteSuggestionsBackgroundFetchTime);
  }

  function receiveWhetherSuggestionPushingPossible(possible) {
    $('push-dummy-suggestion-10-seconds-button').disabled = !possible;
  }

  function downloadJson(json) {
    // Redirect the browser to download data in |json| as a file "snippets.json"
    // (Setting Content-Disposition: attachment via a data: URL is not possible;
    // create a link with download attribute and simulate a click, instead.)
    var link = document.createElement('a');
    link.download = 'snippets.json';
    link.href = 'data:application/json,' + encodeURI(json);
    link.click();
  }

  function downloadDebugLog(debugLog) {
    // Redirect the browser to download data in |debugLog| as a file
    // "debug_log.txt" (Setting Content-Disposition: attachment via a data: URL
    // is not possible; create a link with download attribute and simulate a
    // click, instead.)
    var link = document.createElement('a');
    link.download = 'debug_log.txt';
    link.href = 'data:text/plain,' + encodeURI(debugLog);
    link.click();
  }

  function refreshContent() {
    chrome.send('refreshContent');
  }

  function toggleHidden(event) {
    var id = event.currentTarget.getAttribute('hidden-id');
    $(id).classList.toggle('hidden');
  }

  function displayList(object, domId, toggleClass) {
    jstProcess(new JsEvalContext(object), $(domId));

    var text;
    var display;

    if (object.list.length > 0) {
      text = '';
      display = 'inline';
    } else {
      text = 'The list is empty.';
      display = 'none';
    }

    if ($(domId + '-empty'))
      $(domId + '-empty').textContent = text;
    if ($(domId + '-clear'))
      $(domId + '-clear').style.display = display;

    var links = document.getElementsByClassName(toggleClass);
    for (var link of links) {
      link.addEventListener('click', toggleHidden);
    }
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    receiveProperty: receiveProperty,
    receiveContentSuggestions: receiveContentSuggestions,
    receiveJson: receiveJson,
    receiveDebugLog: receiveDebugLog,
    receiveClassification: receiveClassification,
    receiveRankerDebugData: receiveRankerDebugData,
    receiveLastRemoteSuggestionsBackgroundFetchTime:
        receiveLastRemoteSuggestionsBackgroundFetchTime,
    receiveWhetherSuggestionPushingPossible:
        receiveWhetherSuggestionPushingPossible,
    receiveContextualSuggestions: receiveContextualSuggestions,
  };
});

document.addEventListener(
    'DOMContentLoaded', chrome.SnippetsInternals.initialize);
