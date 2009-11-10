// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview CFInstall.js provides a set of utilities for managing
 * the Chrome Frame detection and installation process.
 * @author slightlyoff@google.com (Alex Russell)
 */

(function(scope) {
  // bail if we'd be over-writing an existing CFInstall object
  if (scope['CFInstall']) {
    return;
  }

  /**
   * returns an item based on DOM ID. Optionally a document may be provided to
   * specify the scope to search in. If a node is passed, it's returned as-is.
   * @param {string|Node} id The ID of the node to be located or a node
   * @param {Node} doc Optional A document to search for id.
   * @return {Node}
   */
  var byId = function(id, doc) {
    return (typeof id == 'string') ? (doc || document).getElementById(id) : id;
  };

  /////////////////////////////////////////////////////////////////////////////
  // Plugin Detection
  /////////////////////////////////////////////////////////////////////////////

  /**
   * Checks to find out if ChromeFrame is available as a plugin
   * @return {Boolean}
   */
  var isAvailable = function() {
    // For testing purposes.
    if (scope.CFInstall._force) {
      return scope.CFInstall._forceValue;
    }

    // Look for CF in the User Agent before trying more expensive checks
    var ua = navigator.userAgent.toLowerCase();
    if (ua.indexOf("chromeframe") >= 0) {
      return true;
    }

    if (typeof window['ActiveXObject'] != 'undefined') {
      try {
        var obj = new ActiveXObject('ChromeTab.ChromeFrame');
        if (obj) {
          return true;
        }
      } catch(e) {
        // squelch
      }
    }
    return false;
  };

  /**
   * Creates a style sheet in the document containing the passed rules.
   */
  var injectStyleSheet = function(rules) {
    try {
      var ss = document.createElement('style');
      ss.setAttribute('type', 'text/css');
      if (ss.styleSheet) {
        ss.styleSheet.cssText = rules;
      } else {
        ss.appendChild(document.createTextNode(rules));
      }
      var h = document.getElementsByTagName('head')[0];
      var firstChild = h.firstChild;
      h.insertBefore(ss, firstChild);
    } catch (e) {
      // squelch
    }
  };

  /** @type {boolean} */
  var cfStyleTagInjected = false;
  /** @type {boolean} */
  var cfHiddenInjected = false;

  /**
   * Injects style rules into the document to handle formatting of Chrome Frame
   * prompt. Multiple calls have no effect.
   */
  var injectCFStyleTag = function() {
    if (cfStyleTagInjected) {
      // Once and only once
      return;
    }
    var rules = '.chromeFrameInstallDefaultStyle {' +
                   'width: 800px;' +
                   'height: 600px;' +
                   'position: absolute;' +
                   'left: 50%;' +
                   'top: 50%;' +
                   'margin-left: -400px;' +
                   'margin-top: -300px;' +
                 '}' +
                 '.chromeFrameOverlayContent {' +
                   'position: absolute;' +
                   'margin-left: -400px;' +
                   'margin-top: -300px;' +
                   'left: 50%;' +
                   'top: 50%;' +
                   'border: 1px solid #93B4D9;' +
                   'background-color: white;' +
                 '}' +
                 '.chromeFrameOverlayContent iframe {' +
                   'width: 800px;' +
                   'height: 600px;' +
                   'border: none;' +
                 '}' +
                 '.chromeFrameOverlayCloseBar {' +
                   'height: 1em;' +
                   'text-align: right;' +
                   'background-color: #CADEF4;' +
                 '}' +
                 '.chromeFrameOverlayUnderlay {' +
                   'position: absolute;' +
                   'width: 100%;' +
                   'height: 100%;' +
                   'background-color: white;' +
                   'opacity: 0.5;' +
                   '-moz-opacity: 0.5;' +
                   '-webkit-opacity: 0.5;' +
                   '-ms-filter: ' +
                      '"progid:DXImageTransform.Microsoft.Alpha(Opacity=50)";' +
                   'filter: alpha(opacity=50);' +
                 '}';
    injectStyleSheet(rules);
    cfStyleTagInjected = true;
  };

  /**
   * Injects style rules to hide the overlay version of the GCF prompt.
   * Multiple calls have no effect.
   */
  var closeOverlay = function() {
    // IE has a limit to the # of <style> tags allowed, so we avoid
    // tempting the fates.
    if (cfHiddenInjected) {
      return;
    }
    var rules = '.chromeFrameOverlayContent { display: none; }' +
                '.chromeFrameOverlayUnderlay { display: none; }';
    injectStyleSheet(rules);
    // Hide the dialog for a year (or until cookies are deleted).
    var age = 365 * 24 * 60 * 60 * 1000;
    document.cookie = "disableGCFCheck=1;path=/;max-age="+age;
    cfHiddenInjected = true;
  };

  /**
   * Plucks properties from the passed arguments and sets them on the passed
   * DOM node
   * @param {Node} node The node to set properties on
   * @param {Object} args A map of user-specified properties to set
   */
  var setProperties = function(node, args) {

    var srcNode = byId(args['node']);

    node.id = args['id'] || (srcNode ? srcNode['id'] || getUid(srcNode) : '');

    // TODO(slightlyoff): Opera compat? need to test there
    var cssText = args['cssText'] || '';
    node.style.cssText = ' ' + cssText;

    var classText = args['className'] || '';
    node.className = classText;

    // default if the browser doesn't so we don't show sad-tab
    var src = args['src'] || 'about:blank';

    node.src = src;

    if (srcNode) {
      srcNode.parentNode.replaceChild(node, srcNode);
    }
  };

  /**
   * Creates an iframe.
   * @param {Object} args A bag of configuration properties, including values
   *    like 'node', 'cssText', 'className', 'id', 'src', etc.
   * @return {Node}
   */
  var makeIframe = function(args) {
    var el = document.createElement('iframe');
    el.setAttribute('frameborder', '0');
    el.setAttribute('border', '0');
    setProperties(el, args);
    return el;
  };

  /**
   * Adds an unadorned iframe into the page, taking arguments to customize it.
   * @param {Object} args A map of user-specified properties to set
   */
  var makeInlinePrompt = function(args) {
    args.className = 'chromeFrameInstallDefaultStyle ' +
                        (args.className || '');
    var ifr = makeIframe(args);
    // TODO(slightlyoff): handle placement more elegantly!
    if (!ifr.parentNode) {
      var firstChild = document.body.firstChild;
      document.body.insertBefore(ifr, firstChild);
    }
  };

  /**
   * Adds a styled, closable iframe into the page with a background that
   * emulates a modal dialog.
   * @param {Object} args A map of user-specified properties to set
   */
  var makeOverlayPrompt = function(args) {
    if (byId('chromeFrameOverlayContent')) {
      return; // Was previously created. Bail.
    }

    var n = document.createElement('span');
    n.innerHTML = '<div class="chromeFrameOverlayUnderlay"></div>' +
      '<table class="chromeFrameOverlayContent"' +
             'id="chromeFrameOverlayContent"' +
             'cellpadding="0" cellspacing="0">' +
        '<tr class="chromeFrameOverlayCloseBar">' +
          '<td>' +
            // TODO(slightlyoff): i18n
            '<button id="chromeFrameCloseButton">close</button>' +
          '</td>' +
        '</tr>' +
        '<tr>' +
          '<td id="chromeFrameIframeHolder"></td>' +
        '</tr>' +
      '</table>';

    document.body.appendChild(n);
    var ifr = makeIframe(args);
    byId('chromeFrameIframeHolder').appendChild(ifr);
    byId('chromeFrameCloseButton').onclick = closeOverlay;
  };

  var CFInstall = {};

  /**
   * Checks to see if Chrome Frame is available, if not, prompts the user to
   * install. Once installation is begun, a background timer starts,
   * checkinging for a successful install every 2 seconds. Upon detection of
   * successful installation, the current page is reloaded, or if a
   * 'destination' parameter is passed, the page navigates there instead.
   * @param {Object} args A bag of configuration properties. Respected
   *    properties are: 'mode', 'url', 'destination', 'node', 'onmissing',
   *    'preventPrompt', 'oninstall', 'preventInstallDetection', 'cssText', and
   *    'className'.
   * @public
   */
  CFInstall.check = function(args) {
    args = args || {};

    // We currently only support CF in IE
    // TODO(slightlyoff): Update this should we support other browsers!
    var ua = navigator.userAgent;
    var ieRe = /MSIE \S+; Windows NT/;
    var bail = false;
    if (ieRe.test(ua)) {
      // We also only support Win2003/XPSP2 or better. See:
      //  http://msdn.microsoft.com/en-us/library/ms537503%28VS.85%29.aspx
      if (parseFloat(ua.split(ieRe)[1]) < 6 &&
          ua.indexOf('SV1') >= 0) {
        bail = true;
      }
    } else {
      bail = true;
    }
    if (bail) {
      return;
    }

    // Inject the default styles
    injectCFStyleTag();

    if (document.cookie.indexOf("disableGCFCheck=1") >=0) {
      // If we're supposed to hide the overlay prompt, add the rules to do it.
      closeOverlay();
    }

    // When loaded in an alternate protocol (e.g., "file:"), still call out to
    // the right location.
    var currentProtocol = document.location.protocol;
    var protocol = (currentProtocol == 'https:') ? 'https:' : 'http:';
    // TODO(slightlyoff): Update this URL when a mini-installer page is
    //   available.
    var installUrl = protocol + '//www.google.com/chromeframe';
    if (!isAvailable()) {
      if (args.onmissing) {
        args.onmissing();
      }

      args.src = args.url || installUrl;
      var mode = args.mode || 'inline';
      var preventPrompt = args.preventPrompt || false;

      if (!preventPrompt) {
        if (mode == 'inline') {
          makeInlinePrompt(args);
        } else if (mode == 'overlay') {
          makeOverlayPrompt(args);
        } else {
          window.open(args.src);
        }
      }

      if (args.preventInstallDetection) {
        return;
      }

      // Begin polling for install success.
      var installTimer = setInterval(function() {
          // every 2 seconds, look to see if CF is available, if so, proceed on
          // to our destination
          if (isAvailable()) {
            if (args.oninstall) {
              args.oninstall();
            }

            clearInterval(installTimer);
            // TODO(slightlyoff): add a way to prevent navigation or make it
            //    contingent on oninstall?
            window.location = args.destination || window.location;
          }
      }, 2000);
    }
  };

  CFInstall._force = false;
  CFInstall._forceValue = false;
  CFInstall.isAvailable = isAvailable;

  // expose CFInstall to the external scope. We've already checked to make
  // sure we're not going to blow existing objects away.
  scope.CFInstall = CFInstall;

})(this['ChromeFrameInstallScope'] || this);
