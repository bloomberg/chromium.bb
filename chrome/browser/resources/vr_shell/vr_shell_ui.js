// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var vrShellUi = (function() {
  'use strict';

  let scene = new ui.Scene();
  let sceneManager;

  let uiRootElement = document.querySelector('#ui');
  let uiStyle = window.getComputedStyle(uiRootElement);
  /** @const */ var ANIM_DURATION = 150;

  // This value should match the one in VrShellImpl.java
  /** @const */ var UI_DPR = 1.2;

  function getStyleFloat(style, property) {
    let value = parseFloat(style.getPropertyValue(property));
    return isNaN(value) ? 0 : value;
  }

  class ContentQuad {
    constructor() {
      /** @const */ this.SCREEN_HEIGHT = 1.6;
      /** @const */ this.SCREEN_RATIO = 16 / 9;
      /** @const */ this.BROWSING_SCREEN_DISTANCE = 2.0;
      /** @const */ this.CINEMA_SCREEN_DISTANCE = 3.0;

      let element = new api.UiElement(0, 0, 0, 0);
      element.setIsContentQuad();
      element.setVisible(false);
      element.setSize(
          this.SCREEN_HEIGHT * this.SCREEN_RATIO, this.SCREEN_HEIGHT);
      element.setTranslation(0, 0, -this.BROWSING_SCREEN_DISTANCE);
      this.elementId = scene.addElement(element);
    }

    setEnabled(enabled) {
      let update = new api.UiElementUpdate();
      update.setVisible(enabled);
      scene.updateElement(this.elementId, update);
    }

    setCinemaMode(enabled) {
      let anim = new api.Animation(this.elementId, ANIM_DURATION);
      if (enabled) {
        anim.setTranslation(0, 0, -this.CINEMA_SCREEN_DISTANCE);
      } else {
        anim.setTranslation(0, 0, -this.BROWSING_SCREEN_DISTANCE);
      }
      scene.addAnimation(anim);
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
      element.setSize(pixelWidth / 1000, pixelHeight / 1000);

      // Pull additional custom properties from CSS.
      let style = window.getComputedStyle(domElement);
      element.setTranslation(
          getStyleFloat(style, '--tranX'),
          getStyleFloat(style, '--tranY'),
          getStyleFloat(style, '--tranZ'));

      this.uiElementId = scene.addElement(element);
      this.uiAnimationId = -1;
      this.domElement = domElement;
    }
  };

  class RoundButton extends DomUiElement {
    constructor(domId, callback) {
      super(domId);

      let button = this.domElement.querySelector('.button');
      button.addEventListener('mouseenter', this.onMouseEnter.bind(this));
      button.addEventListener('mouseleave', this.onMouseLeave.bind(this));
      button.addEventListener('click', callback);
    }

    configure(buttonOpacity, captionOpacity, distanceForward) {
      let button = this.domElement.querySelector('.button');
      let caption = this.domElement.querySelector('.caption');
      button.style.opacity = buttonOpacity;
      caption.style.opacity = captionOpacity;
      let anim = new api.Animation(this.uiElementId, ANIM_DURATION);
      anim.setTranslation(0, 0, distanceForward);
      if (this.uiAnimationId >= 0) {
        scene.removeAnimation(this.uiAnimationId);
      }
      this.uiAnimationId = scene.addAnimation(anim);
      scene.flush();
    }

    onMouseEnter() {
      this.configure(1, 1, 0.015);
    }

    onMouseLeave() {
      this.configure(0.8, 0, 0);
    }
  };

  class Controls {
    constructor(contentQuadId) {
      this.buttons = [];
      let descriptors = [
          ['#back', function() {
            // If we are in cinema mode, revert to standard mode on back press.
            if (sceneManager.cinemaMode) {
              // TODO(crbug/644511): Send a message back to native to handle
              // switching back to standard mode and out of full screen instead
              // of only changing the mode here.
              sceneManager.setMode(sceneManager.mode, sceneManager.menuMode,
                false /* Cinema Mode */);
            } else {
              api.doAction(api.Action.HISTORY_BACK);
            }
          }],
          ['#reload', function() {
            api.doAction(api.Action.RELOAD);
          }],
          ['#forward', function() {
            api.doAction(api.Action.HISTORY_FORWARD);
          }],
      ];

      /** @const */ var BUTTON_SPACING = 0.136;

      let startPosition = -BUTTON_SPACING * (descriptors.length / 2.0 - 0.5);
      for (let i = 0; i < descriptors.length; i++) {
        // Use an invisible parent to simplify Z-axis movement on hover.
        let position = new api.UiElement(0, 0, 0, 0);
        position.setVisible(false);
        position.setTranslation(startPosition + i * BUTTON_SPACING, -0.68, -1);
        let id = scene.addElement(position);

        let domId = descriptors[i][0];
        let callback = descriptors[i][1];
        let element = new RoundButton(domId, callback);
        this.buttons.push(element);

        let update = new api.UiElementUpdate();
        update.setParentId(id);
        update.setVisible(false);
        scene.updateElement(element.uiElementId, update);
      }

      this.reloadUiButton = new DomUiElement('#reload-ui-button');
      this.reloadUiButton.domElement.addEventListener('click', function() {
        scene.purge();
        api.doAction(api.Action.RELOAD_UI);
      });

      let update = new api.UiElementUpdate();
      update.setParentId(contentQuadId);
      update.setVisible(false);
      update.setScale(2.2, 2.2, 1);
      update.setTranslation(0, -0.6, 0.3);
      update.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YBOTTOM);
      scene.updateElement(this.reloadUiButton.uiElementId, update);
    }

    setEnabled(enabled) {
      this.enabled = enabled;
      this.configure();
    }

    setReloadUiEnabled(enabled) {
      this.reloadUiEnabled = enabled;
      this.configure();
    }

    configure() {
      for (let i = 0; i < this.buttons.length; i++) {
        let update = new api.UiElementUpdate();
        update.setVisible(this.enabled);
        scene.updateElement(this.buttons[i].uiElementId, update);
      }
      let update = new api.UiElementUpdate();
      update.setVisible(this.enabled && this.reloadUiEnabled);
      scene.updateElement(this.reloadUiButton.uiElementId, update);
    }
  };

  class SecureOriginWarnings {
    constructor() {
      /** @const */ var DISTANCE = 0.7;
      /** @const */ var ANGLE_UP = 16.3 * Math.PI / 180.0;

      // Permanent WebVR security warning. This warning is shown near the top of
      // the field of view.
      this.webVrSecureWarning = new DomUiElement('#webvr-not-secure-permanent');
      let update = new api.UiElementUpdate();
      update.setScale(DISTANCE, DISTANCE, 1);
      update.setTranslation(0, DISTANCE * Math.sin(ANGLE_UP),
          -DISTANCE * Math.cos(ANGLE_UP));
      update.setRotation(1.0, 0.0, 0.0, ANGLE_UP);
      update.setHitTestable(false);
      update.setVisible(false);
      update.setLockToFieldOfView(true);
      scene.updateElement(this.webVrSecureWarning.uiElementId, update);

      // Temporary WebVR security warning. This warning is shown in the center
      // of the field of view, for a limited period of time.
      this.transientWarning = new DomUiElement(
          '#webvr-not-secure-transient');
      update = new api.UiElementUpdate();
      update.setScale(DISTANCE, DISTANCE, 1);
      update.setTranslation(0, 0, -DISTANCE);
      update.setHitTestable(false);
      update.setVisible(false);
      update.setLockToFieldOfView(true);
      scene.updateElement(this.transientWarning.uiElementId, update);
    }

    setEnabled(enabled) {
      this.enabled = enabled;
      this.updateState();
    }

    setSecureOrigin(secure) {
      this.isSecureOrigin = secure;
      this.updateState();
    }

    updateState() {
      /** @const */ var TRANSIENT_TIMEOUT_MS = 30000;

      let visible = (this.enabled && !this.isSecureOrigin);
      if (this.secureOriginTimer) {
        clearInterval(this.secureOriginTimer);
        this.secureOriginTimer = null;
      }
      if (visible) {
        this.secureOriginTimer = setTimeout(
            this.onTransientTimer.bind(this), TRANSIENT_TIMEOUT_MS);
      }
      this.showOrHideWarnings(visible);
    }

    showOrHideWarnings(visible) {
      let update = new api.UiElementUpdate();
      update.setVisible(visible);
      scene.updateElement(this.webVrSecureWarning.uiElementId, update);
      update = new api.UiElementUpdate();
      update.setVisible(visible);
      scene.updateElement(this.transientWarning.uiElementId, update);
    }

    onTransientTimer() {
      let update = new api.UiElementUpdate();
      update.setVisible(false);
      scene.updateElement(this.transientWarning.uiElementId, update);
      this.secureOriginTimer = null;
      scene.flush();
    }
  };

  class Omnibox {
    constructor(contentQuadId) {
      /** @const */ var VISIBILITY_TIMEOUT_MS = 3000;

      this.domUiElement = new DomUiElement('#omni-container');
      this.enabled = false;
      this.secure = false;
      this.visibilityTimeout = VISIBILITY_TIMEOUT_MS;
      this.visibilityTimer = null;
      this.nativeState = {};

      // Initially invisible.
      let update = new api.UiElementUpdate();
      update.setVisible(false);
      scene.updateElement(this.domUiElement.uiElementId, update);
      this.nativeState.visible = false;

      // Listen to the end of transitions, so that the box can be natively
      // hidden after it finishes hiding itself.
      document.querySelector('#omni').addEventListener('transitionend',
          this.onAnimationDone.bind(this));
    }

    setEnabled(enabled) {
      this.enabled = enabled;
      this.resetVisibilityTimer();
      this.updateState();
    }

    setLoading(loading) {
      this.loading = loading;
      this.resetVisibilityTimer();
      this.updateState();
    }

    setURL(host, path) {
      let omnibox = this.domUiElement.domElement;
      omnibox.querySelector('#domain').innerHTML = host;
      omnibox.querySelector('#path').innerHTML = path;
      this.resetVisibilityTimer();
      this.updateState();
    }

    setSecureOrigin(secure) {
      this.secure = secure;
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

    onAnimationDone(e) {
      if (e.propertyName == 'opacity' && !this.visibleAfterTransition) {
        this.setNativeVisibility(false);
      }
    }

    updateState() {
      if (!this.enabled) {
        this.setNativeVisibility(false);
        return;
      }

      document.querySelector('#omni-secure-icon').style.display =
          (this.secure ? 'block' : 'none');
      document.querySelector('#omni-insecure-icon').style.display =
          (this.secure ? 'none' : 'block');

      let state = 'idle';
      this.visibleAfterTransition = true;
      if (this.loading) {
        state = 'loading';
      } else if (this.visibilityTimeout > 0 && !this.visibilityTimer) {
        state = 'hide';
        this.visibleAfterTransition = false;
      }
      document.querySelector('#omni').className = state;

      this.setNativeVisibility(true);
    }

    setNativeVisibility(visible) {
      if (visible == this.nativeState.visible) {
        return;
      }
      this.nativeState.visible = visible;
      let update = new api.UiElementUpdate();
      update.setVisible(visible);
      scene.updateElement(this.domUiElement.uiElementId, update);
      scene.flush();
    }
  };

  class SceneManager {
    constructor() {
      this.mode = api.Mode.UNKNOWN;
      this.menuMode = false;
      this.cinemaMode = false;

      this.contentQuad = new ContentQuad();
      let contentId = this.contentQuad.getElementId();

      this.controls = new Controls(contentId);
      this.secureOriginWarnings = new SecureOriginWarnings();
      this.omnibox = new Omnibox(contentId);
    }

    setMode(mode, menuMode, cinemaMode) {
      this.mode = mode;
      this.menuMode = menuMode;
      this.cinemaMode = cinemaMode;

      this.contentQuad.setEnabled(mode == api.Mode.STANDARD && !menuMode);
      this.contentQuad.setCinemaMode(cinemaMode);
      // TODO(crbug/643815): Set aspect ratio on content quad when available.
      // TODO(amp): Don't show controls in CINEMA mode once MENU mode lands.
      this.controls.setEnabled(mode == api.Mode.STANDARD && !menuMode);
      this.omnibox.setEnabled(mode == api.Mode.STANDARD && !menuMode);
      this.secureOriginWarnings.setEnabled(mode == api.Mode.WEB_VR);

      api.setUiCssSize(uiRootElement.clientWidth, uiRootElement.clientHeight,
          UI_DPR);
    }

    setSecureOrigin(secure) {
      this.secureOriginWarnings.setSecureOrigin(secure);
      this.omnibox.setSecureOrigin(secure);
    }

    setReloadUiEnabled(enabled) {
      this.controls.setReloadUiEnabled(enabled);
    }
  };

  function initialize() {
    sceneManager = new SceneManager();
    scene.flush();

    api.domLoaded();
  }

  function command(dict) {
    if ('mode' in dict) {
      sceneManager.setMode(dict['mode'], dict['menuMode'], dict['cinemaMode']);
    }
    if ('secureOrigin' in dict) {
      sceneManager.setSecureOrigin(dict['secureOrigin']);
    }
    if ('enableReloadUi' in dict) {
      sceneManager.setReloadUiEnabled(dict['enableReloadUi']);
    }
    if ('url' in dict) {
      let url = dict['url'];
      sceneManager.omnibox.setURL(url['host'], url['path']);
    }
    if ('loading' in dict) {
      sceneManager.omnibox.setLoading(dict['loading']);
    }
    scene.flush();
  }

  return {
    initialize: initialize,
    command: command,
  };
})();

document.addEventListener('DOMContentLoaded', vrShellUi.initialize);
