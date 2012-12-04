// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// If you set this variable to true, then compile and run Chrome, all messages
// sent with chrome.send() will be intercepted and their callback data will be
// recorded. You can later see the recorded data by executing chrome.mock()
// on Web Developer Tools' console.
var recordMockData = false;

(recordMockData || !/^chrome:\/\/./.test(location.href)) && (function() {

  var __chrome__ = chrome;
  var shouldRegisterData = !!window.chrome && !!window.chrome.send;

  var NO_CALLBACK = 1;

  // Only messages registered in the callback map will be intercepted.
  var callbackMap = {
    'appRemoved': 'ntp.appRemoved',
    'blacklistURLFromMostVisited': NO_CALLBACK,
    'clearMostVisitedURLsBlacklist': NO_CALLBACK,
    'getApps': 'ntp.getAppsCallback',
    'getForeignSessions': 'ntp.setForeignSessions',
    'getMostVisited': 'ntp.setMostVisitedPages',
    'getRecentlyClosedTabs': 'ntp.setRecentlyClosedTabs',
    'metricsHandler:logEventTime': NO_CALLBACK,
    'metricsHandler:recordInHistogram': NO_CALLBACK,
    'removeURLsFromMostVisitedBlacklist': NO_CALLBACK,
    'uninstallApp': NO_CALLBACK,
  };

  // TODO(pedrosimonetti): include automatically in the recorded data
  // TODO(pedrosimonetti): document the special char replacement
  var mockedThumbnailUrls = [
    'http---www.wikipedia.org-',
    'http---www.deviantart.com-',
    'http---wefunkradio.com-',
    'http---youtube.com-',
    'http---amazon.com-',
    'http---nytimes.com-',
    'http---news.ycombinator.com-',
    'http---cnn.com-',
    'http---ebay.com-',
    'http---www.google.com-chrome-intl-en-welcome.html',
    'https---chrome.google.com-webstore-hl-en',
  ];

  //----------------------------------------------------------------------------
  // Internals
  //----------------------------------------------------------------------------

  var dataMap = {};
  var recordedDataMap = {};
  var isInitialized = false;

  function initialize() {
    if (shouldRegisterData || !namespace('ntp'))
      return;

    isInitialized = true;
    namespace('ntp.getThumbnailUrl', mockGetThumbnailUrl);

    var data = loadTimeData.data_;
    document.documentElement.dir = data.textdirection;
    document.body.style.fontSize = data.fontsize;
  }

  function namespace(str, data) {
    var ns = str.split('.'), name, object = window;
    for (var i = 0, l = ns.length; i < l; i++) {
      name = ns[i];
      if (data && i == (l - 1))
        object = object[name] = data;
      else
        object = object[name];
    }
    return object == window ? null : object;
  }

  function copyArray(arr) {
    return Array.prototype.slice.call(arr, 0);
  }

  function interceptCallback(message, callbackNamespace) {
    var original = namespace(callbackNamespace);
    namespace(callbackNamespace, function() {
      recordedDataMap[message] = copyArray(arguments);
      var result = original.apply(window, arguments);
      namespace(callbackNamespace, original);
      return result;
    });
  }

  function dispatchCallbackForMessage(message) {
    var callbackNamespace = callbackMap[message];
    var callback = namespace(callbackNamespace);
    var data = dataMap[message];
    var filter = filterMap[message];
    if (filter)
      data = filter(data);
    callback.apply(window, data);
  }

  function interceptLoadData() {
    window.addEventListener('load', function() {
      recordedDataMap['__loadTimeData__'] = loadTimeData.data_;
    });
  }

  function mockGetThumbnailUrl(url) {
    url = url.replace(/[:\/\?=]/g, '-');

    var mockUrl;
    var index = mockedThumbnailUrls.indexOf(url);
    if (index != -1)
      mockUrl = mockedThumbnailUrls[index];
    else
      mockUrl = 'non-existent-file-name';

    mockUrl = 'mock/images/' + mockUrl + '.jpg';
    return mockUrl;
  }

  function mockLoadData() {
    if (loadTimeData)
      loadTimeData.data = dataMap['__loadTimeData__'];
  }

  function getTime(time) {
    var slownessFactor = debugArgs.slownessFactor || 1;
    return Math.round(time * slownessFactor);
  }

  //----------------------------------------------------------------------------
  // ChromeMock implementation
  //----------------------------------------------------------------------------

  var ChromeMock = {
    get debugArgs() {
      return debugArgs;
    },

    mock: function(newDataMap) {
      if (newDataMap) {
        dataMap = newDataMap;
        if (!shouldRegisterData)
          mockLoadData();
      } else {
        return recordedDataMap;
      }
    },

    send: function() {
      if (!isInitialized)
        initialize();

      var message = arguments[0];
      var shouldCallChromeSend = false;

      if (callbackMap.hasOwnProperty(message)) {
        var callbackNamespace = callbackMap[message];

        if (shouldRegisterData) {
          if (callbackNamespace !== NO_CALLBACK)
            interceptCallback(message, callbackNamespace);
        } else {
          if (dataMap.hasOwnProperty(message)) {
            var data = dataMap[message];
            var callback = namespace(callbackNamespace);

            if (filterMap.hasOwnProperty(message))
              data = filterMap[message](data);

            setTimeout(function() {
              callback.apply(window, data);
            }, 0);
          } else {
            if (callbackNamespace !== NO_CALLBACK)
              console.warn('No mock registered for message "%s".', message);
            else if (serverCallbackMap.hasOwnProperty(message))
              serverCallbackMap[message](arguments[1]);
          }
        }
      } else {
        shouldCallChromeSend = true;
        console.warn('No callback data registered for message "%s".', message);
      }

      shouldCallChromeSend = shouldCallChromeSend || shouldRegisterData;
      if (shouldCallChromeSend) {
        if (__chrome__ && __chrome__.send)
          __chrome__.send(message);
      }
    },
  };

  //----------------------------------------------------------------------------
  // C++ mock implementation
  //----------------------------------------------------------------------------

  var mostVisitedBlackList = {};

  var filterMap = {
    getMostVisited: function(data) {
      var filtered = [];
      var list = data[0];
      var hasBlacklistedUrls = false;
      for (var i = 0, length = list.length; i < length; i++) {
        if (mostVisitedBlackList.hasOwnProperty('' + i))
          hasBlacklistedUrls = true;
        else
          filtered.push(list[i]);
      }
      return [filtered, hasBlacklistedUrls];
    }
  };

  var serverCallbackMap = {
    blacklistURLFromMostVisited: function(urls) {
      var url = urls[0];
      var data = dataMap['getMostVisited'][0];
      for (var i = 0, length = data.length; i < length; i++) {
        if (data[i].url == url)
          mostVisitedBlackList['' + i] = 1;
      }
      dispatchCallbackForMessage('getMostVisited');
    },

    removeURLsFromMostVisitedBlacklist: function(urls) {
      var url = urls[0];
      var data = dataMap['getMostVisited'][0];
      for (var i = 0, length = data.length; i < length; i++) {
        if (data[i].url == url)
          delete mostVisitedBlackList['' + i];
      }
      dispatchCallbackForMessage('getMostVisited');
    },

    clearMostVisitedURLsBlacklist: function() {
      mostVisitedBlackList = {};
      dispatchCallbackForMessage('getMostVisited');
    },

    uninstallApp: function(id) {
      var appData;
      var data = dataMap['getApps'][0].apps;
      for (var i = 0, length = data.length; i < length; i++) {
        if (data[i].id == id) {
          appData = data[i];
          data.splice(i, 1);
          break;
        }
      }
      assert(appData);
      dataMap['appRemoved'] = [appData, true, true];
      dispatchCallbackForMessage('appRemoved');
    },
  };

  //----------------------------------------------------------------------------
  // Debug
  //----------------------------------------------------------------------------

  var debugArgs = {
    debug: false,
    slownessFactor: null,
  };

  var debugStylesheet = null;
  var selectorDurationMap = {
    '.animate-grid-width': 200,
    '.animate-grid-width .tile-cell': 200,
    '.animate-tile-repositioning .tile': 200,
    '.animate-tile-repositioning .tile:not(.target-tile)': 400,
    '#bottom-panel': 200,
    '.dot': 200,
    '.tile-grid-content': 200,
    '.tile-row': 200,
  };

  var selectorDurationImportantMap = {
  };

  var selectorDelayMap = {
    '.animate-tile-repositioning.undo-removal .target-tile': 200,
  };

  function adjustAnimationSpeed() {
    var animationRules = [];
    for (var selector in selectorDurationMap) {
      if (selectorDurationMap.hasOwnProperty(selector)) {
        animationRules.push(selector + ' { -webkit-transition-duration: ' +
            getTime(selectorDurationMap[selector]) + 'ms; }\n');
      }
    }

    for (var selector in selectorDurationImportantMap) {
      if (selectorDurationImportantMap.hasOwnProperty(selector)) {
        animationRules.push(selector + ' { -webkit-transition-duration: ' +
            getTime(selectorDurationImportantMap[selector]) +
            'ms !important; }\n');
      }
    }

    for (var selector in selectorDelayMap) {
      if (selectorDelayMap.hasOwnProperty(selector)) {
        animationRules.push(selector + ' { -webkit-transition-delay: ' +
            getTime(selectorDelayMap[selector]) + 'ms; }\n');
      }
    }

    var doc = document;
    debugStylesheet = doc.getElementById('debugStylesheet');
    if (debugStylesheet)
      debugStylesheet.parentElement.removeChild(debugStylesheet);

    debugStylesheet = doc.createElement('style');
    debugStylesheet.id = 'debugStylesheet';
    debugStylesheet.textContent = animationRules.join('');
    doc.querySelector('head').appendChild(debugStylesheet);
  }

  function getArgs() {
    var url = location.href;
    var parts = url.split('?');
    var args = parts[1];
    if (args) {
      var pairs = args.split('&');
      for (var i = 0, l = pairs.length; i < l; i++) {
        var pair = pairs[i];
        var data = pair.split('=');
        var key = data[0];
        var value = data[1];
        debugArgs[key] = typeof value == 'undefined' ? true :
            parseFloat(value) ? parseFloat(value) : value;
      }
    }
  }

  window.addEventListener('load', function() {
    getArgs();

    if (debugArgs.debug)
      document.body.classList.add('debug');

    if (debugArgs.background)
      document.body.style.background = debugArgs.background;

    var slownessFactor = debugArgs.slownessFactor;
    if (slownessFactor)
      adjustAnimationSpeed();
  });

  //----------------------------------------------------------------------------
  // ChromeMock initialization
  //----------------------------------------------------------------------------

  if (shouldRegisterData)
    interceptLoadData();

  window.chrome = ChromeMock;
})();
