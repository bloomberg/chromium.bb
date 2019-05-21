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
 * Creates the base controller of settings view.
 * @param {string} selector Selector text of the view's root element.
 * @param {Object<string|function(Event=)>=} itemHandlers Click-handlers
 *     mapped by element ids.
 * @extends {cca.views.View}
 * @constructor
 */
cca.views.BaseSettings = function(selector, itemHandlers = {}) {
  cca.views.View.call(this, selector, true, true);

  this.root.querySelector('.menu-header button').addEventListener(
      'click', () => this.leave());
  this.root.querySelectorAll('.menu-item').forEach((element) => {
    var handler = itemHandlers[element.id];
    if (handler) {
      element.addEventListener('click', handler);
    }
  });
};

cca.views.BaseSettings.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * Opens sub-settings.
 * @param {string} id Settings identifier.
 * @private
 */
cca.views.BaseSettings.prototype.openSubSettings = function(id) {
  // Dismiss master-settings if sub-settings was dimissed by background click.
  cca.nav.open(id).then((cond) => cond && cond.bkgnd && this.leave());
};

/**
 * Creates the controller of master settings view.
 * @extends {cca.views.BaseSettings}
 * @constructor
 */
cca.views.MasterSettings = function() {
  cca.views.BaseSettings.call(this, '#settings', {
    'settings-gridtype': () => this.openSubSettings('gridsettings'),
    'settings-timerdur': () => this.openSubSettings('timersettings'),
    'settings-resolution': () => this.openSubSettings('resolutionsettings'),
    'settings-feedback': () => this.openFeedback(),
    'settings-help': () => this.openHelp_(),
  });

  // End of properties, seal the object.
  Object.seal(this);

  document.querySelector('#settings-feedback').hidden =
      !cca.util.isChromeVersionAbove(72); // Feedback available since M72.
};

cca.views.MasterSettings.prototype = {
  __proto__: cca.views.BaseSettings.prototype,
};

/**
 * Opens feedback.
 * @private
 */
cca.views.MasterSettings.prototype.openFeedback = function() {
  var data = {
    'categoryTag': 'chromeos-camera-app',
    'requestFeedback': true,
    'feedbackInfo': {
      'description': '',
      'systemInformation': [
        {key: 'APP ID', value: chrome.runtime.id},
        {key: 'APP VERSION', value: chrome.runtime.getManifest().version},
      ],
    },
  };
  const id = 'gfdkimpbcpahaombhbimeihdjnejgicl'; // Feedback extension id.
  chrome.runtime.sendMessage(id, data);
};

/**
 * Opens help.
 * @private
 */
cca.views.MasterSettings.prototype.openHelp_ = function() {
  window.open(
      'https://support.google.com/chromebook/?p=camera_usage_on_chromebook');
};

/**
 * Creates the controller of resolution settings view.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @extends {cca.views.BaseSettings}
 * @constructor
 */
