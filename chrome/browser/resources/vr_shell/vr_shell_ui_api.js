// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var api = {};

/**
 * Enumeration of scene update commands.
 * @enum {number}
 * @const
 */
api.Command = {
  'ADD_ELEMENT': 0,
  'UPDATE_ELEMENT': 1,
  'REMOVE_ELEMENT': 2,
  'ADD_ANIMATION': 3,
  'REMOVE_ANIMATION': 4
};

/**
 * Sends one or more commands to native scene management. Commands are used
 * to add, modify or remove elements and animations. For examples of how to
 * format command parameters, refer to examples in scene.js.
 * @param {Array<Object>} commands
 */
api.sendCommands = function(commands) {
  if (commands.length > 0) {
    chrome.send('updateScene', commands);
  }
};

/**
 * Enumeration of valid Anchroing for X axis.
 * An element can either be anchored to the left, right, or center of the main
 * content rect (or it can be absolutely positioned using NONE). Any
 * translations applied will be relative to this anchoring.
 * @enum {number}
 * @const
 */
api.XAnchoring = {
  'XNONE': 0,
  'XLEFT': 1,
  'XRIGHT': 2
};

/**
 * Enumeration of valid Anchroing for Y axis.
 * @enum {number}
 * @const
 */
api.YAnchoring = {
  'YNONE': 0,
  'YTOP': 1,
  'YBOTTOM': 2
};

/**
 * Enumeration of actions that can be triggered by the HTML UI.
 * @enum {number}
 * @const
 */
api.Action = {
  'HISTORY_BACK': 0,
  'HISTORY_FORWARD': 1,
  'RELOAD': 2,
  'ZOOM_OUT': 3,
  'ZOOM_IN': 4,
  'RELOAD_UI': 5,
};

/**
 * Enumeration of modes that can be specified by the native side.
 * @enum {number}
 * @const
 */
api.Mode = {
  'UNKNOWN': -1,
  'STANDARD': 0,
  'WEB_VR': 1,
};

/**
 * Triggers an Action.
 * @param {api.Action} action
 */
api.doAction = function(action) {
  chrome.send('doAction', [action]);
};

/**
 * Notify native scene management that DOM loading has completed, at the
 * specified page size.
 */
api.domLoaded = function() {
  chrome.send('domLoaded');
};

/**
 * Sets the CSS size for this page.
 * @param {number} width
 * @param {number} height
 * @param {number} dpr
 */
api.setUiCssSize = function(width, height, dpr) {
  chrome.send('setUiCssSize', [width, height, dpr]);
};

/**
 * Represents updates to UI element properties. Any properties set on this
 * object are relayed to an underlying native element via scene command.
 * Properties that are not set on this object are left unchanged.
 * @struct
 */
api.UiElementUpdate = class {
  constructor() {
    /** @private {!Object} */
    this.properties = {'id': -1};
  }

  /**
   * Set the id of the element to update.
   * @param {number} id
   */
  setId(id) {
    this.properties['id'] = id;
  }

  /**
  * Operates on an instance of MyClass and returns something.
  */
  setIsContentQuad() {
    this.properties['contentQuad'] = true;
  }

  /**
   * Specify a parent for this element. If set, this element is positioned
   * relative to its parent element, rather than absolutely. This allows
   * elements to automatically move with a parent.
   * @param {number} id
   */
  setParentId(id) {
    this.properties['parentId'] = id;
  }

  /**
   * Specify the width and height (in meters) of an element.
   * @param {number} x
   * @param {number} y
   */
  setSize(x, y) {
    this.properties['size'] = {x: x, y: y};
  }

  /**
   * Specify optional scaling of the element, and any children.
   * @param {number} x
   * @param {number} y
   * @param {number} z
   */
  setScale(x, y, z) {
    this.properties['scale'] = {x: x, y: y, z: z};
  }

  /**
   * Specify rotation for the element. The rotation is specified in axis-angle
   * representation (rotate around unit vector [x, y, z] by 'a' radians).
   * @param {number} x
   * @param {number} y
   * @param {number} z
   * @param {number} a
   */
  setRotation(x, y, z, a) {
    this.properties['rotation'] = {x: x, y: y, z: z, a: a};
  }

  /**
   * Specify the translation of the element. If anchoring is specified, the
   * offset is applied to the anchoring position rather than the origin.
   * Translation is applied after scaling and rotation.
   * @param {number} x
   * @param {number} y
   * @param {number} z
   */
  setTranslation(x, y, z) {
    this.properties['translation'] = {x: x, y: y, z: z};
  }

  /**
   * Anchoring allows a rectangle to be positioned relative to the edge of
   * its parent, without being concerned about the size of the parent.
   * Values should be XAnchoring and YAnchoring elements.
   * Example: element.setAnchoring(XAnchoring.XNONE, YAnchoring.YBOTTOM);
   * @param {number} x
   * @param {number} y
   */
  setAnchoring(x, y) {
    this.properties['xAnchoring'] = x;
    this.properties['yAnchoring'] = y;
  }

  /**
   * Visibility controls whether the element is rendered.
   * @param {boolean} visible
   */
  setVisible(visible) {
    this.properties['visible'] = !!visible;
  }

  /**
   * Hit-testable implies that the reticle will hit the element, if visible.
   * @param {boolean} testable
   */
  setHitTestable(testable) {
    this.properties['hitTestable'] = !!testable;
  }

  /**
   * Causes an element to be rendered relative to the field of view, rather
   * than the scene. Elements locked in this way should not have a parent.
   * @param {boolean} locked
   */
  setLockToFieldOfView(locked) {
    this.properties['lockToFov'] = !!locked;
  }

  /**
   * Causes an element to be rendered with a specified opacity, between 0.0 and
   * 1.0. Opacity is inherited by children.
   * @param {number} opacity
   */
  setOpacity(opacity) {
    this.properties['opacity'] = opacity;
  }
};

