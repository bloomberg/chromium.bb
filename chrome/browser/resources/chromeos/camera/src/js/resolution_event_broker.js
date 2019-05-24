// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * A list of resolutions represented as its width and height.
 * @typedef {Array<[number, number]>} ResolList
 */

/**
 * Tuple of device id, width, height of preferred capture resolution and all
 * available resolutions for a specific video device.
 * @typedef {[string, number, number, ResolList]} DeviceIdResols
 */

/**
 * Broker for registering or notifying resolution related events.
 * @constructor
 */
cca.ResolutionEventBroker = function() {
  /**
   * Handler for requests of changing user-preferred resolution used in photo
   * taking.
   * @type {function(string, number, number)}
   * @private
   */
  this.photoChangePrefResolHandler_ = () => {};

  /**
   * Handler for requests of changing user-preferred resolution used in video
   * recording.
   * @type {function(string, number, number)}
   * @private
   */
  this.videoChangePrefResolHandler_ = () => {};

  /**
   * Listener for changes of device ids of currently available front, back and
   * external cameras and all of their supported photo resolutions.
   * @type {function(?DeviceIdResols, ?DeviceIdResols, Array<DeviceIdResols>)}
   * @private
   */
  this.photoResolChangeListener_ = () => {};

  /**
   * Listener for changes of device ids of currently available front, back and
   * external cameras and all of their supported video resolutions.
   * @type {function(?DeviceIdResols, ?DeviceIdResols, Array<DeviceIdResols>)}
   * @private
   */
  this.videoResolChangeListener_ = () => {};

  /**
   * Listener for changes of preferred photo resolution used on particular video
   * device.
   * @type {function(string, number, number)}
   * @private
   */
  this.photoPrefResolChangeListener_ = () => {};

  /**
   * Listener for changes of preferred video resolution used on particular video
   * device.
   * @type {function(string, number, number)}
   * @private
   */
  this.videoPrefResolChangeListener_ = () => {};
};

/**
 * Registers handler for requests of changing user-preferred resolution used in
 * photo taking.
 * @param {function(string, number, number)} handler Called with device id of
 *     video device to be changed and width, height of new resolution.
 */
cca.ResolutionEventBroker.prototype.registerChangePhotoPrefResolHandler =
    function(handler) {
  this.photoChangePrefResolHandler_ = handler;
};

/**
 * Registers handler for requests of changing user-preferred resolution used in
 * video recording.
 * @param {function(string, number, number)} handler Called with device id of
 *     video device to be changed and width, height of new resolution.
 */
cca.ResolutionEventBroker.prototype.registerChangeVideoPrefResolHandler =
    function(handler) {
  this.videoChangePrefResolHandler_ = handler;
};

/**
 * Requests for changing user-preferred resolution used in photo taking.
 * @param {string} deviceId Device id of video device to be changed.
 * @param {number} width Change to resolution width.
 * @param {number} height Change to resolution height.
 */
cca.ResolutionEventBroker.prototype.requestChangePhotoPrefResol = function(
    deviceId, width, height) {
  this.photoChangePrefResolHandler_(deviceId, width, height);
};

/**
 * Requests for changing user-preferred resolution used in video recording.
 * @param {string} deviceId Device id of video device to be changed.
 * @param {number} width Change to resolution width.
 * @param {number} height Change to resolution height.
 */
cca.ResolutionEventBroker.prototype.requestChangeVideoPrefResol = function(
    deviceId, width, height) {
  this.videoChangePrefResolHandler_(deviceId, width, height);
};

/**
 * Adds listener for changes of available photo resolutions of all cameras.
 * @param {function(?DeviceIdResols, ?DeviceIdResols, Array<DeviceIdResols>)}
 *     listener Called with device id, preferred photo capture resolution and
 *     available photo resolutions of front, back and external cameras.
 */
cca.ResolutionEventBroker.prototype.addPhotoResolChangeListener = function(
    listener) {
  this.photoResolChangeListener_ = listener;
};

/**
 * Adds listener for changes of available video resolutions of all cameras.
 * @param {function(?DeviceIdResols, ?DeviceIdResols, Array<DeviceIdResols>)}
 *     listener Called with device id, preferred video capture resolution and
 *     available video resolutions of front, back and external cameras.
 */
cca.ResolutionEventBroker.prototype.addVideoResolChangeListener = function(
    listener) {
  this.videoResolChangeListener_ = listener;
};

/**
 * Notifies the change of available photo resolution of all cameras.
 * @param {?DeviceIdResols} frontResolutions Device id of front camera and
 *     all of its available supported photo resolutions.
 * @param {?DeviceIdResols} backResolutions Device id of back camera and
 *     all of its available supported photo resolutions.
 * @param {Array<DeviceIdResols>} externalResolutions Device id of external
 *     cameras and all of their available supported photo resolutions.
 */
cca.ResolutionEventBroker.prototype.notifyPhotoResolChange = function(
    frontResolutions, backResolutions, externalResolutions) {
  this.photoResolChangeListener_(
      frontResolutions, backResolutions, externalResolutions);
};

/**
 * Notifies the change of available video resolution of all cameras.
 * @param {?DeviceIdResols} frontResolutions Device id of front camera and
 *     all of its available supported video resolutions.
 * @param {?DeviceIdResols} backResolutions Device id of back camera and
 *     all of its available supported video resolutions.
 * @param {Array<DeviceIdResols>} externalResolutions Device id of external
 *     cameras and all of their available supported video resolutions.
 */
cca.ResolutionEventBroker.prototype.notifyVideoResolChange = function(
    frontResolutions, backResolutions, externalResolutions) {
  this.videoResolChangeListener_(
      frontResolutions, backResolutions, externalResolutions);
};

/**
 * Adds listener for changes of preferred resolution used in taking photo on
 * particular video device.
 * @param {function(string, number, number)} listener Called with changed video
 *     device id and new preferred resolution width, height.
 */
cca.ResolutionEventBroker.prototype.addPhotoPrefResolChangeListener = function(
    listener) {
  this.photoPrefResolChangeListener_ = listener;
};

/**
 * Adds listener for changes of preferred resolution used in video recording on
 * particular video device.
 * @param {function(string, number, number)} listener Called with changed video
 *     device id and new preferred resolution width, height.
 */
cca.ResolutionEventBroker.prototype.addVideoPrefResolChangeListener = function(
    listener) {
  this.videoPrefResolChangeListener_ = listener;
};

/**
 * Notifies the change of preferred resolution used in photo taking on
 * particular video device.
 * @param {string} deviceId Device id of changed video device.
 * @param {number} width New resolution width.
 * @param {number} height New resolution height.
 */
cca.ResolutionEventBroker.prototype.notifyPhotoPrefResolChange = function(
    deviceId, width, height) {
  this.photoPrefResolChangeListener_(deviceId, width, height);
};

/**
 * Notifies the change of preferred resolution used in video recording on
 * particular video device.
 * @param {string} deviceId Device id of changed video device.
 * @param {number} width New resolution width.
 * @param {number} height New resolution height.
 */
cca.ResolutionEventBroker.prototype.notifyVideoPrefResolChange = function(
    deviceId, width, height) {
  this.videoPrefResolChangeListener_(deviceId, width, height);
};
