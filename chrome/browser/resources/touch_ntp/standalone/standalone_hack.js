// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 *  @fileoverview NTP Standalone hack
 *  This file contains the code necessary to make the Touch NTP work
 *  as a stand-alone application (as opposed to being embedded into chrome).
 *  This is useful for rapid development and testing, but does not actually form
 *  part of the product.
 *
 *  Note that, while the product portion of the touch NTP is designed to work
 *  just in the latest version of Chrome, this hack attempts to add some support
 *  for working in older browsers to enable testing and demonstration on
 *  existing tablet platforms.  In particular, this code has been tested to work
 *  on Mobile Safari in iOS 4.2.  The goal is that the need to support any other
 *  browser should not leak out of this file - and so we will hack global JS
 *  objects as necessary here to present the illusion of running on the latest
 *  version of Chrome.
 */

// Note that this file never gets concatenated and embeded into Chrome, so we
// can enable strict mode for the whole file just like normal.
'use strict';


/**
 * For non-Chrome browsers, create a dummy chrome object
 */
if (!window.chrome) {
  var chrome = {};
}


/**
 *  A replacement chrome.send method that supplies static data for the
 *  key APIs used by the NTP.
 *
 *  Note that the real chrome object also supplies data for most-viewed and
 *  recently-closed pages, but the tangent NTP doesn't use that data so we
 *  don't bother simulating it here.
 *
 *  We create this object by applying an anonymous function so that we can have
 *  local variables (avoid polluting the global object)
 */