/**
 * Represents a new UI element. This object builds on UiElementUpdate,
 * forcing the underlying texture coordinates to be specified.
 * @struct
 */
api.UiElement = class extends api.UiElementUpdate {
  /**
   * Constructor of UiElement.
   * pixelX and pixelY values indicate the left upper corner; pixelWidth and
   * pixelHeight is width and height of the texture to be copied from the web
   * contents.
   * @param {number} pixelX
   * @param {number} pixelY
   * @param {number} pixelWidth
   * @param {number} pixelHeight
   */
  constructor(pixelX, pixelY, pixelWidth, pixelHeight) {
    super();

    /** @private {Object} */
    this.properties['copyRect'] =
        {x: pixelX, y: pixelY, width: pixelWidth, height: pixelHeight};
  }
};

/**
 * Enumeration of animatable properties.
 * @enum {number}
 * @const
 */
api.Property = {
  'COPYRECT': 0,
  'SIZE': 1,
  'TRANSLATION': 2,
  'SCALE': 3,
  'ROTATION': 4,
  'OPACITY': 5
};

/**
 * Enumeration of easing type.
 * @enum {number}
 * @const
 */
api.Easing = {
  'LINEAR': 0,
  'CUBICBEZIER': 1,
  'EASEIN': 2,
  'EASEOUT': 3
};

/**
 * Base animation class. An animation can vary only one object property.
 * @struct
 */
api.Animation = class {
  constructor(elementId, durationMs) {
    /** @private {number} */
    this.id = -1;
    /** @private {number} */
    this.meshId = elementId;
    /** @private {number} */
    this.property = -1;
    /** @private {Object} */
    this.to = {};
    /** @private {Object} */
    this.easing = {};

    // How many milliseconds in the future to start the animation.
    /** @private {number} */
    this.startInMillis = 0.0;

    // Duration of the animation (milliseconds).
    /** @private {number} */
    this.durationMillis = durationMs;

    this.easing.type = api.Easing.LINEAR;
  }

  /**
   * Set the id of the animation.
   * @param {number} id
   */
  setId(id) {
    this.id = id;
  }

  /**
   * Set the animation's final element size.
   * @param {number} width
   * @param {number} height
   */
  setSize(width, height) {
    this.property = api.Property.SIZE;
    this.to.x = width;
    this.to.y = height;
  }

  /**
   * Set the animation's final element scale.
   * @param {number} x
   * @param {number} y
   * @param {number} z
   */
  setScale(x, y, z) {
    this.property = api.Property.SCALE;
    this.to.x = x;
    this.to.y = y;
    this.to.z = z;
  }

  /**
   * Set the animation's final element rotation.
   * @param {number} x
   * @param {number} y
   * @param {number} z
   * @param {number} a
   */
  setRotation(x, y, z, a) {
    this.property = api.Property.ROTATION;
    this.to.x = x;
    this.to.y = y;
    this.to.z = z;
    this.to.a = a;
  }

  /**
   * Set the animation's final element translation.
   * @param {number} x
   * @param {number} y
   * @param {number} z
   */
  setTranslation(x, y, z) {
    this.property = api.Property.TRANSLATION;
    this.to.x = x;
    this.to.y = y;
    this.to.z = z;
  }

  /**
   * Set the animation's final element opacity.
   * @param {number} opacity
   */
  setOpacity(opacity) {
    this.property = api.Property.OPACITY;
    this.to.x = opacity;
  }
};