cca.views.ResolutionSettings = function(resolBroker) {
  cca.views.BaseSettings.call(this, '#resolutionsettings', {
    'settings-front-photores': () => {
      const element = document.querySelector('#settings-front-photores');
      if (element.classList.contains('multi-option')) {
        this.openPhotoResSettings_(
            this.frontSetting_[0], this.frontSetting_[1], element);
      }
    },
    'settings-front-videores': () => {
      const element = document.querySelector('#settings-front-videores');
      if (element.classList.contains('multi-option')) {
        this.openVideoResSettings_(
            this.frontSetting_[0], this.frontSetting_[2], element);
      }
    },
    'settings-back-photores': () => {
      const element = document.querySelector('#settings-back-photores');
      if (element.classList.contains('multi-option')) {
        this.openPhotoResSettings_(
            this.backSetting_[0], this.backSetting_[1], element);
      }
    },
    'settings-back-videores': () => {
      const element = document.querySelector('#settings-back-videores');
      if (element.classList.contains('multi-option')) {
        this.openVideoResSettings_(
            this.backSetting_[0], this.backSetting_[2], element);
      }
    },
  });

  /**
   * @type {cca.ResolutionEventBroker}
   * @private
   */
  this.resolBroker_ = resolBroker;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.resMenu_ = document.querySelector('#resolutionsettings>div.menu');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.videoResMenu_ =
      document.querySelector('#videoresolutionsettings>div.menu');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.photoResMenu_ =
      document.querySelector('#photoresolutionsettings>div.menu');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.frontPhotoItem_ = document.querySelector('#settings-front-photores');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.frontVideoItem_ = document.querySelector('#settings-front-videores');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.backPhotoItem_ = document.querySelector('#settings-back-photores');

  /**
   * @type {HTMLElement}
   * @private
   */
  this.backVideoItem_ = document.querySelector('#settings-back-videores');

  /**
   * @type {HTMLTemplateElement}
   * @private
   */
  this.resItemTempl_ = document.querySelector('#resolution-item-template');

  /**
   * @type {HTMLTemplateElement}
   * @private
   */
  this.extcamItemTempl_ =
      document.querySelector('#extcam-resolution-item-template');

  /**
   * Device id and resolutions of front camera. Null for no front camera.
   * @type {?DeviceIdResols}
   * @private
   */
  this.frontSetting_ = null;

  /**
   * Device id and resolutions of back camera. Null for no back camera.
   * @type {?DeviceIdResols}
   * @private
   */
  this.backSetting_ = null;

  /**
   * Device id and resolutions of external cameras.
   * @type {Array<DeviceIdResols>}
   * @private
   */
  this.externalSettings_ = [];

  // End of properties, seal the object.
  Object.seal(this);

  this.resolBroker_.addUpdateDeviceResolutionsListener(
      this.updateResolutions.bind(this));
  this.resolBroker_.addPhotoResolChangeListener(
      this.updateSelectedPhotoResolution_.bind(this));
  this.resolBroker_.addVideoResolChangeListener(
      this.updateSelectedVideoResolution_.bind(this));
};

cca.views.ResolutionSettings.prototype = {
  __proto__: cca.views.BaseSettings.prototype,
};

/**
 * Template for generating option text from photo resolution width and height.
 * @param {number} w Resolution width.
 * @param {number} h Resolution height.
 * @return {string} Text shown on resolution option item.
 * @private
 */
cca.views.ResolutionSettings.prototype.photoOptTextTempl_ = function(w, h) {
  return `${Math.round(w * h / 100000) / 10} megapixels (${w}:${h})`;
};

/**
 * Template for generating option text from video resolution width and height.
 * @param {number} w Resolution width.
 * @param {number} h Resolution height.
 * @return {string} Text shown on resolution option item.
 * @private
 */
cca.views.ResolutionSettings.prototype.videoOptTextTempl_ = function(w, h) {
  return `HD ${h}p (${w}:${h})`;
};

/**
 * Updates resolutions of front, back camera and external cameras.
 * @param {?DeviceIdResols} frontSetting Resolutions of front camera.
 * @param {?DeviceIdResols} backSetting Resolutions of back camera.
 * @param {Array<DeviceIdResols>} externalSettings Resolutions of external
 *     cameras.
 */