chrome.send = (function() {
  var apps = [{
    app_launch_index: 2,
    description: 'The prickly puzzle game where popping balloons has ' +
        'never been so much fun!',
    icon_big: 'standalone/poppit-icon.png',
    icon_small: 'standalone/poppit-favicon.png',
    id: 'mcbkbpnkkkipelfledbfocopglifcfmi',
    launch_container: 2,
    launch_type: 1,
    launch_url: 'http://poppit.pogo.com/hd/PoppitHD.html',
    name: 'Poppit',
    options_url: ''
  },
  {
    app_launch_index: 1,
    description: 'Fast, searchable email with less spam.',
    icon_big: 'standalone/gmail-icon.png',
    icon_small: 'standalone/gmail-favicon.png',
    id: 'pjkljhegncpnkpknbcohdijeoejaedia',
    launch_container: 2,
    launch_type: 1,
    launch_url: 'https://mail.google.com/',
    name: 'Gmail',
    options_url: 'https://mail.google.com/mail/#settings'
  },
  {
    app_launch_index: 3,
    description: 'Read over 3 million Google eBooks on the web.',
    icon_big: 'standalone/googlebooks-icon.png',
    icon_small: 'standalone/googlebooks-favicon.png',
    id: 'mmimngoggfoobjdlefbcabngfnmieonb',
    launch_container: 2,
    launch_type: 1,
    launch_url: 'http://books.google.com/ebooks?source=chrome-app',
    name: 'Google Books',
    options_url: ''
  },
  {
    app_launch_index: 4,
    description: 'Find local business information, directions, and ' +
        'street-level imagery around the world with Google Maps.',
    icon_big: 'standalone/googlemaps-icon.png',
    icon_small: 'standalone/googlemaps-favicon.png',
    id: 'lneaknkopdijkpnocmklfnjbeapigfbh',
    launch_container: 2,
    launch_type: 1,
    launch_url: 'http://maps.google.com/',
    name: 'Google Maps',
    options_url: ''
  },
  {
    app_launch_index: 5,
    description: 'Create the longest path possible and challenge your ' +
        'friends in the game of Entanglement.',
    icon_big: 'standalone/entaglement-icon.png',
    id: 'aciahcmjmecflokailenpkdchphgkefd',
    launch_container: 2,
    launch_type: 1,
    launch_url: 'http://entanglement.gopherwoodstudios.com/',
    name: 'Entanglement',
    options_url: ''
  },
  {
    name: 'NYTimes',
    app_launch_index: 6,
    description: 'The New York Times App for the Chrome Web Store.',
    icon_big: 'standalone/nytimes-icon.png',
    id: 'ecmphppfkcfflgglcokcbdkofpfegoel',
    launch_container: 2,
    launch_type: 1,
    launch_url: 'http://www.nytimes.com/chrome/',
    options_url: '',
    page_index: 2
  },
  {
    app_launch_index: 7,
    description: 'The world\'s most popular online video community.',
    id: 'blpcfgokakmgnkcojhhkbfbldkacnbeo',
    icon_big: 'standalone/youtube-icon.png',
    launch_container: 2,
    launch_type: 1,
    launch_url: 'http://www.youtube.com/',
    name: 'YouTube',
    options_url: '',
    page_index: 3
  }];

  // For testing
  apps = spamApps(apps);

  /**
   * Invoke the getAppsCallback function with a snapshot of the current app
   * database.
   */
  function sendGetAppsCallback()
  {
    // We don't want to hand out our array directly because the NTP will
    // assume it owns the array and is free to modify it.  For now we make a
    // one-level deep copy of the array (since cloning the whole thing is
    // more work and unnecessary at the moment).
    var appsData = {
      showPromo: false,
      showLauncher: true,
      apps: apps.slice(0)
    };
    getAppsCallback(appsData);
  }

  /**
   * To make testing real-world scenarios easier, this expands our list of
   * apps by duplicating them a number of times
   */
  function spamApps(apps)
  {
    // Create an object that extends another object
    // This is an easy/efficient way to make slightly modified copies of our
    // app objects without having to do a deep copy
    function createObject(proto) {
      /** @constructor */
      var F = function() {};
      F.prototype = proto;
      return new F();
    }

    var newApps = [];
    var pages = Math.floor(Math.random() * 8) + 1;
    var idx = 1;
    for (var p = 0; p < pages; p++) {
      var count = Math.floor(Math.random() * 18) + 1;
      for (var a = 0; a < count; a++) {
        var i = Math.floor(Math.random() * apps.length);
        var newApp = createObject(apps[i]);
        newApp.page_index = p;
        newApp.app_launch_index = idx;
        // Uniqify the ID
        newApp.id = apps[i].id + '-' + idx;
        idx++;
        newApps.push(newApp);
      }
    }
    return newApps;
  }

  /**
   * Like Array.prototype.indexOf but calls a predicate to test for match
   *
   * @param {Array} array The array to search.
   * @param {function(Object): boolean} predicate The function to invoke on
   *     each element.
   * @return {number} First index at which predicate returned true, or -1.
   */
  function indexOfPred(array, predicate) {
    for (var i = 0; i < array.length; i++) {
      if (predicate(array[i]))
        return i;
    }
    return -1;
  }

  /**
   * Get index into apps of an application object
   * Requires the specified app to be present
   *
   * @param {string} id The ID of the application to locate.
   * @return {number} The index in apps for an object with the specified ID.
   */
  function getAppIndex(id) {
    var i = indexOfPred(apps, function(e) { return e.id === id;});
    if (i == -1)
      alert('Error: got unexpected App ID');
    return i;
  }

  /**
   * Get an application object given the application ID
   * Requires
   * @param {string} id The application ID to search for.
   * @return {Object} The corresponding application object.
   */
  function getApp(id) {
    return apps[getAppIndex(id)];
  }

  /**
   * Simlulate the launching of an application
   *
   * @param {string} id The ID of the application to launch.
   */
  function launchApp(id) {
    // Note that we don't do anything with the icon location.
    // That's used by Chrome only on Windows to animate the icon during
    // launch.
    var app = getApp(id);
    switch (parseInt(app.launch_type, 10)) {
      case 0: // pinned
      case 1: // regular
        // Replace the current tab with the app.
        // Pinned seems to omit the tab title, but I doubt it's
        // possible for us to do that here
        window.location = (app.launch_url);
        break;

      case 2: // fullscreen
      case 3: // window
        // attempt to launch in a new window
        window.close();
        window.open(app.launch_url, app.name,
            'resizable=yes,scrollbars=yes,status=yes');
        break;

      default:
        alert('Unexpected launch type: ' + app.launch_type);
    }
  }

  /**
   * Simulate uninstall of an app
   * @param {string} id The ID of the application to uninstall.
   */
  function uninstallApp(id) {
    var i = getAppIndex(id);
    // This confirmation dialog doesn't look exactly the same as the
    // standard NTP one, but it's close enough.
    if (window.confirm('Uninstall \"' + apps[i].name + '\"?')) {
      apps.splice(i, 1);
      sendGetAppsCallback();
    }
  }

  /**
   * Update the app_launch_index of all apps
   * @param {Array.<string>} appIds All app IDs in their desired order.
   */
  function reorderApps(movedAppId, appIds) {
    assert(apps.length == appIds.length, 'Expected all apps in reorderApps');

    // Clear the launch indicies so we can easily verify no dups
    apps.forEach(function(a) {
      a.app_launch_index = -1;
    });

    for (var i = 0; i < appIds.length; i++) {
      var a = getApp(appIds[i]);
      assert(a.app_launch_index == -1,
             'Found duplicate appId in reorderApps');
      a.app_launch_index = i;
    }
    sendGetAppsCallback();
  }

  /**
   * Update the page number of an app
   * @param {string} id The ID of the application to move.
   * @param {number} page The page index to place the app.
   */
  function setPageIndex(id, page) {
    var app = getApp(id);
    app.page_index = page;
  }

  // The 'send' function
  /**
   * The chrome server communication entrypoint.
   *
   * @param {string} command Name of the command to send.
   * @param {Array} args Array of command-specific arguments.
   */
  return function(command, args) {
    // Chrome API is async
    window.setTimeout(function() {
      switch (command) {
        // called to populate the list of applications
        case 'getApps':
          sendGetAppsCallback();
          break;

        // Called when an app is launched
        // Ignore additional arguments - they've been changing over time and
        // we don't use them in our NTP anyway.
        case 'launchApp':
          launchApp(args[0]);
          break;

        // Called when an app is uninstalled
        case 'uninstallApp':
          uninstallApp(args[0]);
          break;

        // Called when an app is repositioned in the touch NTP
        case 'reorderApps':
          reorderApps(args[0], args[1]);
          break;

        // Called when an app is moved to a different page
        case 'setPageIndex':
          setPageIndex(args[0], parseInt(args[1], 10));
          break;

        default:
          throw new Error('Unexpected chrome command: ' + command);
          break;
      }
    }, 0);
  };
})();

