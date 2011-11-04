// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements a compatibility layer for older versions of
 * CFInstall.js .
 **/

goog.provide('google.cf.installer.OverlayDelegate');

goog.require('google.cf.installer.InteractionDelegate');
goog.require('google.cf.installer.frame');

/**
 * Constructs an InteractionDelegate compatible with previous versions of
 * CFInstall.js .
 * @constructor
 * @implements {google.cf.installer.InteractionDelegate}
 */
google.cf.installer.OverlayDelegate = function(args) {
  this.args_ = args;
};

/**
 * Indicates whether the overlay CSS has already been injected.
 * @type {boolean}
 * @private
 */
google.cf.installer.OverlayDelegate.styleInjected_ = false;

/**
 * Generates the CSS for the overlay.
 * @private
 */
google.cf.installer.OverlayDelegate.injectCss_ = function() {
  if (google.cf.installer.OverlayDelegate.styleInjected_)
    return;
  goog.style.installStyles('.chromeFrameOverlayContent {' +
                             'position: absolute;' +
                             'margin-left: -400px;' +
                             'margin-top: -300px;' +
                             'left: 50%;' +
                             'top: 50%;' +
                             'border: 1px solid #93B4D9;' +
                             'background-color: white;' +
                             'z-index: 2001;' +
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
  '-ms-filter: progid:DXImageTransform.Microsoft.Alpha(Opacity=50)";' +
                             'filter: alpha(opacity=50);' +
                             'z-index: 2000;' +
                           '}');
  google.cf.installer.OverlayDelegate.styleInjected_ = true;
};

/**
 * Generates the HTML for the overlay.
 * @private
 */
google.cf.installer.OverlayDelegate.injectHtml_ = function() {
  google.cf.installer.OverlayDelegate.injectCss_();
  if (document.getElementById('chromeFrameOverlayContent')) {
    return; // Was previously created. Bail.
  }

  var n = document.createElement('span');
  n.innerHTML = '<div class="chromeFrameOverlayUnderlay"></div>' +
    '<table class="chromeFrameOverlayContent"' +
           'id="chromeFrameOverlayContent"' +
           'cellpadding="0" cellspacing="0">' +
      '<tr class="chromeFrameOverlayCloseBar">' +
        '<td>' +
          // TODO(user): i18n
          '<button id="chromeFrameCloseButton">close</button>' +
        '</td>' +
      '</tr>' +
      '<tr>' +
        '<td id="chromeFrameIframeHolder"></td>' +
      '</tr>' +
    '</table>';

  var b = document.body;
  // Insert underlay nodes into the document in the right order.
  while (n.firstChild) {
    b.insertBefore(n.lastChild, b.firstChild);
  }
};

/**
 * @inheritDoc
 */
google.cf.installer.OverlayDelegate.prototype.getIFrameContainer =
    function() {
  google.cf.installer.OverlayDelegate.injectHtml_();
  document.getElementById('chromeFrameCloseButton').onclick =
      goog.bind(this.reset, this);
  return /** @type {!HTMLElement} */(
      document.getElementById('chromeFrameIframeHolder'));
};

/**
 * @inheritDoc
 */
google.cf.installer.OverlayDelegate.prototype.customizeIFrame =
    function(iframe) {
  google.cf.installer.frame.setProperties(iframe, this.args_);
};

/**
 * @inheritDoc
 */
google.cf.installer.OverlayDelegate.prototype.show = function() {
  // Should start it out hidden and reveal/resize here.
};

/**
 * @inheritDoc
 */
google.cf.installer.OverlayDelegate.prototype.reset = function() {
  // IE has a limit to the # of <style> tags allowed, so we avoid
  // tempting the fates.
  if (google.cf.installer.OverlayDelegate.hideInjected_)
    return;

  goog.style.installStyles(
    '.chromeFrameOverlayContent { display: none; } ' +
    '.chromeFrameOverlayUnderlay { display: none; }');
  document.getElementById('chromeFrameIframeHolder').removeChild(
      document.getElementById('chromeFrameIframeHolder').firstChild);

  document.getElementById('chromeFrameCloseButton').onclick = null;

// TODO(user): put this in somewhere.
//   // Hide the dialog for a year (or until cookies are deleted).
//   var age = 365 * 24 * 60 * 60 * 1000;
//   document.cookie = "disableGCFCheck=1;path=/;max-age="+age;
   google.cf.installer.OverlayDelegate.hideInjected_ = true;
};
