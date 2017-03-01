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
  'REMOVE_ANIMATION': 4,
  'UPDATE_BACKGROUND': 5
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
  'LOAD_URL': 6,
  'OMNIBOX_CONTENT': 7,
  'SET_CONTENT_PAUSED': 8,
  'SHOW_TAB': 9,
  'OPEN_NEW_TAB': 10,
  'KEY_EVENT': 11,
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
 * @param {Object} parameters
 */
api.doAction = function(action, parameters) {
  chrome.send('doAction', [action, parameters]);
};

/**
 * Notify native scene management that DOM loading has completed, at the
 * specified page size.
 */
api.domLoaded = function() {
  chrome.send('domLoaded');
};

/**
 * Sets the CSS size for the content window.
 * @param {number} width
 * @param {number} height
 * @param {number} dpr
 */
api.setContentCssSize = function(width, height, dpr) {
  chrome.send('setContentCssSize', [width, height, dpr]);
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

api.FillType = {
  'NONE': 0,
  'SPRITE': 1,
  'OPAQUE_GRADIENT': 2,
  'GRID_GRADIENT': 3,
  'CONTENT': 4
};

/**
 * Abstract fill base class.
 * @abstract
 */
api.Fill = class {
  constructor(type) {
    this.properties = {};
    this.properties['fillType'] = type;
  }
};

api.NoFill = class extends api.Fill {
  constructor() {
    super(api.FillType.NONE);
  }
}

api.Sprite = class extends api.Fill {
  constructor(pixelX, pixelY, pixelWidth, pixelHeight) {
    super(api.FillType.SPRITE);
    this.properties['copyRectX'] = pixelX;
    this.properties['copyRectY'] = pixelY;
    this.properties['copyRectWidth'] = pixelWidth;
    this.properties['copyRectHeight'] = pixelHeight;
  }
};

api.OpaqueGradient = class extends api.Fill {
  constructor(edgeColor, centerColor) {
    super(api.FillType.OPAQUE_GRADIENT);
    this.properties.edgeColor = edgeColor;
    this.properties.centerColor = centerColor;
  }
};

api.GridGradient = class extends api.Fill {
  constructor(edgeColor, centerColor, gridlineCount) {
    super(api.FillType.GRID_GRADIENT);
    this.properties.edgeColor = edgeColor;
    this.properties.centerColor = centerColor;
    this.properties.gridlineCount = gridlineCount;
  }
};

api.Content = class extends api.Fill {
  constructor() {
    super(api.FillType.CONTENT);
  }
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
    this.properties['sizeX'] = x;
    this.properties['sizeY'] = y;
  }

  /**
   * Specify optional scaling of the element, and any children.
   * @param {number} x
   * @param {number} y
   * @param {number} z
   */
  setScale(x, y, z) {
    this.properties['scaleX'] = x;
    this.properties['scaleY'] = y;
    this.properties['scaleZ'] = z;
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
    this.properties['rotationX'] = x;
    this.properties['rotationY'] = y;
    this.properties['rotationZ'] = z;
    this.properties['rotationAngle'] = a;
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
    this.properties['translationX'] = x;
    this.properties['translationY'] = y;
    this.properties['translationZ'] = z;
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

  setFill(fill) {
    Object.assign(this.properties, fill.properties);
  }

  /**
   * Sets the draw phase. Elements with a lower draw phase are rendered before
   * elements with a higher draw phase. If elements have an equal draw phase
   * the element with the larger distance is drawn first. The default draw phase
   * is 1.
   * @param {number} drawPhase
   */
  setDrawPhase(drawPhase) {
    this.properties['drawPhase'] = drawPhase;
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

    // Apply defaults to new elements.
    this.setVisible(true);
    this.setHitTestable(true);
    this.setFill(new api.Sprite(pixelX, pixelY, pixelWidth, pixelHeight));
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
api.EasingType = {
  'LINEAR': 0,
  'CUBICBEZIER': 1,
  'EASEIN': 2,
  'EASEOUT': 3,
  'EASEINOUT': 4
};

/** @const */ var DEFAULT_EASING_POW = 2;
/** @const */ var DEFAULT_CUBIC_BEZIER_P1X = 0.25;
/** @const */ var DEFAULT_CUBIC_BEZIER_P1Y = 0;
/** @const */ var DEFAULT_CUBIC_BEZIER_P2X = 0.75;
/** @const */ var DEFAULT_CUBIC_BEZIER_P2Y = 1;

/**
 * Abstract easing base class.
 * @abstract
 */
api.Easing = class {
  constructor(type) {
    this.type = type;
  }
};

api.LinearEasing = class extends api.Easing {
  constructor() {
    super(api.EasingType.LINEAR);
  }
};

api.CubicBezierEasing = class extends api.Easing {
  constructor(
      p1x = DEFAULT_CUBIC_BEZIER_P1X,
      p1y = DEFAULT_CUBIC_BEZIER_P1Y,
      p2x = DEFAULT_CUBIC_BEZIER_P2X,
      p2y = DEFAULT_CUBIC_BEZIER_P2Y) {
    super(api.EasingType.CUBICBEZIER);
    this.p1x = p1x;
    this.p1y = p1y;
    this.p2x = p2x;
    this.p2y = p2y;
  }
};

api.InEasing = class extends api.Easing {
  constructor(pow = DEFAULT_EASING_POW) {
    super(api.EasingType.EASEIN);
    this.pow = pow;
  }
};

api.OutEasing = class extends api.Easing {
  constructor(pow = DEFAULT_EASING_POW) {
    super(api.EasingType.EASEOUT);
    this.pow = pow;
  }
};

api.InOutEasing = class extends api.Easing {
  constructor(pow = DEFAULT_EASING_POW) {
    super(api.EasingType.EASEINOUT);
    this.pow = pow;
  }
}

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
    this.easing = new api.LinearEasing();

    // How many milliseconds in the future to start the animation.
    /** @private {number} */
    this.startInMillis = 0.0;

    // Duration of the animation (milliseconds).
    /** @private {number} */
    this.durationMillis = durationMs;
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

  /**
   * Set the animation's easing.
   * @param {api.Easing} easing
   */
  setEasing(easing) {
    this.easing = easing;
  }
};

/**
 * Abstract class handling webui command calls from native.  The UI must
 * subclass this and override the handlers.
 * @abstract
 */
api.NativeCommandHandler = class {
  /**
   * @param {api.Mode} mode
   */
  onSetMode(mode) {}

  /**
   * Handles entering or exiting full-screen mode.
   * @param {boolean} fullscreen
   */
  onSetFullscreen(fullscreen) {}

  /**
   * A controller app button click has happened.
   */
  onAppButtonClicked() {}

  /**
   * Handles a change in the visible page's security level.
   * @param {number} level
   */
  onSetSecurityLevel(level) {}

  /**
   * Handles a change in the WebVR-specific secure-origin state. If |secure| is
   * false, the UI must convey appropriate security warnings.
   * @param {boolean} secure
   */
  onSetWebVRSecureOrigin(secure) {}

  /**
   * Handles enabling of a development-oriented control to reload the UI.
   * @param {boolean} enabled
   */
  onSetReloadUiCapabilityEnabled(enabled) {}

  /**
   * Handles a new URL, specifying the host and path compoments.
   * @param {string} host
   * @param {string} path
   */
  onSetUrl(host, path) {}

  /**
   * Handle a change in loading state (used to show a spinner or other loading
   * indicator).
   * @param {boolean} loading
   */
  onSetLoading(loading) {}

  /**
   * Handle a change in loading progress. Progress is supplied as a number
   * between 0.0 and 1.0.
   * @param {boolean} progress
   */
  onSetLoadingProgress(progress) {}

  /**
   * Handle a change in the set of omnibox suggestions.
   * @param {Array<Object>} suggestions Array of suggestions with string members
   * |description| and |url|.
   */
  onSetOmniboxSuggestions(suggestions) {}

  /**
   * Handle a new set of tabs, overwriting the previous state.
   * @param {Array<Object>} tabs Array of tab states.
   */
  onSetTabs(tabs) {}

  /**
   * Update (or add if not present) a tab.
   * @param {Object} tab
   */
  onUpdateTab(tab) {}

  /**
   * Remove a tab.
   * @param {Object} tab
   */
  onRemoveTab(tab) {}

  /**
   * This function is executed after command parsing completes.
   */
  onCommandHandlerFinished() {}

  /** @final */
  handleCommand(dict) {
    if ('mode' in dict) {
      this.onSetMode(dict['mode']);
    }
    if ('fullscreen' in dict) {
      this.onSetFullscreen(dict['fullscreen'])
    }
    if ('appButtonClicked' in dict) {
      this.onAppButtonClicked();
    }
    if ('securityLevel' in dict) {
      this.onSetSecurityLevel(dict['securityLevel']);
    }
    if ('webVRSecureOrigin' in dict) {
      this.onSetWebVRSecureOrigin(dict['webVRSecureOrigin']);
    }
    if ('enableReloadUi' in dict) {
      this.onSetReloadUiCapabilityEnabled(dict['enableReloadUi']);
    }
    if ('url' in dict) {
      let url = dict['url'];
      this.onSetUrl(url['host'], url['path']);
    }
    if ('loading' in dict) {
      this.onSetLoading(dict['loading']);
    }
    if ('loadProgress' in dict) {
      this.onSetLoadingProgress(dict['loadProgress']);
    }
    if ('suggestions' in dict) {
      this.onSetOmniboxSuggestions(dict['suggestions']);
    }
    if ('setTabs' in dict) {
      this.onSetTabs(dict['setTabs']);
    }
    if ('updateTab' in dict) {
      this.onUpdateTab(dict['updateTab']);
    }
    if ('removeTab' in dict) {
      this.onRemoveTab(dict['removeTab']);
    }

    this.onCommandHandlerFinished()
  }
};