/* A static templateData with english resources */
var templateData = {
  title: 'Standalone New Tab',
  web_store_title: 'Web Store',
  web_store_url: 'https://chrome.google.com/webstore?hl=en-US'
};

/* Hook construction of chrome://theme URLs */
function themeUrlMapper(resourceName) {
  if (resourceName == 'IDR_WEBSTORE_ICON') {
    return 'standalone/webstore_icon.png';
  }
  return undefined;
}

/*
 * On iOS we need a hack to avoid spurious click events
 * In particular, if the user delays briefly between first touching and starting
 * to drag, when the user releases a click event will be generated.
 * Note that this seems to happen regardless of whether we do preventDefault on
 * touchmove events.
 */
if (/iPhone|iPod|iPad/.test(navigator.userAgent) &&
    !(/Chrome/.test(navigator.userAgent))) {
  // We have a real iOS device (no a ChromeOS device pretending to be iOS)
  (function() {
    // True if a gesture is occuring that should cause clicks to be swallowed
    var gestureActive = false;

    // The position a touch was last started
    var lastTouchStartPosition;

    // Distance which a touch needs to move to be considered a drag
    var DRAG_DISTANCE = 3;

    document.addEventListener('touchstart', function(event) {
      lastTouchStartPosition = {
        x: event.touches[0].clientX,
        y: event.touches[0].clientY
      };
      // A touchstart ALWAYS preceeds a click (valid or not), so cancel any
      // outstanding gesture. Also, any multi-touch is a gesture that should
      // prevent clicks.
      gestureActive = event.touches.length > 1;
    }, true);

    document.addEventListener('touchmove', function(event) {
      // When we see a move, measure the distance from the last touchStart
      // If this is a multi-touch then the work here is irrelevant
      // (gestureActive is already true)
      var t = event.touches[0];
      if (Math.abs(t.clientX - lastTouchStartPosition.x) > DRAG_DISTANCE ||
          Math.abs(t.clientY - lastTouchStartPosition.y) > DRAG_DISTANCE) {
        gestureActive = true;
      }
    }, true);

    document.addEventListener('click', function(event) {
      // If we got here without gestureActive being set then it means we had
      // a touchStart without any real dragging before touchEnd - we can allow
      // the click to proceed.
      if (gestureActive) {
        event.preventDefault();
        event.stopPropagation();
      }
    }, true);
  })();
}

