// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Creates a controller to handle layouts of Camera view.
 * @constructor
 */
cca.views.camera.Layout = function() {
  /**
   * CSS sylte of the shifted right-stripe.
   * @type {CSSStyleDeclaration}
   * @private
   */
  this.rightStripe_ = cca.views.camera.Layout.cssStyle_(
      'body.shift-right-stripe .right-stripe, ' +
      'body.shift-right-stripe.tablet-landscape .actions-group');

  /**
   * CSS sylte of the shifted bottom-stripe.
   * @type {CSSStyleDeclaration}
   * @private
   */
  this.bottomStripe_ = cca.views.camera.Layout.cssStyle_(
      'body.shift-bottom-stripe .bottom-stripe, ' +
      'body.shift-bottom-stripe:not(.tablet-landscape) .actions-group');

  /**
   * CSS sylte of the shifted left-stripe.
   * @type {CSSStyleDeclaration}
   * @private
   */
  this.leftStripe_ = cca.views.camera.Layout.cssStyle_(
      'body.shift-left-stripe .left-stripe');

  /**
   * CSS sylte of the shifted top-stripe.
   * @type {CSSStyleDeclaration}
   * @private
   */
  this.topStripe_ = cca.views.camera.Layout.cssStyle_(
      'body.shift-top-stripe .top-stripe');

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * CSS rules.
 * @type {Array<CSSRule>}
 * @private
 */
cca.views.camera.Layout.cssRules_ =
    [].slice.call(document.styleSheets[0].cssRules);

/**
 * Gets the CSS style by the given selector.
 * @param {string} selector Selector text.
 * @return {CSSStyleDeclaration}
 * @private
 */
cca.views.camera.Layout.cssStyle_ = function(selector) {
  return cca.views.camera.Layout.cssRules_.find(
      (rule) => rule.selectorText == selector).style;
};

/**
 * Updates the video element size for previewing in the window.
 * @param {boolean} fullWindow Whether the window is maximized or fullscreen..
 * @return {Array<number>} Letterbox size in [width, height].
 * @private
 */
cca.views.camera.Layout.prototype.updatePreviewSize_ = function(fullWindow) {
  // Make video content keeps its aspect ratio inside the window's inner-bounds;
  // it may fill up the window or be letterboxed when fullscreen/maximized.
  // Don't use app-window.innerBounds' width/height properties during resizing
  // as they are not updated immediately.
  var video = document.querySelector('#preview-video');
  if (video.videoHeight) {
    var f = fullWindow ? Math.min : Math.max;
    var scale = f(window.innerHeight / video.videoHeight,
        window.innerWidth / video.videoWidth);
    video.width = scale * video.videoWidth;
    video.height = scale * video.videoHeight;
  }
  return [window.innerWidth - video.width, window.innerHeight - video.height];
};

/**
 * Updates the layout for video-size or window-size changes.
 */
cca.views.camera.Layout.prototype.update = function() {
  // TODO(yuli): Replace tablet-landscape with max-wnd/tall.
  var fullWindow = cca.util.isWindowFullSize();
  var tall = window.innerHeight > window.innerWidth;
  var tabletLandscape = fullWindow && !tall;
  cca.state.set('tablet-landscape', tabletLandscape);
  cca.state.set('max-wnd', fullWindow);
  cca.state.set('tall', tall);

  var dimens = (shutter) => {
    // These following constants need kept in sync with relevant values in css.
    // preset: button-size + preset-margin + min-margin
    // least: button-size + 2 * min-margin
    // gap: preset-margin - min-margin
    // baseline: preset-baseline
    return shutter ? [100, 88, 12, 56] : [76, 56, 20, 48];
  };

  var [letterboxW, letterboxH] = this.updatePreviewSize_(fullWindow);
  var [halfW, halfH] = [letterboxW / 2, letterboxH / 2];

  // Shift preview to accommodate the shutter in letterbox if applicable.
  var accommodate = (measure) => {
    var [, leastShutter] = dimens(true);
    return (measure > leastShutter) && (measure < leastShutter * 2);
  };
  var cond = fullWindow && tabletLandscape && accommodate(letterboxW);
  var [rightBox, leftBox] = cond ? [letterboxW, 0] : [halfW, halfW];
  cca.state.set('shift-preview-left', cond);

  cond = fullWindow && !tabletLandscape && accommodate(letterboxH);
  var [bottomBox, topBox] = cond ? [letterboxH, 0] : [halfH, halfH];
  cca.state.set('shift-preview-top', cond);

  // Shift buttons' stripes if necessary. Buttons are either fully on letterbox
  // or preview while the shutter/options keep minimum margin to either edges.
  var calc = (measure, least) => {
    return (measure >= least) ?
        Math.round(measure / 2) : // Centered in letterbox.
        Math.round(measure + least / 2); // Inset in preview.
  };
  var shift = (stripe, name, measure, shutter) => {
    var [preset, least, gap, baseline] = dimens(shutter);
    cond = measure > gap && measure < preset;
    if (cond) {
      baseline = calc(measure, least);
      stripe.setProperty(name, baseline + 'px');
    }
    cca.state.set('shift-' + name + '-stripe', cond);
    // Return shutter's baseline in letterbox if applicable.
    return (shutter && baseline < measure) ? baseline : 0;
  };
  var symm = (stripe, name, measure, shutterBaseline) => {
    if (measure && shutterBaseline) {
      cca.state.set('shift-' + name + '-stripe', true);
      stripe.setProperty(name, shutterBaseline + 'px');
      return true;
    }
    return false;
  };
  // Make both letterbox look symmetric if shutter is in either letterbox.
  if (!symm(this.leftStripe_, 'left', leftBox,
      shift(this.rightStripe_, 'right', rightBox, tabletLandscape))) {
    shift(this.leftStripe_, 'left', leftBox, false);
  }
  if (!symm(this.topStripe_, 'top', topBox,
      shift(this.bottomStripe_, 'bottom', bottomBox, !tabletLandscape))) {
    shift(this.topStripe_, 'top', topBox, false);
  }
};