cca.views.ResolutionSettings.prototype.updateResolutions = function(
    frontSetting, backSetting, externalSettings) {
  const checkMulti = (item, resolutions, optTextTempl) => {
    // Filter out resolutions of megapixels < 0.1 i.e. megapixels 0.0
    resolutions = resolutions.filter(([w, h]) => w * h >= 100000);
    item.classList.toggle('multi-option', resolutions.length > 1);
    if (resolutions.length == 1) {
      const [w, h] = resolutions;
      item.dataset.sWidth = w;
      item.dataset.sHeight = h;
      item.querySelector('.description>span').textContent = optTextTempl(w, h);
    }
  };

  // Update front camera setting
  this.frontSetting_ = frontSetting;
  cca.state.set('has-front-camera', this.frontSetting_);
  if (this.frontSetting_) {
    const [deviceId, photoRs, videoRs] = this.frontSetting_;
    this.frontPhotoItem_.dataset.deviceId =
        this.frontVideoItem_.dataset.deviceId = deviceId;
    checkMulti(this.frontPhotoItem_, photoRs, this.photoOptTextTempl_);
    checkMulti(this.frontVideoItem_, videoRs, this.videoOptTextTempl_);
  }

  // Update back camera setting
  this.backSetting_ = backSetting;
  cca.state.set('has-back-camera', this.backSetting_);
  if (this.backSetting_) {
    const [deviceId, photoRs, videoRs] = this.backSetting_;
    this.backPhotoItem_.dataset.deviceId =
        this.backVideoItem_.dataset.deviceId = deviceId;
    checkMulti(this.backPhotoItem_, photoRs, this.photoOptTextTempl_);
    checkMulti(this.backVideoItem_, videoRs, this.videoOptTextTempl_);
  }

  // Update external camera settings
  this.externalSettings_ = externalSettings;

  // To prevent losing focus on item already exist before update, locate
  // focused item in both previous and current list, pop out all items in
  // previous list except those having same deviceId as focused one and
  // recreate all other items from current list.
  const prevFocus =
      this.resMenu_.querySelector('.menu-item.external-camera:focus');
  const prevFId = prevFocus && prevFocus.dataset.deviceId;
  const focusIdx = externalSettings.findIndex(([id]) => id === prevFId);
  const fTitle =
      this.resMenu_.querySelector(`.menu-item[data-device-id="${prevFId}"]`);
  const focusedId = focusIdx === -1 ? null : prevFId;

  this.resMenu_.querySelectorAll('.menu-item.external-camera')
      .forEach(
          (element) => element.dataset.deviceId !== focusedId &&
              element.parentNode.removeChild(element));

  externalSettings.forEach(([deviceId, photoRs, videoRs], index) => {
    if (deviceId === focusedId) {
      return;
    }
    const extItem = document.importNode(this.extcamItemTempl_.content, true);
    const [titleItem, photoItem, videoItem] =
        extItem.querySelectorAll('.menu-item');
    titleItem.dataset.deviceId = photoItem.dataset.deviceId =
        videoItem.dataset.deviceId = deviceId;
    checkMulti(photoItem, photoRs, this.photoOptTextTempl_);
    checkMulti(videoItem, videoRs, this.videoOptTextTempl_);

    photoItem.addEventListener('click', () => {
      if (photoItem.classList.contains('multi-option')) {
        this.openPhotoResSettings_(deviceId, photoRs, photoItem);
      }
    });
    videoItem.addEventListener('click', () => {
      if (videoItem.classList.contains('multi-option')) {
        this.openVideoResSettings_(deviceId, videoRs, videoItem);
      }
    });
    if (index < focusIdx) {
      this.resMenu_.insertBefore(extItem, fTitle);
    } else {
      this.resMenu_.appendChild(extItem);
    }
  });
};

/**
 * Updates current selected photo resolution.
 * @param {string} deviceId Device id of the selected resolution.
 * @param {number} width Width of selected resolution.
 * @param {number} height Height of selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateSelectedPhotoResolution_ =
    function(deviceId, width, height) {
  const [photoItem] = this.resMenu_.querySelectorAll(
      `.resol-item[data-device-id="${deviceId}"]`);
  photoItem.dataset.sWidth = width;
  photoItem.dataset.sHeight = height;
  photoItem.querySelector('.description>span').textContent =
      this.photoOptTextTempl_(width, height);

  // Update setting option if it's opened.
  if (cca.state.get('photoresolutionsettings')) {
    this.photoResMenu_
        .querySelector(`input[data-width="${width}"][data-height="${height}"]`)
        .checked = true;
  }
};

/**
 * Updates current selected video resolution.
 * @param {string} deviceId Device id of the selected resolution.
 * @param {number} width Width of selected resolution.
 * @param {number} height Height of selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateSelectedVideoResolution_ =
    function(deviceId, width, height) {
  const [, videoItem] = this.resMenu_.querySelectorAll(
      `.resol-item[data-device-id="${deviceId}"]`);
  videoItem.dataset.sWidth = width;
  videoItem.dataset.sHeight = height;
  videoItem.querySelector('.description>span').textContent =
      this.videoOptTextTempl_(width, height);

  // Update setting option if it's opened.
  if (cca.state.get('videoresolutionsettings')) {
    this.videoResMenu_
        .querySelector(`input[data-width="${width}"][data-height="${height}"]`)
        .checked = true;
  }
};

/**
 * Opens photo resolution setting view.
 * @param {string} deviceId Device id of the opened view.
 * @param {ResolList} resolutions Resolutions to be shown in opened view.
 * @param {HTMLElement} resolItem Dom element from upper layer holding the
 *     selected resolution width, height.
 * @private
 */
