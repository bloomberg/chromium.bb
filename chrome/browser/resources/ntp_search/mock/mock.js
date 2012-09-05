// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(pedrosimonetti): document how to generate the data pseudo-automatically.
!/^chrome:\/\/./.test(location.href) && (function() {

  var __chrome__ = chrome;
  var shouldRegisterData = !!window.chrome && !!window.chrome.send;

  var NO_CALLBACK = 1;

  // Only messages registered in the callback map will be intercepted.
  var callbackMap = {
    'metricsHandler:logEventTime': NO_CALLBACK,
    'metricsHandler:recordInHistogram': NO_CALLBACK,
    'getApps': 'ntp.getAppsCallback',
    'getRecentlyClosedTabs': 'ntp.setRecentlyClosedTabs',
    'getMostVisited': 'ntp.setMostVisitedPages'
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
    'https---chrome.google.com-webstore-hl-en'
  ];


  //----------------------------------------------------------------------------
  // Internals
  //----------------------------------------------------------------------------

  var dataMap = {};
  var recordedDataMap = {};
  var isInitialized = false;
  var thumbnailUrlList = [];

  function initialize() {
    if (shouldRegisterData || !namespace('ntp')) {
      return;
    }
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
      if (data && i == (l - 1)) {
        object = object[name] = data;
      } else {
        object = object[name];
      }
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

  function interceptLoadData() {
    window.addEventListener('load', function() {
      recordedDataMap['__loadTimeData__'] = loadTimeData.data_;
    });
  }

  function mockGetThumbnailUrl(url) {
    url = url.replace(/[\:\/\?\=]/g, '-');

    if (thumbnailUrlList.length == 0) {
      thumbnailUrlList = copyArray(mockedThumbnailUrls);
    }
    var mockUrl;
    var index = thumbnailUrlList.indexOf(url);
    if (index != -1) {
      // Remove an element from a particular index.
      mockUrl = thumbnailUrlList.splice(index, 1);
    } else {
      // Remove the first element.
      mockUrl = thumbnailUrlList.shift();
    }

    mockUrl = 'mock/images/' + mockUrl + '.jpg';
    return mockUrl;
  }

  function mockLoadData() {
    if (loadTimeData) {
      loadTimeData.data = dataMap['__loadTimeData__'];
    }
  }


  //----------------------------------------------------------------------------
  // ChromeMock implementation
  //----------------------------------------------------------------------------

  ChromeMock = {
    mock: function(newDataMap) {
      if (newDataMap) {
        dataMap = newDataMap;
        if (!shouldRegisterData) {
          mockLoadData();
        }
      } else {
        return recordedDataMap;
      }
    },

    send: function() {
      if (!isInitialized) {
        initialize();
      }

      var message = arguments[0];
      var shouldCallChromeSend = false;

      var data;
      var callback;
      var callbackNamespace;

      if (callbackMap.hasOwnProperty(message)) {
        callbackNamespace = callbackMap[message];

        if (shouldRegisterData) {
          if (callbackNamespace !== NO_CALLBACK) {
            interceptCallback(message, callbackNamespace);
          }
        } else {
          if (dataMap.hasOwnProperty(message)) {
            data = dataMap[message];
            callback = namespace(callbackNamespace);
            setTimeout(function() {
              callback.apply(window, data);
            }, 0);
          } else {
            if (callbackNamespace !== NO_CALLBACK) {
              console.warn('No mock registered for message "%s".', message);
            }
          }
        }
      } else {
        shouldCallChromeSend = true;
        console.warn('No callback data registered for message "%s".', message);
      }

      shouldCallChromeSend = shouldCallChromeSend || shouldRegisterData;
      if (shouldCallChromeSend) {
        if (__chrome__ && __chrome__.send) {
          __chrome__.send(message);
        }
      }
    },
  };

  //----------------------------------------------------------------------------
  // Debug
  //----------------------------------------------------------------------------

  var debugArgs = {};
  var debugStylesheet = null;
  var animationSelectorSpeedMap = {
    '#card-slider-frame': 250,
    '.dot': 200,
    '.animate-page-height': 200,
    '.animate-grid-width': 200,
    '.tile-grid-content': 200,
    '.tile-row': 200,
    '.animate-grid-width .tile-cell': 200
  };

  function adjustAnimationSpeed(slownessFactor) {
    slownessFactor = slownessFactor || 1;

    var animationRules = [];
    for (var selector in animationSelectorSpeedMap) {
      if (animationSelectorSpeedMap.hasOwnProperty(selector)) {
        animationRules.push(selector + ' { -webkit-transition-duration: ' +
            Math.round(animationSelectorSpeedMap[selector] * slownessFactor) +
            'ms !important; }\n');
      }
    }

    var doc = document;
    debugStylesheet = doc.getElementById('debugStylesheet');
    if (debugStylesheet) {
      debugStylesheet.parentElement.removeChild(debugStylesheet);
    }
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
            parseInt(value) ? parseInt(value) : value;
      }
    }
  }

  window.addEventListener('load', function() {
    getArgs();

    if (debugArgs.debug)
      document.body.classList.add('debug');

    if (debugArgs.slownessFactor)
      adjustAnimationSpeed(debugArgs.slownessFactor);
  });

  //----------------------------------------------------------------------------
  // ChromeMock initialization
  //----------------------------------------------------------------------------

  if (shouldRegisterData) {
    interceptLoadData();
  }

  window.chrome = ChromeMock;
})();
