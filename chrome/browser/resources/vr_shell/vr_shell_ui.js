// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var vrShellUi = (function() {
  'use strict';

  let ui = new scene.Scene();
  let uiManager;
  let nativeCommandHandler;

  let uiRootElement = document.querySelector('#ui');
  let uiStyle = window.getComputedStyle(uiRootElement);
  /** @const */ var ANIM_DURATION = 150;

  // This value should match the one in VrShellImpl.java
  /** @const */ var UI_DPR = 1.2;

  function getStyleFloat(style, property, defaultValue) {
    let value = parseFloat(style.getPropertyValue(property));
    return isNaN(value) ? defaultValue : value;
  }

  function getStyleString(style, property) {
    let str = style.getPropertyValue(property);
    return !str || 0 === str.length ? '' : str;
  }

  class ContentQuad {
    constructor() {
      /** @const */ this.SCREEN_HEIGHT = 1.375;
      /** @const */ this.SCREEN_RATIO = 9.6 / 6.4;
      /** @const */ this.BROWSING_SCREEN_ELEVATION = -0.15;
      /** @const */ this.BROWSING_SCREEN_DISTANCE = 2.0;
      /** @const */ this.FULLSCREEN_DISTANCE = 3.0;
      /** @const */ this.CSS_WIDTH_PIXELS = 960.0;
      /** @const */ this.CSS_HEIGHT_PIXELS = 640.0;
      /** @const */ this.DPR = 1.2;
      /** @const */ this.MENU_MODE_SCREEN_DISTANCE = 2.0;
      /** @const */ this.MENU_MODE_SCREEN_HEIGHT = 0.48;
      /** @const */ this.MENU_MODE_SCREEN_ELEVATION = -0.125;
      /** @const */ this.BACKGROUND_DISTANCE_MULTIPLIER = 1.414;
      /** @const */ this.DOM_INTERCEPTOR_SELECTOR = '#content-interceptor';
      /** @const */ this.DOM_INTERCEPTOR_ELEVATION = 0.01;

      this.menuMode = false;
      this.fullscreen = false;

      let element = new api.UiElement(0, 0, 0, 0);
      element.setFill(new api.Content());
      element.setVisible(false);
      element.setSize(
          this.SCREEN_HEIGHT * this.SCREEN_RATIO, this.SCREEN_HEIGHT);
      element.setTranslation(
          0, this.BROWSING_SCREEN_ELEVATION, -this.BROWSING_SCREEN_DISTANCE);
      this.elementId = ui.addElement(element);

      // Place an invisible (fill none) but hittable plane behind the content
      // quad, to keep the reticle roughly planar with the content if near
      // content.
      let backPlane = new api.UiElement(0, 0, 0, 0);
      backPlane.setSize(1000, 1000);
      backPlane.setTranslation(0, 0, -0.01);
      backPlane.setParentId(this.elementId);
      backPlane.setFill(new api.NoFill());
      ui.addElement(backPlane);

      // Place invisible plane on top of content quad, to intercept the clicks
      // while on menu mode.
      this.interceptor = new DomUiElement(this.DOM_INTERCEPTOR_SELECTOR);
      let update = new api.UiElementUpdate();
      update.setTranslation(0, 0, this.DOM_INTERCEPTOR_ELEVATION);
      update.setVisible(false);
      update.setParentId(this.elementId);
      update.setSize(
          this.MENU_MODE_SCREEN_HEIGHT * this.SCREEN_RATIO,
          this.MENU_MODE_SCREEN_HEIGHT);
      ui.updateElement(this.interceptor.id, update);
      let interceptorButton =
          document.querySelector(this.DOM_INTERCEPTOR_SELECTOR);
      interceptorButton.addEventListener('click', function() {
        uiManager.exitMenuMode();
        ui.flush();
      });

      this.updateState();
    }

    setEnabled(enabled) {
      let update = new api.UiElementUpdate();
      update.setVisible(enabled);
      ui.updateElement(this.elementId, update);
      if (enabled) {
        api.setContentCssSize(
            this.CSS_WIDTH_PIXELS, this.CSS_HEIGHT_PIXELS, this.DPR);
      } else {
        // TODO(mthiesse): Restore the webVR resolution (which matches native
        // display resolution).
      }
    }

    setMenuMode(enabled) {
      if (this.menuMode == enabled)
        return;
      this.menuMode = enabled;
      this.updateState();
    }

    setFullscreen(enabled) {
      if (this.fullscreen == enabled)
        return;
      this.fullscreen = enabled;
      this.updateState();
    }

    updateState() {
      // Defaults content quad parameters.
      let y = this.BROWSING_SCREEN_ELEVATION;
      let distance = this.BROWSING_SCREEN_DISTANCE;
      let height = this.SCREEN_HEIGHT;

      // Mode-specific overrides.
      if (this.menuMode) {
        y = this.MENU_MODE_SCREEN_ELEVATION;
        distance = this.MENU_MODE_SCREEN_DISTANCE;
        height = this.MENU_MODE_SCREEN_HEIGHT;
      } else if (this.fullscreen) {
        distance = this.FULLSCREEN_DISTANCE;
      }

      let update = new api.UiElementUpdate();
      update.setVisible(this.menuMode);
      ui.updateElement(this.interceptor.id, update);

      let anim;
      anim = new api.Animation(this.elementId, ANIM_DURATION);
      anim.setTranslation(0, y, -distance);
      anim.setEasing(new api.InOutEasing());
      ui.addAnimation(anim);
      anim = new api.Animation(this.elementId, ANIM_DURATION);
      anim.setSize(height * this.SCREEN_RATIO, height);
      anim.setEasing(new api.InOutEasing());
      ui.addAnimation(anim);

      ui.setBackgroundDistance(distance * this.BACKGROUND_DISTANCE_MULTIPLIER);
    }

    // TODO(crbug/643815): Add a method setting aspect ratio (and possible
    // animation of changing it).

    getElementId() {
      return this.elementId;
    }
  };

  class DomUiElement {
    constructor(domId) {
      let domElement = document.querySelector(domId);

      // Pull copy rectangle from the position of the element.
      let rect = domElement.getBoundingClientRect();
      let pixelX = Math.floor(rect.left);
      let pixelY = Math.floor(rect.top);
      let pixelWidth = Math.ceil(rect.right) - pixelX;
      let pixelHeight = Math.ceil(rect.bottom) - pixelY;

      let element = new api.UiElement(pixelX, pixelY, pixelWidth, pixelHeight);
      this.sizeX = pixelWidth / 1000;
      this.sizeY = pixelHeight / 1000;
      element.setSize(this.sizeX, this.sizeY);

      // Parse element CSS properties.
      let style = window.getComputedStyle(domElement);

      this.translationX = getStyleFloat(style, '--tranX', 0);
      this.translationY = getStyleFloat(style, '--tranY', 0);
      this.translationZ = getStyleFloat(style, '--tranZ', 0);
      if (this.translationX != 0 || this.translationY != 0 ||
          this.translationZ != 0) {
        element.setTranslation(
            this.translationX, this.translationY, this.translationZ);
      }

      this.scale = getStyleFloat(style, '--scale', 1);
      if (this.scale != 1) {
        element.setScale(this.scale, this.scale, this.scale);
      }

      this.rotationX = getStyleFloat(style, '--rotX', 0);
      this.rotationY = getStyleFloat(style, '--rotY', 0);
      this.rotationZ = getStyleFloat(style, '--rotZ', 0);
      if (this.rotationX != 0 || this.rotationY != 0 || this.rotationZ != 0) {
        element.setRotation(
            this.rotationX, this.rotationY, this.rotationZ, 2 * Math.PI);
      }

      this.id = ui.addElement(element);
      this.domElement = domElement;
    }
  };

  class Button {
    constructor(domId, callback, parentId) {
      let captionId = domId + '-caption';
      this.button = document.querySelector(domId);
      this.caption = document.querySelector(captionId);
      this.callback = callback;
      this.enabled = true;

      // Create an invisible parent, from which the button will hover.
      let backing = new api.UiElement(0, 0, 0, 0);
      backing.setParentId(parentId);
      backing.setVisible(false);
      this.backingElementId = ui.addElement(backing);

      this.buttonElement = new DomUiElement(domId);
      let update = new api.UiElementUpdate();
      update.setParentId(this.backingElementId);
      ui.updateElement(this.buttonElement.id, update);

      this.captionElement = new DomUiElement(captionId);
      update = new api.UiElementUpdate();
      update.setParentId(this.buttonElement.id);
      update.setTranslation(0, -this.captionElement.sizeY / 2, 0);
      update.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YBOTTOM);
      ui.updateElement(this.captionElement.id, update);

      this.button.addEventListener('mouseenter', this.onMouseEnter.bind(this));
      this.button.addEventListener('mouseleave', this.onMouseLeave.bind(this));
      this.button.addEventListener('click', this.callback);
    }

    setTranslation(x, y, z) {
      let update = new api.UiElementUpdate();
      update.setTranslation(x, y, z);
      ui.updateElement(this.backingElementId, update);
    }

    setVisible(visible) {
      let update = new api.UiElementUpdate();
      update.setVisible(visible);
      ui.updateElement(this.buttonElement.id, update);
      update = new api.UiElementUpdate();
      update.setVisible(visible);
      ui.updateElement(this.captionElement.id, update);
    }

    setEnabled(enabled) {
      this.enabled = enabled;
      if (enabled) {
        this.button.classList.remove('disabled-button');
        this.button.addEventListener('click', this.callback);
      } else {
        this.button.classList.add('disabled-button');
        this.button.removeEventListener('click', this.callback);
      }
    }

    configure(buttonOpacity, captionOpacity, distanceForward) {
      this.button.style.opacity = buttonOpacity;
      this.caption.style.opacity = captionOpacity;

      let anim = new api.Animation(this.buttonElement.id, ANIM_DURATION);
      anim.setTranslation(0, 0, distanceForward);
      ui.addAnimation(anim);
      ui.flush();
    }

    onMouseEnter() {
      if (this.enabled) {
        this.configure(1, 1, 0.015);
      }
    }

    onMouseLeave() {
      this.configure(0.8, 0, 0);
    }
  };

  class GestureHandlers {
    constructor() {
      /** @const */ var BACKING_DISTANCE = 0.8;
      /** @const */ var INDICATOR_DISTANCE = 0.15;
      this.enabled = false;

      let backing = new api.UiElement(0, 0, 0, 0);
      backing.setVisible(false);
      backing.setTranslation(0, 0, -BACKING_DISTANCE);
      backing.setLockToFieldOfView(true);
      this.backingElementId = ui.addElement(backing);

      this.indicators = {};
      this.indicators[api.Direction.LEFT] =
          new GestureHandler(
              '#back-indicator',
              function() {
                api.doAction(api.Action.HISTORY_BACK, {});
              },
              this.backingElementId,
              [-INDICATOR_DISTANCE, 0]);
      this.indicators[api.Direction.RIGHT] =
          new GestureHandler(
              '#forward-indicator',
              function() {
                api.doAction(api.Action.HISTORY_FORWARD, {});
              },
              this.backingElementId,
              [INDICATOR_DISTANCE, 0]);
    }

    setEnabled(enabledIndicators) {
      for (let key in enabledIndicators) {
        if (key in this.indicators) {
          this.indicators[key].setEnabled(enabledIndicators[key]);
        }
      }
    }

    run(direction) {
      if (direction in this.indicators) {
        this.indicators[direction].run();
      }
    }
  }

  class GestureHandler {
    constructor(selector, callback, parentId, position) {
      /** @const */ this.ANIM_DURATION = 250;
      this.enabled = false;

      this.element = new DomUiElement(selector);
      this.callback = callback;

      let update = new api.UiElementUpdate();
      update.setParentId(parentId);
      update.setVisible(true);
      update.setOpacity(0);
      update.setScale(0, 0, 0);
      update.setTranslation(position[0], position[1], 0);
      ui.updateElement(this.element.id, update);
    }

    setTranslation(x, y, z) {
      let update = new api.UiElementUpdate();
      update.setTranslation(x, y, z);
      ui.updateElement(this.element.id, update);
    }

    setEnabled(enabled) {
      this.enabled = enabled;
    }

    run() {
      if (!this.enabled) {
        return;
      }
      this.callback();

      let update = new api.UiElementUpdate();
      update.setScale(0.5, 0.5, 0.5);
      update.setOpacity(0.5);
      ui.updateElement(this.element.id, update);
      let anim = new api.Animation(this.element.id, this.ANIM_DURATION);
      anim.setOpacity(1);
      ui.addAnimation(anim);
      anim = new api.Animation(this.element.id, this.ANIM_DURATION);
      anim.setOpacity(0);
      anim.setDelayedStart(this.ANIM_DURATION);
      ui.addAnimation(anim);
      anim = new api.Animation(this.element.id, this.ANIM_DURATION * 2);
      anim.setScale(1, 1, 1);
      ui.addAnimation(anim);
    }
  }

  class Controls {
    constructor(contentQuadId) {
      this.enabled = false;

      this.buttons = {
        backButton: null,
        reloadButton: null,
        forwardButton: null
      };
      let descriptors = [
        [
          'backButton', '#back-button',
          function() {
            api.doAction(api.Action.HISTORY_BACK, {});
          }
        ],
        [
          'reloadButton', '#reload-button',
          function() {
            api.doAction(api.Action.RELOAD, {});
          }
        ],
        [
          'forwardButton', '#forward-button',
          function() {
            api.doAction(api.Action.HISTORY_FORWARD, {});
          }
        ],
      ];

      /** @const */ var BUTTON_Y = -0.53;
      /** @const */ var BUTTON_Z = -2;
      /** @const */ var BUTTON_SPACING = 0.14;

      let controls = new api.UiElement(0, 0, 0, 0);
      controls.setVisible(false);
      controls.setTranslation(0, BUTTON_Y, BUTTON_Z);
      this.controlsId = ui.addElement(controls);

      let startPosition = -BUTTON_SPACING * (descriptors.length / 2.0 - 0.5);

      for (let i = 0; i < descriptors.length; i++) {
        let name = descriptors[i][0];
        let domId = descriptors[i][1];
        let callback = descriptors[i][2];
        let button = new Button(domId, callback, this.controlsId);
        button.setTranslation(startPosition + i * BUTTON_SPACING, 0, 0);
        this.buttons[name] = button;
      }
    }

    setEnabled(enabled) {
      this.enabled = enabled;
      this.configure();
    }

    configure() {
      for (let key in this.buttons) {
        this.buttons[key].setVisible(this.enabled);
      }
    }

    setBackButtonEnabled(enabled) {
      this.buttons.backButton.setEnabled(enabled);
    }

    setForwardButtonEnabled(enabled) {
      this.buttons.forwardButton.setEnabled(enabled);
    }
  };

  /**
   * A button to trigger a reload of the HTML UI for development purposes.
   */
  class ReloadUiButton {
    constructor() {
      this.enabled = false;
      this.devMode = false;

      this.uiElement = new DomUiElement('#reload-ui-button');
      this.uiElement.domElement.addEventListener('click', function() {
        ui.purge();
        api.doAction(api.Action.RELOAD_UI, {});
      });

      let update = new api.UiElementUpdate();
      update.setVisible(false);
      ui.updateElement(this.uiElement.id, update);
    }

    setEnabled(enabled) {
      this.enabled = enabled;
      this.updateState();
    }

    setDevMode(enabled) {
      this.devMode = enabled;
      this.updateState();
    }

    updateState() {
      let update = new api.UiElementUpdate();
      update.setVisible(this.enabled && this.devMode);
      ui.updateElement(this.uiElement.id, update);
    }
  };

  class SecureOriginWarnings {
    constructor() {
      /** @const */ var DISTANCE = 0.7;
      /** @const */ var ANGLE_UP = 16.3 * Math.PI / 180.0;

      this.enabled = false;
      this.secure = false;
      this.secureOriginTimer = null;

      // Permanent WebVR security warning. This warning is shown near the top of
      // the field of view.
      this.webVrSecureWarning = new DomUiElement('#webvr-not-secure-permanent');
      let update = new api.UiElementUpdate();
      update.setScale(DISTANCE, DISTANCE, 1);
      update.setTranslation(
          0, DISTANCE * Math.sin(ANGLE_UP), -DISTANCE * Math.cos(ANGLE_UP));
      update.setRotation(1.0, 0.0, 0.0, ANGLE_UP);
      update.setHitTestable(false);
      update.setVisible(false);
      update.setLockToFieldOfView(true);
      ui.updateElement(this.webVrSecureWarning.id, update);

      // Temporary WebVR security warning. This warning is shown in the center
      // of the field of view, for a limited period of time.
      this.transientWarning = new DomUiElement('#webvr-not-secure-transient');
      update = new api.UiElementUpdate();
      update.setScale(DISTANCE, DISTANCE, 1);
      update.setTranslation(0, 0, -DISTANCE);
      update.setHitTestable(false);
      update.setVisible(false);
      update.setLockToFieldOfView(true);
      ui.updateElement(this.transientWarning.id, update);
    }

    setEnabled(enabled) {
      this.enabled = enabled;
      this.updateState();
    }

    setSecure(secure) {
      this.secure = secure;
      this.updateState();
    }

    updateState() {
      /** @const */ var TRANSIENT_TIMEOUT_MS = 30000;

      let visible = (this.enabled && !this.secure);
      if (this.secureOriginTimer) {
        clearInterval(this.secureOriginTimer);
        this.secureOriginTimer = null;
      }
      if (visible) {
        this.secureOriginTimer =
            setTimeout(this.onTransientTimer.bind(this), TRANSIENT_TIMEOUT_MS);
      }
      this.showOrHideWarnings(visible);
    }

    showOrHideWarnings(visible) {
      let update = new api.UiElementUpdate();
      update.setVisible(visible);
      ui.updateElement(this.webVrSecureWarning.id, update);
      update = new api.UiElementUpdate();
      update.setVisible(visible);
      ui.updateElement(this.transientWarning.id, update);
    }

    onTransientTimer() {
      let update = new api.UiElementUpdate();
      update.setVisible(false);
      ui.updateElement(this.transientWarning.id, update);
      this.secureOriginTimer = null;
      ui.flush();
    }
  };

  class UrlIndicator {
    constructor() {
      this.domUiElement = new DomUiElement('#url-indicator-container');
      this.enabled = false;
      this.hidden = false;
      this.loading = false;
      this.loadProgress = 0;
      this.level = 0;
      this.visibilityTimeout = 0;
      this.visibilityTimer = null;
      this.nativeState = {};

      // Initially invisible.
      let update = new api.UiElementUpdate();
      update.setVisible(false);
      ui.updateElement(this.domUiElement.id, update);
      this.nativeState.visible = false;

      // Pull some CSS properties so that Javascript can reconfigure the
      // indicator programmatically.
      let border =
          this.domUiElement.domElement.querySelector('#url-indicator-border');
      let style = window.getComputedStyle(border);
      this.statusBarColor = getStyleString(style, '--statusBarColor');
      this.backgroundColor = style.backgroundColor;
      this.fadeTimeMs = getStyleFloat(style, '--fadeTimeMs', 0);
      this.fadeYOffset = getStyleFloat(style, '--fadeYOffset', 0);
      this.opacity = getStyleFloat(style, '--opacity', 1);
    }

    getSecurityIconElementId(level) {
      // See security_state.h and getSecurityIconResource() for this mapping.
      switch (level) {
        case 0:  // NONE
        case 1:  // HTTP_SHOW_WARNING
        case 4:  // SECURITY_WARNING
          return '#url-indicator-info-icon';
        case 2:  // SECURE:
        case 3:  // EV_SECURE:
          return '#url-indicator-lock-icon';
        case 5:  // SECURE_WITH_POLICY_INSTALLED_CERT (ChromeOS only)
        case 6:  // DANGEROUS
        default:
          return '#url-indicator-warning-icon';
      }
    }

    setEnabled(enabled) {
      this.enabled = enabled;
      this.resetVisibilityTimer();
      this.updateState();
    }

    setLoading(loading) {
      this.loading = loading;
      this.loadProgress = 0;
      this.resetVisibilityTimer();
      this.updateState();
    }

    setLoadProgress(progress) {
      this.loadProgress = progress;
      this.updateState();
    }

    setURL(host, path) {
      let indicator = this.domUiElement.domElement;
      indicator.querySelector('#domain').textContent = host;
      indicator.querySelector('#path').textContent = path;
      this.resetVisibilityTimer();
      this.updateState();
    }

    setSecurityLevel(level) {
      document.querySelector('#url-indicator-warning-icon').style.display =
          'none';
      document.querySelector('#url-indicator-info-icon').style.display = 'none';
      document.querySelector('#url-indicator-lock-icon').style.display = 'none';
      let icon = this.getSecurityIconElementId(level);
      document.querySelector(icon).style.display = 'block';

      this.resetVisibilityTimer();
      this.updateState();
    }

    setVisibilityTimeout(milliseconds) {
      this.visibilityTimeout = milliseconds;
      this.resetVisibilityTimer();
      this.updateState();
    }

    resetVisibilityTimer() {
      if (this.visibilityTimer) {
        clearInterval(this.visibilityTimer);
        this.visibilityTimer = null;
      }
      if (this.enabled && this.visibilityTimeout > 0 && !this.loading) {
        this.visibilityTimer = setTimeout(
            this.onVisibilityTimer.bind(this), this.visibilityTimeout);
      }
    }

    onVisibilityTimer() {
      this.visibilityTimer = null;
      this.updateState();
    }

    updateState() {
      this.setNativeVisibility(this.enabled);

      if (!this.enabled) {
        return;
      }

      let indicator = document.querySelector('#url-indicator-border');
      if (this.loading) {
        // Remap load progress range 0-100 as 5-95 percent, to avoid the
        // extremities of the rounded ends of the indicator.
        let percent = Math.round((this.loadProgress * 0.9 + 0.05) * 100);
        let gradient = 'linear-gradient(to right, ' + this.statusBarColor +
            ' 0%, ' + this.statusBarColor + ' ' + percent + '%, ' +
            this.backgroundColor + ' ' + percent + '%, ' +
            this.backgroundColor + ' 100%)';
        indicator.style.background = gradient;
      } else {
        indicator.style.background = this.backgroundColor;
      }

      let shouldBeHidden =
          !this.loading && this.visibilityTimeout > 0 && !this.visibilityTimer;
      if (shouldBeHidden != this.hidden) {
        // Make the box fade away if it's disappearing.
        this.hidden = shouldBeHidden;

        // Fade-out or fade-in the box.
        let opacityAnimation =
            new api.Animation(this.domUiElement.id, this.fadeTimeMs);
        opacityAnimation.setOpacity(this.hidden ? 0.0 : this.opacity);
        ui.addAnimation(opacityAnimation);

        // Drop the position as it fades, or raise the position if appearing.
        let yOffset = this.hidden ? this.fadeYOffset : 0;
        let positionAnimation =
            new api.Animation(this.domUiElement.id, this.fadeTimeMs);
        positionAnimation.setTranslation(
            this.domUiElement.translationX,
            this.domUiElement.translationY + yOffset,
            this.domUiElement.translationZ);
        ui.addAnimation(positionAnimation);
      }

      ui.flush();
    }

    setNativeVisibility(visible) {
      if (visible == this.nativeState.visible) {
        return;
      }
      this.nativeState.visible = visible;
      let update = new api.UiElementUpdate();
      update.setVisible(visible);
      ui.updateElement(this.domUiElement.id, update);
      ui.flush();
    }
  };

  class Background {
    constructor() {
      /** @const */ this.SCENE_GROUND_SIZE = 25.0;
      /** @const */ this.SCENE_HEIGHT = 4.0;
      /** @const */ this.GRIDLINE_COUNT = 40;
      /** @const */ this.HORIZON_COLOR = {r: 0.57, g: 0.57, b: 0.57, a: 1.0};
      /** @const */ this.CENTER_COLOR = {r: 0.48, g: 0.48, b: 0.48, a: 1.0};
      /** @const */ this.FULLSCREEN_BACKGROUND_COLOR =
          {r: 0.1, g: 0.1, b: 0.1, a: 1.0};

      // Make ground plane.
      let groundPlane = new api.UiElementUpdate();
      groundPlane.setVisible(true);
      groundPlane.setSize(this.SCENE_GROUND_SIZE, this.SCENE_GROUND_SIZE);
      groundPlane.setFill(
          new api.OpaqueGradient(this.HORIZON_COLOR, this.CENTER_COLOR));
      groundPlane.setTranslation(0, -this.SCENE_HEIGHT / 2, 0);
      groundPlane.setRotation(1.0, 0.0, 0.0, -Math.PI / 2);
      groundPlane.setDrawPhase(0);
      this.groundPlaneId = ui.addElement(groundPlane);

      // Make ceiling plane.
      let ceilingPlane = new api.UiElementUpdate();
      ceilingPlane.setVisible(true);
      ceilingPlane.setSize(this.SCENE_GROUND_SIZE, this.SCENE_GROUND_SIZE);
      ceilingPlane.setFill(
          new api.OpaqueGradient(this.HORIZON_COLOR, this.CENTER_COLOR));
      ceilingPlane.setTranslation(0, this.SCENE_HEIGHT * 2, 0);
      ceilingPlane.setRotation(1.0, 0.0, 0.0, Math.PI / 2);
      ceilingPlane.setDrawPhase(0);
      this.ceilingPlaneId = ui.addElement(ceilingPlane);

      // Ground grid.
      let groundGrid = new api.UiElementUpdate();
      groundGrid.setVisible(true);
      groundGrid.setSize(this.SCENE_GROUND_SIZE, this.SCENE_GROUND_SIZE);
      let transparentHorizonColor = {
        r: this.HORIZON_COLOR.r,
        g: this.HORIZON_COLOR.g,
        b: this.HORIZON_COLOR.b,
        a: 0
      };
      groundGrid.setFill(new api.GridGradient(
          transparentHorizonColor, this.HORIZON_COLOR, this.GRIDLINE_COUNT));
      groundGrid.setTranslation(0, -this.SCENE_HEIGHT / 2 + 0.01, 0);
      groundGrid.setRotation(1.0, 0.0, 0.0, -Math.PI / 2);
      groundGrid.setDrawPhase(0);
      this.groundGridId = ui.addElement(groundGrid);

      this.setHiddenBackground();
    }

    setElementVisible(elementId, visible) {
      let update = new api.UiElementUpdate();
      update.setVisible(visible);
      ui.updateElement(elementId, update);
    }

    setLightBackground() {
      this.setElementVisible(this.groundPlaneId, true);
      this.setElementVisible(this.ceilingPlaneId, true);
      this.setElementVisible(this.groundGridId, true);
      ui.setBackgroundColor(this.HORIZON_COLOR);
    }

    setDarkBackground() {
      this.setElementVisible(this.groundPlaneId, false);
      this.setElementVisible(this.ceilingPlaneId, false);
      this.setElementVisible(this.groundGridId, true);
      ui.setBackgroundColor(this.FULLSCREEN_BACKGROUND_COLOR);
    }

    setHiddenBackground() {
      this.setElementVisible(this.groundPlaneId, false);
      this.setElementVisible(this.ceilingPlaneId, false);
      this.setElementVisible(this.groundGridId, false);
      ui.setBackgroundColor(this.FULLSCREEN_BACKGROUND_COLOR);
    }

    setState(mode, menuMode, fullscreen) {
      switch (mode) {
        case api.Mode.STANDARD:
          if (fullscreen) {
            this.setDarkBackground();
          } else {
            this.setLightBackground();
          }
          break;
        case api.Mode.WEB_VR:
          if (menuMode) {
            this.setLightBackground();
          } else {
            this.setHiddenBackground();
          }
          break;
      }
    }
  };

  class Omnibox {
    constructor() {
      /** @const */ this.ELEVATION = -0.7;
      /** @const */ this.DISTANCE = -1.99;
      /** @const */ this.SCALE = -this.DISTANCE;

      this.enabled = false;

      let root = document.querySelector('#omnibox-ui-element');
      this.domUiElement = new DomUiElement('#omnibox-url-element');
      this.inputField = root.querySelector('#omnibox-input-field');

      // Initially invisible.
      let update = new api.UiElementUpdate();
      update.setVisible(false);
      update.setTranslation(0, this.ELEVATION, this.DISTANCE);
      update.setScale(this.SCALE, this.SCALE, this.SCALE);
      ui.updateElement(this.domUiElement.id, update);

      // Field-clearing button.
      let clearButton = root.querySelector('#omnibox-clear-button');
      clearButton.addEventListener('click', function() {
        this.inputField.value = '';
        api.doAction(api.Action.OMNIBOX_CONTENT, {'text': ''});
      }.bind(this));

      // Watch for the enter key to trigger navigation.
      this.inputField.addEventListener('keypress', function(e) {
        if (e.keyCode == 13) {
          this.setSuggestions([]);
          api.doAction(
              // TODO(crbug.com/683344): Properly choose prefix.
              api.Action.LOAD_URL, {'url': 'http://' + e.target.value});
        }
      });

      // Watch for field input to generate suggestions.
      this.inputField.addEventListener('input', function(e) {
        api.doAction(api.Action.OMNIBOX_CONTENT, {'text': e.target.value});
      });

      // Clicking on suggestions triggers navigation.
      let elements = root.querySelectorAll('.suggestion');
      this.maxSuggestions = elements.length;
      this.suggestions = [];
      this.suggestionUiElements = [];
      for (var i = 0; i < elements.length; i++) {
        elements[i].addEventListener('click', function(index, e) {
          if (e.target.url) {
            api.doAction(api.Action.LOAD_URL, {'url': e.target.url});
            this.setSuggestions([]);
          }
        }.bind(this, i));

        let elem = new DomUiElement('#suggestion-' + i);
        this.suggestionUiElements.push(elem);
        let update = new api.UiElementUpdate();
        update.setVisible(false);
        update.setParentId(this.domUiElement.id);
        update.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YTOP);
        // Vertically offset suggestions to stack on top of the omnibox. The 0.5
        // offset moves each anchor point from center to edge.
        update.setTranslation(0, elem.sizeY * (i + 0.5), 0);
        ui.updateElement(elem.id, update);
      }
      this.setSuggestions([]);
    }

    setEnabled(enabled) {
      this.enabled = enabled;
      let update = new api.UiElementUpdate();
      update.setVisible(enabled);
      ui.updateElement(this.domUiElement.id, update);
      this.updateSuggestions();
    }

    setURL(url) {
      this.inputField.value = url;
    }

    setSuggestions(suggestions) {
      this.suggestions = suggestions;
      this.updateSuggestions();
    }

    updateSuggestions() {
      for (var i = 0; i < this.maxSuggestions; i++) {
        let element = document.querySelector('#suggestion-' + i);
        let update = new api.UiElementUpdate();
        if (this.enabled && i < this.suggestions.length) {
          element.textContent = this.suggestions[i].description;
          element.url = this.suggestions[i].url;
          update.setVisible(true);
        } else {
          element.textContent = '';
          element.url = null;
          update.setVisible(false);
        }
        ui.updateElement(this.suggestionUiElements[i].id, update);
      }
    }
  };

  // Shows the open tabs.
  //
  // The tab container is made of three <div> nesting levels. The first is the
  // tab container element, which acts like a scroll view. It has a fixed size
  // and corresponds to a UI element in the scene. The second level is the clip
  // element, which is programmatically set to the total width of all it's
  // children (the third nesting level). The clip element is needed to enable
  // horizontal scrolling and prevent the children from breaking to a new line.
  // The third nesting level comprises the actual tabs.
  //
  // TODO(crbug/641487): currently, tabs cannot be scrolled because the
  // scrolling event is not sent to UI elements.
  class TabContainer {
    constructor(contentQuadId) {
      /** @const */ var DOM_TAB_TEMPLATE_SELECTOR = '#tab-template';
      /** @const */ var DOM_TAB_CONTAINER_SELECTOR = '#tab-container';
      /** @const */ var DOM_TAB_CLIP_SELECTOR = '#tab-clip';
      /** @const */ var DOM_NEW_TAB_BUTTON_SELECTOR = '#new-tab';
      /** @const */ var DOM_NEW_INCOGNITO_TAB_BUTTON_SELECTOR =
          '#new-incognito-tab';
      /** @const */ var TAB_CONTAINER_Y_OFFSET = 0.3;
      /** @const */ var TAB_CONTAINER_Z_OFFSET = -2;

      this.domTabs = {};
      this.contentQuadId = contentQuadId;
      this.domTabTemplate = document.querySelector(DOM_TAB_TEMPLATE_SELECTOR);
      this.domTabContainer = document.querySelector(DOM_TAB_CONTAINER_SELECTOR);
      this.domTabClip = document.querySelector(DOM_TAB_CLIP_SELECTOR);

      // Add tab container to native scene.
      this.tabContainerElement = new DomUiElement(DOM_TAB_CONTAINER_SELECTOR);
      let positionUpdate = new api.UiElementUpdate();
      positionUpdate.setTranslation(
          0, TAB_CONTAINER_Y_OFFSET, TAB_CONTAINER_Z_OFFSET);
      positionUpdate.setVisible(false);
      ui.updateElement(this.tabContainerElement.id, positionUpdate);

      // Add the new tab buttons to the native scene.
      let buttonConfigs = [
        {
          selector: DOM_NEW_TAB_BUTTON_SELECTOR, x: -0.2, incognito: false
        }, {
          selector: DOM_NEW_INCOGNITO_TAB_BUTTON_SELECTOR,
          x: 0.2,
          incognito: true
        }
      ];
      this.buttonElements = [];
      buttonConfigs.forEach(function(buttonConfig) {
        let buttonElement = new DomUiElement(buttonConfig.selector);
        let update = new api.UiElementUpdate();
        update.setTranslation(buttonConfig.x, 0.1, 0);
        update.setVisible(false);
        update.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YTOP);
        update.setParentId(this.tabContainerElement.id);
        ui.updateElement(buttonElement.id, update);
        buttonElement.domElement.addEventListener('click', function() {
          api.doAction(
              api.Action.OPEN_NEW_TAB, {'incognito': buttonConfig.incognito});
        });
        this.buttonElements.push(buttonElement);
      }, this);

      // Calculate the width of one tab so that we can properly set the clip
      // element's width.
      this.domTabWidth = this.domTabTemplate.offsetWidth;
      var domTabStyle = window.getComputedStyle(this.domTabTemplate);
      this.domTabWidth += parseInt(domTabStyle.marginLeft, 10) +
          parseInt(domTabStyle.marginRight, 10);
    }

    makeDomTab(tab) {
      // Make a copy of the template tab and add this copy to the tab container
      // view.
      let domTab = this.domTabTemplate.cloneNode(true);
      domTab.removeAttribute('id');
      domTab.addEventListener('click', function() {
        api.doAction(api.Action.SHOW_TAB, {'id': domTab.tab.id});
      });
      domTab.tab = tab;
      this.domTabClip.appendChild(domTab);
      this.domTabs[tab.id] = domTab;
      return domTab;
    }

    resizeClipElement() {
      // Resize clip element so that scrolling works.
      this.domTabClip.style.width =
          (Object.keys(this.domTabs).length * this.domTabWidth) + 'px';
    }

    setTabs(tabs) {
      // Remove all current tabs.
      while (this.domTabClip.firstChild) {
        this.domTabClip.removeChild(this.domTabClip.firstChild);
      }
      this.domTabs = {};

      // Add new tabs.
      for (let i = 0; i < tabs.length; i++) {
        this.addTab(tabs[i]);
      }
    }

    hasTab(tab) {
      return tab.id in this.domTabs;
    }

    addTab(tab) {
      this.makeDomTab(tab);
      this.updateTab(tab);
      this.resizeClipElement();
    }

    updateTab(tab) {
      let domTab = this.domTabs[tab.id];
      domTab.textContent = tab.title;
      domTab.classList.remove('tab-incognito');
      if (tab.incognito) {
        domTab.classList.add('tab-incognito');
      }
    }

    removeTab(tab) {
      let qualifiedTabId = tab.id;
      let domTab = this.domTabs[qualifiedTabId];
      delete this.domTabs[qualifiedTabId];
      this.domTabClip.removeChild(domTab);
      this.resizeClipElement();
    }

    setEnabled(enabled) {
      let update = new api.UiElementUpdate();
      update.setVisible(enabled);
      ui.updateElement(this.tabContainerElement.id, update);
      this.buttonElements.forEach(function(buttonElement) {
        let update = new api.UiElementUpdate();
        update.setVisible(enabled);
        ui.updateElement(buttonElement.id, update);
      }, this);
    }
  };

  class VirtualKeyboard {
    constructor(contentQuadId) {
      /** @const */ this.SCALE = 1.4;
      /** @const */ this.ANGLE_UP = Math.PI / 8;
      /** @const */ this.Y_OFFSET = -1.0;
      /** @const */ this.Z_OFFSET = -1.8;

      this.element = new DomUiElement('#vkb');
      let update = new api.UiElementUpdate();
      update.setVisible(true);
      update.setOpacity(0);
      update.setRotation(1, 0, 0, -this.ANGLE_UP);
      update.setScale(this.SCALE, this.SCALE, this.SCALE);
      update.setTranslation(0, this.Y_OFFSET, this.Z_OFFSET);
      ui.updateElement(this.element.id, update);
    }

    setEnabled(enabled) {
      let anim = new api.Animation(this.element.id, ANIM_DURATION);
      anim.setOpacity(enabled ? 1 : 0);
      ui.addAnimation(anim);
      anim = new api.Animation(this.element.id, ANIM_DURATION);
      let scale = enabled ? this.SCALE : 0;
      anim.setScale(scale, scale, scale);
      ui.addAnimation(anim);
    }
  };

  class UiManager {
    constructor() {
      this.mode = api.Mode.UNKNOWN;
      this.menuMode = false;
      this.fullscreen = false;
      this.canGoBack = false;
      this.canGoForward = false;

      this.background = new Background();
      this.contentQuad = new ContentQuad();
      let contentId = this.contentQuad.getElementId();

      this.gestureHandlers = new GestureHandlers();
      this.controls = new Controls(contentId);
      this.secureOriginWarnings = new SecureOriginWarnings();
      this.urlIndicator = new UrlIndicator();
      this.omnibox = new Omnibox();
      this.reloadUiButton = new ReloadUiButton();
      this.tabContainer = new TabContainer(contentId);
      this.keyboard = new VirtualKeyboard(contentId);
    }

    setMode(mode) {
      this.mode = mode;
      this.updateState();
    }

    setFullscreen(fullscreen) {
      this.fullscreen = fullscreen;
      this.updateState();
    }

    handleAppButtonGesturePerformed(direction) {
      this.gestureHandlers.run(direction);
    }

    handleAppButtonClicked() {
      this.menuMode = !this.menuMode;
      this.updateState();
    }

    setHistoryButtonsEnabled(canGoBack, canGoForward) {
      this.canGoBack = canGoBack;
      this.canGoForward = canGoForward;

      canGoBack = canGoBack || this.fullscreen;

      /** No need to call updateState to adjust button properties. */
      this.controls.setBackButtonEnabled(canGoBack);
      this.controls.setForwardButtonEnabled(canGoForward);
      let enabledIndicators = {}
      enabledIndicators[api.Direction.LEFT] = canGoBack;
      enabledIndicators[api.Direction.RIGHT] = canGoForward;
      this.gestureHandlers.setEnabled(enabledIndicators);
    }

    exitMenuMode() {
      if (this.menuMode) {
        this.menuMode = false;
        this.updateState();
      }
    }

    updateState() {
      /** @const */ var URL_INDICATOR_VISIBILITY_TIMEOUT_MS = 5000;

      let mode = this.mode;
      let menuMode = this.menuMode;
      let fullscreen = this.fullscreen;

      api.doAction(api.Action.SET_CONTENT_PAUSED, {'paused': menuMode});
      ui.setWebVrRenderingModeEnabled(mode == api.Mode.WEB_VR && !menuMode);

      this.contentQuad.setEnabled(mode == api.Mode.STANDARD || menuMode);
      this.contentQuad.setFullscreen(fullscreen);
      this.contentQuad.setMenuMode(menuMode);
      // TODO(crbug/643815): Set aspect ratio on content quad when available.
      this.controls.setEnabled(menuMode);
      this.controls.setBackButtonEnabled(this.canGoBack || this.fullscreen);
      let enabledIndicators = {}
      enabledIndicators[api.Direction.LEFT] = this.canGoBack || this.fullscreen;
      this.gestureHandlers.setEnabled(enabledIndicators);
      this.omnibox.setEnabled(menuMode);
      this.urlIndicator.setEnabled(mode == api.Mode.STANDARD && !menuMode);
      this.urlIndicator.setVisibilityTimeout(
          URL_INDICATOR_VISIBILITY_TIMEOUT_MS);
      this.secureOriginWarnings.setEnabled(
          mode == api.Mode.WEB_VR && !menuMode);
      this.background.setState(mode, menuMode, fullscreen);
      this.tabContainer.setEnabled(menuMode);

      this.reloadUiButton.setEnabled(mode == api.Mode.STANDARD);
      this.keyboard.setEnabled(mode == api.Mode.STANDARD && menuMode);


      api.setUiCssSize(
          uiRootElement.clientWidth, uiRootElement.clientHeight, UI_DPR);
    }

    setSecurityLevel(level) {
      this.urlIndicator.setSecurityLevel(level);
    }

    setWebVRSecureOrigin(secure) {
      this.secureOriginWarnings.setSecure(secure);
    }
  };

  function initialize() {
    uiManager = new UiManager();
    nativeCommandHandler = new UiCommandHandler(uiManager);
    ui.flush();

    api.domLoaded();
  }

  class UiCommandHandler extends api.NativeCommandHandler {
    constructor(uiManager) {
      super();
      this.manager = uiManager;
    }

    /** @override */
    onSetMode(mode) {
      this.manager.setMode(mode);
    }

    /** @override */
    onSetFullscreen(fullscreen) {
      this.manager.setFullscreen(fullscreen);
    }

    /** @override */
    onAppButtonGesturePerformed(direction) {
      this.manager.handleAppButtonGesturePerformed(direction);
    }

    /** @override */
    onAppButtonClicked() {
      this.manager.handleAppButtonClicked();
    }

    /** @override */
    onSetSecurityLevel(level) {
      this.manager.setSecurityLevel(level);
    }

    /** @override */
    onSetWebVRSecureOrigin(secure) {
      this.manager.setWebVRSecureOrigin(secure);
    }

    /** @override */
    onSetReloadUiCapabilityEnabled(enabled) {
      this.manager.reloadUiButton.setDevMode(enabled);
    }

    /** @override */
    onSetUrl(host, path) {
      this.manager.urlIndicator.setURL(host, path);
      this.manager.omnibox.setURL(host, path);
    }

    /** @override */
    onSetLoading(loading) {
      this.manager.urlIndicator.setLoading(loading);
    }

    /** @override */
    onSetLoadingProgress(progress) {
      this.manager.urlIndicator.setLoadProgress(progress);
    }

    /** @override */
    onSetOmniboxSuggestions(suggestions) {
      this.manager.omnibox.setSuggestions(suggestions);
    }

    /** @override */
    onSetTabs(tabs) {
      uiManager.tabContainer.setTabs(tabs);
    }

    /** @override */
    onUpdateTab(tab) {
      if (uiManager.tabContainer.hasTab(tab)) {
        uiManager.tabContainer.updateTab(tab);
      } else {
        uiManager.tabContainer.addTab(tab);
      }
    }

    /** @override */
    onRemoveTab(tab) {
      uiManager.tabContainer.removeTab(tab);
    }

    /** @override */
    onSetHistoryButtonsEnabled(canGoBack, canGoForward) {
      uiManager.setHistoryButtonsEnabled(canGoBack, canGoForward);
    }

    /** @override */
    onCommandHandlerFinished() {
      ui.flush();
    }
  }

  function command(dict) {
    nativeCommandHandler.handleCommand(dict);
  }

  return {
    initialize: initialize,
    command: command,
  };
})();

window.addEventListener('load', vrShellUi.initialize);