/*  Hack to add Element.classList to older browsers that don't yet support it.
    From https://developer.mozilla.org/en/DOM/element.classList.
*/
if (typeof Element !== 'undefined' &&
    !Element.prototype.hasOwnProperty('classList')) {
  (function() {
    var classListProp = 'classList',
        protoProp = 'prototype',
        elemCtrProto = Element[protoProp],
        objCtr = Object,
        strTrim = String[protoProp].trim || function() {
          return this.replace(/^\s+|\s+$/g, '');
        },
        arrIndexOf = Array[protoProp].indexOf || function(item) {
          for (var i = 0, len = this.length; i < len; i++) {
            if (i in this && this[i] === item) {
              return i;
            }
          }
          return -1;
        },
        // Vendors: please allow content code to instantiate DOMExceptions
        /** @constructor  */
        DOMEx = function(type, message) {
          this.name = type;
          this.code = DOMException[type];
          this.message = message;
        },
        checkTokenAndGetIndex = function(classList, token) {
          if (token === '') {
            throw new DOMEx(
                'SYNTAX_ERR',
                'An invalid or illegal string was specified'
            );
          }
          if (/\s/.test(token)) {
            throw new DOMEx(
                'INVALID_CHARACTER_ERR',
                'String contains an invalid character'
            );
          }
          return arrIndexOf.call(classList, token);
        },
        /** @constructor
         *  @extends {Array} */
        ClassList = function(elem) {
          var trimmedClasses = strTrim.call(elem.className),
              classes = trimmedClasses ? trimmedClasses.split(/\s+/) : [];

          for (var i = 0, len = classes.length; i < len; i++) {
            this.push(classes[i]);
          }
          this._updateClassName = function() {
            elem.className = this.toString();
          };
        },
        classListProto = ClassList[protoProp] = [],
        classListGetter = function() {
          return new ClassList(this);
        };

    // Most DOMException implementations don't allow calling DOMException's
    // toString() on non-DOMExceptions. Error's toString() is sufficient here.
    DOMEx[protoProp] = Error[protoProp];
    classListProto.item = function(i) {
      return this[i] || null;
    };
    classListProto.contains = function(token) {
      token += '';
      return checkTokenAndGetIndex(this, token) !== -1;
    };
    classListProto.add = function(token) {
      token += '';
      if (checkTokenAndGetIndex(this, token) === -1) {
        this.push(token);
        this._updateClassName();
      }
    };
    classListProto.remove = function(token) {
      token += '';
      var index = checkTokenAndGetIndex(this, token);
      if (index !== -1) {
        this.splice(index, 1);
        this._updateClassName();
      }
    };
    classListProto.toggle = function(token) {
      token += '';
      if (checkTokenAndGetIndex(this, token) === -1) {
        this.add(token);
      } else {
        this.remove(token);
      }
    };
    classListProto.toString = function() {
      return this.join(' ');
    };

    if (objCtr.defineProperty) {
      var classListDescriptor = {
        get: classListGetter,
        enumerable: true,
        configurable: true
      };
      objCtr.defineProperty(elemCtrProto, classListProp, classListDescriptor);
    } else if (objCtr[protoProp].__defineGetter__) {
      elemCtrProto.__defineGetter__(classListProp, classListGetter);
    }
  }());
}

/* Hack to add Function.bind to older browsers that don't yet support it. From:
   https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Function/bind
*/
if (!Function.prototype.bind) {
  /**
   * @param {Object} selfObj Specifies the object which |this| should
   *     point to when the function is run. If the value is null or undefined,
   *     it will default to the global object.
   * @param {...*} var_args Additional arguments that are partially
   *     applied to the function.
   * @return {!Function} A partially-applied form of the function bind() was
   *     invoked as a method of.
   *  @suppress {duplicate}
   */
  Function.prototype.bind = function(selfObj, var_args) {
    var slice = [].slice,
        args = slice.call(arguments, 1),
        self = this,
        /** @constructor  */
        nop = function() {},
        bound = function() {
          return self.apply(this instanceof nop ? this : (selfObj || {}),
                              args.concat(slice.call(arguments)));
        };
    nop.prototype = self.prototype;
    bound.prototype = new nop();
    return bound;
  };
}

