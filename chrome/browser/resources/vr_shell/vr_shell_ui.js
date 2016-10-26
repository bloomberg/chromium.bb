// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var vrShellUi = (function() {
  'use strict';

  let scene = new ui.Scene();
  let sceneManager;

  class ContentQuad {
    constructor() {
      /** @const */ var SCREEN_HEIGHT = 1.6;
      /** @const */ var SCREEN_DISTANCE = 2.0;

      let element = new api.UiElement(0, 0, 0, 0);
      element.setIsContentQuad(false);
      element.setVisible(false);
      element.setSize(SCREEN_HEIGHT * 16 / 9, SCREEN_HEIGHT);
      element.setTranslation(0, 0, -SCREEN_DISTANCE);
      this.elementId = scene.addElement(element);
    }

    show(visible) {
      let update = new api.UiElementUpdate();
      update.setVisible(visible);
      scene.updateElement(this.elementId, update);
    }

    getElementId() {
      return this.elementId;
    }
  };

  class DomUiElement {
    constructor(domId) {
      let domElement = document.querySelector(domId);
      let style = window.getComputedStyle(domElement);

      // Pull copy rectangle from DOM element properties.
      let pixelX = domElement.offsetLeft;
      let pixelY = domElement.offsetTop;
      let pixelWidth = parseInt(style.getPropertyValue('width'));
      let pixelHeight = parseInt(style.getPropertyValue('height'));

      let element = new api.UiElement(pixelX, pixelY, pixelWidth, pixelHeight);
      element.setSize(pixelWidth / 1000, pixelHeight / 1000);

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
      let anim = new api.Animation(this.uiElementId, 150);
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
            api.doAction(api.Action.HISTORY_BACK);
          }],
          ['#reload', function() {
            api.doAction(api.Action.RELOAD);
          }],
          ['#forward', function() {
            api.doAction(api.Action.HISTORY_FORWARD);
          }],
      ];

      /** @const */ var BUTTON_SPACING = 0.3;

      let startPosition = -BUTTON_SPACING * (descriptors.length / 2.0 - 0.5);
      for (let i = 0; i < descriptors.length; i++) {
        // Use an invisible parent to simplify Z-axis movement on hover.
        let position = new api.UiElement(0, 0, 0, 0);
        position.setParentId(contentQuadId);
        position.setVisible(false);
        position.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YBOTTOM);
        position.setTranslation(
            startPosition + i * BUTTON_SPACING, -0.3, 0.3);
        let id = scene.addElement(position);

        let domId = descriptors[i][0];
        let callback = descriptors[i][1];
        let element = new RoundButton(domId, callback);
        this.buttons.push(element);

        let update = new api.UiElementUpdate();
        update.setParentId(id);
        update.setVisible(false);
        update.setScale(2.2, 2.2, 1);
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

    show(visible) {
      this.enabled = visible;
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

    show(visible) {
      this.enabled = visible;
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

  class SceneManager {
    constructor() {
      this.mode = api.Mode.UNKNOWN;

      this.contentQuad = new ContentQuad();
      let contentId = this.contentQuad.getElementId();

      this.controls = new Controls(contentId);
      this.secureOriginWarnings = new SecureOriginWarnings();
    }

    setMode(mode) {
      this.mode = mode;
      this.contentQuad.show(mode == api.Mode.STANDARD);
      this.controls.show(mode == api.Mode.STANDARD);
      this.secureOriginWarnings.show(mode == api.Mode.WEB_VR);
    }

    setSecureOrigin(secure) {
      this.secureOriginWarnings.setSecureOrigin(secure);
    }

    setReloadUiEnabled(enabled) {
      console.log('ENABLE');
      this.controls.setReloadUiEnabled(enabled);
    }
  };

  function initialize() {

    // Change the body background so that the transparency applies.
    window.setTimeout(function() {
      document.body.parentNode.style.backgroundColor = 'rgba(255,255,255,0)';
    }, 100);

    sceneManager = new SceneManager();
    scene.flush();

    api.domLoaded();
  }

  function command(dict) {
    if ('mode' in dict) {
      sceneManager.setMode(dict['mode']);
    }
    if ('secureOrigin' in dict) {
      sceneManager.setSecureOrigin(dict['secureOrigin']);
    }
    if ('enableReloadUi' in dict) {
      sceneManager.setReloadUiEnabled(dict['enableReloadUi']);
    }
    scene.flush();
  }

  return {
    initialize: initialize,
    command: command,
  };
})();

document.addEventListener('DOMContentLoaded', vrShellUi.initialize);