cca.views.ResolutionSettings.prototype.openPhotoResSettings_ = function(
    deviceId, resolutions, resolItem) {
  this.updateMenu_(
      resolItem, this.photoResMenu_, this.photoOptTextTempl_,
      (w, h) => this.resolBroker_.requestChangePhotoResol(deviceId, w, h),
      resolutions, parseInt(resolItem.dataset.sWidth),
      parseInt(resolItem.dataset.sHeight));
  this.openSubSettings('photoresolutionsettings');
};

/**
 * Opens video resolution setting view.
 * @param {string} deviceId Device id of the opened view.
 * @param {ResolList} resolutions Resolutions to be shown in opened view.
 * @param {HTMLElement} resolItem Dom element from upper layer holding the
 *     selected resolution width, height.
 * @private
 */
cca.views.ResolutionSettings.prototype.openVideoResSettings_ = function(
    deviceId, resolutions, resolItem) {
  this.updateMenu_(
      resolItem, this.videoResMenu_, this.videoOptTextTempl_,
      (w, h) => this.resolBroker_.requestChangeVideoResol(deviceId, w, h),
      resolutions, parseInt(resolItem.dataset.sWidth),
      parseInt(resolItem.dataset.sHeight));
  this.openSubSettings('videoresolutionsettings');
};

/**
 * Updates resolution menu with specified resolutions.
 * @param {HTMLElement} resolItem DOM element holding selected resolution.
 * @param {HTMLElement} menu Menu holding all resolution option elements.
 * @param {function(number, number): string} optTextTempl Template
 *     generating text content for each resolution option from its
 *     width and height.
 * @param {function(number, number)} onChange Called when selected
 *     option changed with the its width and height
 * @param {ResolList} resolutions Resolutions of its width and height to be
 *      updated with.
 * @param {number} selectedWidth Width of selected resolution.
 * @param {number} selectedHeight Height of selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateMenu_ = function(
    resolItem, menu, optTextTempl, onChange, resolutions, selectedWidth,
    selectedHeight) {
  const captionText = resolItem.querySelector('.description>span');
  const updateSelection = (w, h) => {
    resolItem.dataset.sWidth = w;
    resolItem.dataset.sHeight = h;
    captionText.textContent = optTextTempl(w, h);
  };
  captionText.textContent = '';
  menu.querySelectorAll('.menu-item')
      .forEach((element) => element.parentNode.removeChild(element));

  resolutions.forEach(([w, h], index) => {
    // TODO(inker): i18n contents in optTextTempl
    const item = document.importNode(this.resItemTempl_.content, true);
    const inputElement = item.querySelector('input');
    item.querySelector('span').textContent = optTextTempl(w, h);
    inputElement.name = menu.dataset.name;
    inputElement.dataset.width = w;
    inputElement.dataset.height = h;
    if (w == selectedWidth && h == selectedHeight) {
      updateSelection(w, h);
      inputElement.checked = true;
    }
    inputElement.addEventListener('click', (event) => {
      if (!cca.state.get('streaming') || cca.state.get('taking')) {
        event.preventDefault();
      }
    });
    inputElement.addEventListener('change', (event) => {
      if (inputElement.checked) {
        updateSelection(w, h);
        onChange(w, h);
      }
    });
    menu.appendChild(item);
  });
};
