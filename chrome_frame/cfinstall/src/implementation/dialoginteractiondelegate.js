// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Displays the Chrome Frame installation flow in a Closure
 * dialog component.
 */

goog.provide('google.cf.installer.DialogInteractionDelegate');

goog.require('goog.math.Size');
goog.require('goog.style');
goog.require('goog.ui.Dialog');
goog.require('goog.userAgent');

goog.require('google.cf.installer.InteractionDelegate');

/**
 * An implementation of google.cf.installer.InteractionDelegate that decorates
 * the installation flow using a goog.ui.Dialog .
 * @constructor
 * @implements {google.cf.installer.InteractionDelegate}
 */
google.cf.installer.DialogInteractionDelegate = function() {
};

/**
 * Whether we have already injected the dialog styles into the page.
 * @type {boolean}
 * @private
 **/
google.cf.installer.DialogInteractionDelegate.injectedCss_ = false;

/**
 * Injects the dialog styles into the page.
 * @private
 **/
google.cf.installer.DialogInteractionDelegate.injectCss_ = function() {
  if (google.cf.installer.DialogInteractionDelegate.injectedCss_)
    return;

  google.cf.installer.DialogInteractionDelegate.injectedCss_ = true;

  goog.style.installStyles(
      '.xdgcfinstall-dlg {' +
        'background: #c1d9ff;' +
        'border: 1px solid #3a5774;' +
        'color: #000;' +
        'padding: 4px;' +
        'position: absolute;' +
      '}' +

      '.xdgcfinstall-dlg-bg {' +
        'background: #666;' +
        'left: 0;' +
        'position: absolute;' +
        'top: 0;' +
      '}' +

      '.xdgcfinstall-dlg-title {' +
        'background: #e0edfe;' +
        'color: #000;' +
        'cursor: pointer;' +
        'font-size: 120%;' +
        'font-weight: bold;' +

        'padding: 0px;' +
        'height: 17px;' +
        'position: relative;' +
        '_zoom: 1; /* Ensures proper width in IE6 RTL. */' +
      '}' +

      '.xdgcfinstall-dlg-title-close {' +
        /* Client apps may override the URL at which they serve the sprite. */
        'background: #e0edfe' +
            'url(//ssl.gstatic.com/editor/editortoolbar.png)' +
            'no-repeat -528px 0;' +
        'cursor: default;' +
        'font-size: 0%;' +
        'height: 15px;' +
        'position: absolute;' +
        'top: 1px;' +
        'right: 1px;' +
        'width: 15px;' +
        'padding: 0px 0px 0px 0px;' +
        'align: right;' +
      '}' +

      '.xdgcfinstall-dlg-content {' +
        'background-color: #fff;' +
        'padding: 0px;' +
      '}'
  );
};

/**
 * The active dialog.
 * @type {goog.ui.Dialog}
 * @private
 */
google.cf.installer.DialogInteractionDelegate.prototype.dialog_ = null;

/**
 * @inheritDoc
 */
google.cf.installer.DialogInteractionDelegate.prototype.getIFrameContainer =
    function() {
  google.cf.installer.DialogInteractionDelegate.injectCss_();

  this.dialog_ = new goog.ui.Dialog('xdgcfinstall-dlg', true);
  this.dialog_.setButtonSet(null);
  // TODO(user): The IFrame must be 'visible' or else calculation of its
  // contents' dimensions is incorrect on IE, yet the following line also
  // disables interaction with and dims the page contents. It would be best to
  // only do that when show() is called. Best way is probably to push the mask
  // elements offscreen.
  this.dialog_.setVisible(true);
  goog.style.setPosition(this.dialog_.getDialogElement(), -5000, -5000);
  return /** @type {!HTMLElement} */(this.dialog_.getContentElement());
};

/**
 * @inheritDoc
 */
google.cf.installer.DialogInteractionDelegate.prototype.customizeIFrame =
    function(iframe) {
};

/**
 * @inheritDoc
 */
google.cf.installer.DialogInteractionDelegate.prototype.show =
    function() {
  if (goog.userAgent.IE) {
    // By default, in IE, the dialog does not respect the width of its contents.
    // So we set its dimensions here. We need to specifically add some space for
    // the title bar.
    var iframeSize = goog.style.getBorderBoxSize(
        this.dialog_.getContentElement().getElementsByTagName('iframe')[0]);
    var titleSize = goog.style.getBorderBoxSize(this.dialog_.getTitleElement());
    goog.style.setContentBoxSize(this.dialog_.getDialogElement(),
                                 new goog.math.Size(
                                     iframeSize.width,
                                     iframeSize.height + titleSize.height));
  }
  // Ask the dialog to re-center itself based on the new dimensions.
  this.dialog_.reposition();
};


/**
 * @inheritDoc
 */
google.cf.installer.DialogInteractionDelegate.prototype.reset = function() {
  if (this.dialog_) {
   this.dialog_.setVisible(false);
   this.dialog_.dispose();
   this.dialog_ = null;
  }
};
