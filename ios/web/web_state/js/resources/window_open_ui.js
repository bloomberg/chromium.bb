// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scripts that are conceptually part of core.js, but have UIWebView-specific
// details/behaviors.

goog.provide('__crWeb.windowOpen');

goog.require('__crWeb.core');

// Namespace for this module.
__gCrWeb.windowOpen = {};

// Beginning of anonymous object.
(function() {
  // Preserve a reference to the original window.open method.
  __gCrWeb['originalWindowOpen'] = window.open;

  // Object used to keep track of all windows opened from this window.
  var openedWindows = {};

  /**
   * Checks if a child window exists with the given name and if so, sets its
   * closed property to true and removes it from |openedWindows|.
   * @param {String} windowName The name of the window to mark as closed.
   */
  __gCrWeb['windowClosed'] = function(windowName) {
    if (openedWindows.hasOwnProperty(windowName)) {
      openedWindows[windowName].closed = true;
      delete openedWindows[windowName];
    }
  };

  function invokeOnHost_(command) {
    __gCrWeb.message.invokeOnHost(command);
  };

  var invokeNotImplementedOnHost_ = function(methodName) {
    invokeOnHost_({'command': 'window.error',
                   'message': methodName + ' is not implemented'});
  };

  // Define Object watch/unwatch functions to detect assignments to
  // certain object properties. Handles defineProperty case only because
  // this code runs in UIWebView (i.e. Safari).
  var objectWatch = function(obj, prop, handler) {
    var val = obj[prop];
    if (delete obj[prop]) {
      Object['defineProperty'](obj, prop, {
        'get': function() {
          return val;
        },
        'set': function(newVal) {
          return val = handler.call(obj, prop, val, newVal);
        }
      });
    }
  };

  /**
   * Creates and returns a window proxy used to represent the window object and
   * intercept calls made on it.
   * @param {String} target The name of the window.
   * @return {Object} A window proxy object for intercepting window methods.
   * @private
   */
  var createWindowProxy_ = function(target) {
    // Create return window object.
    // 'name' is always the original supplied name.
    var windowProxy = {name: target};

    // Define window object methods.
    windowProxy.alert = function() {
      invokeNotImplementedOnHost_('windowProxy.alert');
    };

    windowProxy.blur = function() {
      invokeNotImplementedOnHost_('windowProxy.blur');
    };

    windowProxy.clearInterval = function() {
      invokeNotImplementedOnHost_('windowProxy.clearInterval');
    };

    windowProxy.clearTimeout = function() {
      invokeNotImplementedOnHost_('windowProxy.clearTimeout');
    };

    windowProxy.close = function() {
      invokeOnHost_({'command': 'window.close',
                     'target': target});
    };

    windowProxy.confirm = function() {
      invokeNotImplementedOnHost_('windowProxy.confirm');
    };

    windowProxy.createPopup = function() {
      invokeNotImplementedOnHost_('windowProxy.createPopup');
    };

    windowProxy.focus = function() {
      // Noop as the opened window always gets focus.
    };

    windowProxy.moveBy = function() {
      invokeNotImplementedOnHost_('windowProxy.moveBy');
    };

    windowProxy.moveTo = function() {
      invokeNotImplementedOnHost_('windowProxy.moveTo');
    };

    windowProxy.stop = function() {
      invokeOnHost_({'command': 'window.stop',
                     'target': target});
    };

    windowProxy.open = function() {
      invokeNotImplementedOnHost_('windowProxy.open');
    };

    windowProxy.print = function() {
      invokeNotImplementedOnHost_('windowProxy.print');
    };

    windowProxy.prompt = function() {
      invokeNotImplementedOnHost_('windowProxy.prompt');
    };

    windowProxy.resizeBy = function() {
      invokeNotImplementedOnHost_('windowProxy.resizeBy');
    };

    windowProxy.resizeTo = function() {
      invokeNotImplementedOnHost_('windowProxy.resizeTo');
    };

    windowProxy.scroll = function() {
      invokeNotImplementedOnHost_('windowProxy.scroll');
    };

    windowProxy.scrollBy = function() {
      invokeNotImplementedOnHost_('windowProxy.scrollBy');
    };

    windowProxy.scrollTo = function() {
      invokeNotImplementedOnHost_('windowProxy.scrollTo');
    };

    windowProxy.setInterval = function() {
      invokeNotImplementedOnHost_('windowProxy.setInterval');
    };

    windowProxy.setTimeout = function() {
      invokeNotImplementedOnHost_('windowProxy.setTimeout');
    };

    // Define window object properties.
    // The current window.
    windowProxy.self = windowProxy;
    // The topmost browser window.
    windowProxy.top = windowProxy;

    // Provide proxy document which supplies one method, document.write().
    windowProxy.document = {};
    windowProxy.document.title = '';
    windowProxy.document.write = function(html) {
      invokeOnHost_({'command': 'window.document.write',
                       'html': html,
                     'target': target});
    };

    windowProxy.document.open = function() {
      // The open() method should open an output stream to collect the output
      // from any document.write() or document.writeln() methods.
      invokeNotImplementedOnHost_('windowProxy.document.open');
    };

    windowProxy.document.close = function() {
      // The close() method should close the output stream previously opened
      // with the document.open() method, and displays the collected data in
      // this process.
      invokeNotImplementedOnHost_('windowProxy.document.close');
    };

    windowProxy.location = {};
    windowProxy.location.assign = function(url) {
      windowProxy.location = url;
    };
    // Watch assignments to window.location and window.location.href.
    // Invoke equivalent method in ObjC code.
    var onWindowProxyLocationChange = function(prop, oldVal, newVal) {
      invokeOnHost_({'command': 'window.location',
                     'value': __gCrWeb['getFullyQualifiedURL'](newVal),
                     'target': target});
      return newVal;
    };
    objectWatch(windowProxy, 'location', onWindowProxyLocationChange);
    objectWatch(windowProxy.location, 'href', onWindowProxyLocationChange);
    windowProxy.closed = false;

    return windowProxy;
  };

  // Intercept window.open calls.
  window.open = function(url, target, features) {
    if (target == '_parent' || target == '_self' || target == '_top') {
      return __gCrWeb['originalWindowOpen'].call(window, url, target, features);
    }

    // Because of the difficulty of returning data from JS->ObjC calls, in the
    // event of a blank window name the JS side chooses a pseudo-GUID to
    // use as the window name which is passed to ObjC and mapped to the real
    // Tab there.
    var isTargetBlank = (typeof target == 'undefined' || target == '_blank' ||
                         target == '' || target == null);
    if (isTargetBlank) {
      target = '' + Date.now() + '-' + Math.random();
    }

    if (typeof(url) == 'undefined') {
      // W3C recommended behavior.
      url = 'about:blank';
    }

    invokeOnHost_({
      'command': 'window.open',
      'target': target,
      'url': url,
      'referrerPolicy': __gCrWeb.getPageReferrerPolicy()
    });

    // Create a new |windowProxy| if none already exists with |target| as its
    // name.
    var windowProxy;
    if (openedWindows.hasOwnProperty(target)) {
      windowProxy = openedWindows[target];
    } else {
      windowProxy = createWindowProxy_(target);
      openedWindows[target] = windowProxy;
    }
    return windowProxy;
  };

}());  // End of anonymous object
