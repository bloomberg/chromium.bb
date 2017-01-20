// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var vrShellUi = (function() {
  'use strict';

  let ui = new scene.Scene();
  let uiManager;

  let uiRootElement = document.querySelector('#ui');
  let uiStyle = window.getComputedStyle(uiRootElement);
  /** @const */ var ANIM_DURATION = 150;

  // This value should match the one in VrShellImpl.java
  /** @const */ var UI_DPR = 1.2;

  function getStyleFloat(style, property) {
    let value = parseFloat(style.getPropertyValue(property));
    return isNaN(value) ? 0 : value;
  }

  function getStyleString(style, property) {
    let str = style.getPropertyValue(property);
    return !str || 0 === str.length ? '' : str;
  }

  class ContentQuad {
    constructor() {
      /** @const */ this.SCREEN_HEIGHT = 1.6;
      /** @const */ this.SCREEN_RATIO = 16 / 9;
      /** @const */ this.BROWSING_SCREEN_DISTANCE = 2.0;
      /** @const */ this.FULLSCREEN_DISTANCE = 3.0;

      let element = new api.UiElement(0, 0, 0, 0);
      element.setIsContentQuad();
      element.setVisible(false);
      element.setSize(
          this.SCREEN_HEIGHT * this.SCREEN_RATIO, this.SCREEN_HEIGHT);
      element.setTranslation(0, 0, -this.BROWSING_SCREEN_DISTANCE);
      this.elementId = ui.addElement(element);
    }

    setEnabled(enabled) {
      let update = new api.UiElementUpdate();
      update.setVisible(enabled);
      ui.updateElement(this.elementId, update);
    }

    setFullscreen(enabled) {
      let anim = new api.Animation(this.elementId, ANIM_DURATION);
      if (enabled) {
        anim.setTranslation(0, 0, -this.FULLSCREEN_DISTANCE);
      } else {
        anim.setTranslation(0, 0, -this.BROWSING_SCREEN_DISTANCE);
      }
      ui.addAnimation(anim);
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
      this.translationX = getStyleFloat(style, '--tranX');
      this.translationY = getStyleFloat(style, '--tranY');
      this.translationZ = getStyleFloat(style, '--tranZ');
      element.setTranslation(
          this.translationX, this.translationY, this.translationZ);

      this.uiElementId = ui.addElement(element);
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
        ui.removeAnimation(this.uiAnimationId);
      }
      this.uiAnimationId = ui.addAnimation(anim);
      ui.flush();
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
      this.enabled = false;
      this.reloadUiEnabled = false;

      this.buttons = [];
      let descriptors = [
        [
          '#back',
          function() {
            api.doAction(api.Action.HISTORY_BACK);
          }
        ],
        [
          '#reload',
          function() {
            api.doAction(api.Action.RELOAD);
          }
        ],
        [
          '#forward',
          function() {
            api.doAction(api.Action.HISTORY_FORWARD);
          }
        ],
      ];

      /** @const */ var BUTTON_SPACING = 0.136;

      let startPosition = -BUTTON_SPACING * (descriptors.length / 2.0 - 0.5);
      for (let i = 0; i < descriptors.length; i++) {
        // Use an invisible parent to simplify Z-axis movement on hover.
        let position = new api.UiElement(0, 0, 0, 0);
        position.setVisible(false);
        position.setTranslation(startPosition + i * BUTTON_SPACING, -0.68, -1);
        let id = ui.addElement(position);

        let domId = descriptors[i][0];
        let callback = descriptors[i][1];
        let element = new RoundButton(domId, callback);
        this.buttons.push(element);

        let update = new api.UiElementUpdate();
        update.setParentId(id);
        update.setVisible(false);
        ui.updateElement(element.uiElementId, update);
      }

      this.reloadUiButton = new DomUiElement('#reload-ui-button');
      this.reloadUiButton.domElement.addEventListener('click', function() {
        ui.purge();
        api.doAction(api.Action.RELOAD_UI);
      });

      let update = new api.UiElementUpdate();
      update.setParentId(contentQuadId);
      update.setVisible(false);
      update.setScale(2.2, 2.2, 1);
      update.setTranslation(0, -0.6, 0.3);
      update.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YBOTTOM);
      ui.updateElement(this.reloadUiButton.uiElementId, update);
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
        ui.updateElement(this.buttons[i].uiElementId, update);
      }
      let update = new api.UiElementUpdate();
      update.setVisible(this.enabled && this.reloadUiEnabled);
      ui.updateElement(this.reloadUiButton.uiElementId, update);
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
      ui.updateElement(this.webVrSecureWarning.uiElementId, update);

      // Temporary WebVR security warning. This warning is shown in the center
      // of the field of view, for a limited period of time.
      this.transientWarning = new DomUiElement('#webvr-not-secure-transient');
      update = new api.UiElementUpdate();
      update.setScale(DISTANCE, DISTANCE, 1);
      update.setTranslation(0, 0, -DISTANCE);
      update.setHitTestable(false);
      update.setVisible(false);
      update.setLockToFieldOfView(true);
      ui.updateElement(this.transientWarning.uiElementId, update);
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
      ui.updateElement(this.webVrSecureWarning.uiElementId, update);
      update = new api.UiElementUpdate();
      update.setVisible(visible);
      ui.updateElement(this.transientWarning.uiElementId, update);
    }

    onTransientTimer() {
      let update = new api.UiElementUpdate();
      update.setVisible(false);
      ui.updateElement(this.transientWarning.uiElementId, update);
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
      ui.updateElement(this.domUiElement.uiElementId, update);
      this.nativeState.visible = false;

      // Pull some CSS properties so that Javascript can reconfigure the
      // indicator programmatically.
      let border =
          this.domUiElement.domElement.querySelector('#url-indicator-border');
      let style = window.getComputedStyle(border);
      this.statusBarColor = getStyleString(style, '--statusBarColor');
      this.backgroundColor = style.backgroundColor;
      this.fadeTimeMs = getStyleFloat(style, '--fadeTimeMs');
      this.fadeYOffset = getStyleFloat(style, '--fadeYOffset');
      this.opacity = getStyleFloat(style, '--opacity');
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
      indicator.querySelector('#domain').innerHTML = host;
      indicator.querySelector('#path').innerHTML = path;
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
            new api.Animation(this.domUiElement.uiElementId, this.fadeTimeMs);
        opacityAnimation.setOpacity(this.hidden ? 0.0 : this.opacity);
        ui.addAnimation(opacityAnimation);

        // Drop the position as it fades, or raise the position if appearing.
        let yOffset = this.hidden ? this.fadeYOffset : 0;
        let positionAnimation =
            new api.Animation(this.domUiElement.uiElementId, this.fadeTimeMs);
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
      ui.updateElement(this.domUiElement.uiElementId, update);
      ui.flush();
    }
  };

  class UiManager {
    constructor() {
      this.mode = api.Mode.UNKNOWN;
      this.menuMode = false;
      this.fullscreen = false;

      this.contentQuad = new ContentQuad();
      let contentId = this.contentQuad.getElementId();

      this.controls = new Controls(contentId);
      this.secureOriginWarnings = new SecureOriginWarnings();
      this.urlIndicator = new UrlIndicator();
    }

    setMode(mode, menuMode, fullscreen) {
      /** @const */ var URL_INDICATOR_VISIBILITY_TIMEOUT_MS = 5000;

      this.mode = mode;
      this.menuMode = menuMode;
      this.fullscreen = fullscreen;

      this.contentQuad.setEnabled(mode == api.Mode.STANDARD && !menuMode);
      this.contentQuad.setFullscreen(fullscreen);
      // TODO(crbug/643815): Set aspect ratio on content quad when available.
      // TODO(amp): Don't show controls in fullscreen once MENU mode lands.
      this.controls.setEnabled(mode == api.Mode.STANDARD && !menuMode);
      this.urlIndicator.setEnabled(mode == api.Mode.STANDARD && !menuMode);
      // TODO(amp): Don't show controls in CINEMA mode once MENU mode lands.
      this.urlIndicator.setVisibilityTimeout(
          mode == api.Mode.STANDARD && !menuMode ?
              0 :
              URL_INDICATOR_VISIBILITY_TIMEOUT_MS);
      this.secureOriginWarnings.setEnabled(mode == api.Mode.WEB_VR);

      api.setUiCssSize(
          uiRootElement.clientWidth, uiRootElement.clientHeight, UI_DPR);
    }

    setSecurityLevel(level) {
      this.urlIndicator.setSecurityLevel(level);
    }

    setWebVRSecureOrigin(secure) {
      this.secureOriginWarnings.setSecure(secure);
    }

    setReloadUiEnabled(enabled) {
      this.controls.setReloadUiEnabled(enabled);
    }
  };

  function initialize() {
    uiManager = new UiManager();
    ui.flush();

    api.domLoaded();
  }

  function command(dict) {
    if ('mode' in dict) {
      uiManager.setMode(dict['mode'], dict['menuMode'], dict['fullscreen']);
    }
    if ('securityLevel' in dict) {
      uiManager.setSecurityLevel(dict['securityLevel']);
    }
    if ('webVRSecureOrigin' in dict) {
      uiManager.setWebVRSecureOrigin(dict['webVRSecureOrigin']);
    }
    if ('enableReloadUi' in dict) {
      uiManager.setReloadUiEnabled(dict['enableReloadUi']);
    }
    if ('url' in dict) {
      let url = dict['url'];
      uiManager.urlIndicator.setURL(url['host'], url['path']);
    }
    if ('loading' in dict) {
      uiManager.urlIndicator.setLoading(dict['loading']);
    }
    if ('loadProgress' in dict) {
      uiManager.urlIndicator.setLoadProgress(dict['loadProgress']);
    }
    ui.flush();
  }

  return {
    initialize: initialize,
    command: command,
  };
})();

document.addEventListener('DOMContentLoaded', vrShellUi.initialize);
