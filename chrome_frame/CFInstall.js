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
  
  var cachedAvailable;

  /** 
   * Checks to find out if ChromeFrame is available as a plugin
   * @return {Boolean} 
   */
  var isAvailable = function() {
    if (typeof cachedAvailable != 'undefined') {
      return cachedAvailable;
    }

    cachedAvailable = false;

    // Look for CF in the User Agent before trying more expensive checks
    var ua = navigator.userAgent.toLowerCase();
    if (ua.indexOf("chromeframe") >= 0 || ua.indexOf("x-clock") >= 0) {
      cachedAvailable = true;
      return cachedAvailable;
    }

    if (typeof window['ActiveXObject'] != 'undefined') {
      try {
        var obj = new ActiveXObject('ChromeTab.ChromeFrame');
        if (obj) {
          cachedAvailable = true;
        }
      } catch(e) {
        // squelch
      }
    }
    return cachedAvailable;
  };


  /** @type {boolean} */
  var cfStyleTagInjected = false;

  /** 
   * Creates a style sheet in the document which provides default styling for
   * ChromeFrame instances. Successive calls should have no additive effect.
   */
  var injectCFStyleTag = function() {
    if (cfStyleTagInjected) {
      // Once and only once
      return;
    }
    try {
      var rule = '.chromeFrameInstallDefaultStyle {' +
                    'width: 500px;' +
                    'height: 400px;' +
                    'padding: 0;' +
                    'border: 1px solid #0028c4;' +
                    'margin: 0;' +
                  '}';
      var ss = document.createElement('style');
      ss.setAttribute('type', 'text/css');
      if (ss.styleSheet) {
        ss.styleSheet.cssText = rule;
      } else {
        ss.appendChild(document.createTextNode(rule));
      }
      var h = document.getElementsByTagName('head')[0];
      var firstChild = h.firstChild;
      h.insertBefore(ss, firstChild);
      cfStyleTagInjected = true;
    } catch (e) {
      // squelch
    }
  };


  /** 
   * Plucks properties from the passed arguments and sets them on the passed
   * DOM node
   * @param {Node} node The node to set properties on
   * @param {Object} args A map of user-specified properties to set
   */
  var setProperties = function(node, args) {
    injectCFStyleTag();

    var srcNode = byId(args['node']);

    node.id = args['id'] || (srcNode ? srcNode['id'] || getUid(srcNode) : '');

    // TODO(slightlyoff): Opera compat? need to test there
    var cssText = args['cssText'] || '';
    node.style.cssText = ' ' + cssText;

    var classText = args['className'] || '';
    node.className = 'chromeFrameInstallDefaultStyle ' + classText;

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
    setProperties(el, args);
    return el;
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
    var ieRe = /MSIE (\S+)/;
    if (!ieRe.test(navigator.userAgent)) {
      return;
    }


    // TODO(slightlyoff): Update this URL when a mini-installer page is
    //   available.
    var installUrl = '//www.google.com/chromeframe';
    if (!isAvailable()) {
      if (args.onmissing) {
        args.onmissing();
      }

      args.src = args.url || installUrl;
      var mode = args.mode || 'inline';
      var preventPrompt = args.preventPrompt || false;

      if (!preventPrompt) {
        if (mode == 'inline') {
          var ifr = makeIframe(args);
          // TODO(slightlyoff): handle placement more elegantly!
          if (!ifr.parentNode) {
            var firstChild = document.body.firstChild;
            document.body.insertBefore(ifr, firstChild);
          }
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

  CFInstall.isAvailable = isAvailable;

  // expose CFInstall to the external scope. We've already checked to make
  // sure we're not going to blow existing objects away.
  scope.CFInstall = CFInstall;

})(this['ChromeFrameInstallScope'] || this);
