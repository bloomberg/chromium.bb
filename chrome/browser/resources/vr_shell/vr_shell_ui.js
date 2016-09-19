// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var vrShellUi = (function() {
  'use strict';

  /**
   * Enumeration of valid Anchroing for X axis.
   * A mesh can either be anchored to the left, right, or center of the main
   * content rect (or it can be absolutely positioned using NONE). Any
   * translations applied will be relative to this anchoring.
   * @enum {number}
   * @const
   */
  var XAnchoring = Object.freeze({
    'XLEFT': 0,
    'XRIGHT': 1,
    'XCENTER': 2,
    'XNONE': 3
  });

  /**
   * Enumeration of valid Anchroing for Y axis.
   * @enum {number}
   * @const
   */
  var YAnchoring = Object.freeze({
    'YTOP': 0,
    'YBOTTOM': 1,
    'YCENTER': 2,
    'YNONE': 3
  });

  /**
   * Enumeration of animatable properties.
   * @enum {number}
   * @const
   */
  var Property = Object.freeze({
    'COPYRECT': 0,
    'SIZE': 1,
    'TRANSLATION': 2,
    'ORIENTATION': 3,
    'ROTATION': 4
  });

  /**
   * Enumeration of easing type.
   * @enum {number}
   * @const
   */
  var Easing = Object.freeze({
    'LINEAR': 0,
    'CUBICBEZIER': 1,
    'EASEIN': 2,
    'EASEOUT': 3
  });

  /**
   * @type {number} Id generator.
   */
  var idIndex = 1;

  class UiElement {
    /**
     * Constructor of UiElement.
     * pixelX and pixelY values indicate the left upper corner; pixelWidth and
     * pixelHeight is width and height of the texture to be copied from the web
     * contents. metersX and metersY indicate the size of the rectangle onto
     * which the pixel region will be mapped.
     */
    constructor(pixelX, pixelY, pixelWidth, pixelHeight, metersX, metersY) {
      this.copyRect = {
          x: pixelX,
          y: pixelY,
          width: pixelWidth,
          height: pixelHeight
      };
      this.size = { x: metersX, y: metersY };
      this.xAnchoring = XAnchoring.XNONE;
      this.yAnchoring = YAnchoring.YNONE;
      this.anchorZ = false;
      this.translation = { x: 0, y: 0, z: 0 };
      this.orientationAxisAngle = { x: 0, y: 0, z: 0, a: 0 };
      this.rotationAxisAngle = { x: 0, y: 0, z: 0, a: 0 };
    }

    /**
     * The rotation for the mesh in 3D, applied before translation. The
     * rotation is axis-angle representation (rotated around unit vector [x, y,
     * z] by 'a' radians).
     */
    setRotation(x, y, z, a) {
      this.rotationAxisAngle = { x: x, y: y, z: z, a: a };
    }

    /**
     * The offset for the mesh in 3D.  If anchoring is specified, the offset is
     * applied to the anchoring position rather than the origin.
     */
    setTranslation(x, y, z) {
      this.translation = { x: x, y: y, z: z };
    }

    /**
     * Anchoring allows a rectangle to be positioned relative to the content
     * window.  X and Y values should be XAnchoring and YAnchoring elements.
     * anchorZ is a boolean.
     * Example: rect.setAnchoring(XAnchoring.XCENTER, YAnchoring.YBOTTOM, true);
     */
    setAnchoring(x, y, z) {
      this.xAnchoring = x;
      this.yAnchoring = y;
      this.anchorZ = z;
    }
  };

  class Animation {
    constructor(meshId, durationMs) {
      this.meshId = meshId;
      this.easing = {};
      this.from = {};
      this.to = {};
      this.easing.type = Easing.LINEAR;

      // How many milliseconds in the future to start the animation.
      this.startInMillis = 0.0;
      // Duration of the animation (milliseconds).
      this.durationMillis = durationMs;
    }

    setRotateTo(x, y, z, a) {
      this.property = Property.ROTATION;
      this.to.x = x;
      this.to.y = y;
      this.to.z = z;
      this.to.a = a;
    }

    setResizeTo(newWidth, newHeight) {
      this.property = Property.SIZE;
      this.to.x = newWidth;
      this.to.y = newHeight;
    }
  };

  function initialize() {
    domLoaded();

    // Change the body background so that the transparency applies.
    window.setTimeout(function() {
      document.body.parentNode.style.backgroundColor = 'rgba(255,255,255,0)';
    }, 100);

    addControlButtons();
  }

  // Build a row of control buttons.
  function addControlButtons() {
    var buttons = [
        // Button text, UI action passed down to native.
        ['<', 'HISTORY_BACK'],
        ['>', 'HISTORY_FORWARD'],
        ['R', 'RELOAD'],
        ['-', 'ZOOM_OUT'],
        ['+', 'ZOOM_IN']
    ];

    var buttonWidth = 0.3;
    var buttonHeight = 0.2;
    var buttonSpacing = 0.5;
    var buttonStartPosition = -buttonSpacing * (buttons.length / 2.0 - 0.5);

    for (var i = 0; i < buttons.length; i++) {
      var b = document.createElement('div');
      b.position = 'absolute';
      b.style.top = '200px';
      b.style.left = 50 * i + 'px';
      b.style.width = '50px';
      b.style.height = '50px';
      b.className = 'ui-button';
      b.textContent = buttons[i][0];

      // Add click behaviour.
      b.addEventListener('click', function(action, e) {
        chrome.send('doAction', [action]);
      }.bind(undefined, buttons[i][1]));

      document.body.appendChild(b);

      // Add a UI rectangle for the button.
      var el = new UiElement(50 * i, 200, 50, 50, buttonWidth, buttonHeight);
      el.setAnchoring(XAnchoring.XCENTER, YAnchoring.YBOTTOM, true);
      el.setTranslation(buttonStartPosition + buttonSpacing * i, -0.3, 0.0);
      var id = idIndex++;
      addMesh(id, el);

      // Add transitions when the mouse hovers over (and leaves) the button.
      b.addEventListener('mouseenter', function(buttonId, width, height, e) {
        var resize = new Animation(buttonId, 250);
        resize.id = idIndex++;
        resize.setResizeTo(width, height);
        chrome.send('addAnimations', [resize]);
      }.bind(undefined, id, buttonWidth * 1.5, buttonHeight * 1.5));
      b.addEventListener('mouseleave', function(buttonId, width, height) {
        var resize = new Animation(buttonId, 250);
        resize.id = idIndex++;
        resize.setResizeTo(width, height);
        chrome.send('addAnimations', [resize]);
      }.bind(undefined, id, buttonWidth, buttonHeight));
    }
  }

  function domLoaded() {
    chrome.send('domLoaded', [window.innerWidth, window.innerHeight]);
  }

  function addAnimations(animations) {
    chrome.send('addAnimations', animations);
  }

  function addMesh(id, mesh) {
    chrome.send('addMesh', [id, mesh]);
  }

  return {
    initialize: initialize,
  };
})();

document.addEventListener('DOMContentLoaded', vrShellUi.initialize);
