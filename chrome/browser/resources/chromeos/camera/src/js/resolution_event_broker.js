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
 * A list of device id and the supported video, photo resolutions of its
 * corresponding video device.
 * @typedef {[number, ResolList, ResolList]} DeviceIdResols
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
  this.photoChangeResolHandler_ = () => {};

  /**
   * Handler for requests of changing user-preferred resolution used in video
   * recording.
   * @type {function(string, number, number)}
   * @private
   */
  this.videoChangeResolHandler_ = () => {};

  /**
   * Listener for changes of resolution currently used in photo taking.
   * @type {function(string, number, number)}
   * @private
   */
  this.photoResolChangeListener_ = () => {};

  /**
   * Listener for changes of resolution currently used in video recording.
   * @type {function(string, number, number)}
   * @private
   */
  this.videoResolChangeListener_ = () => {};

  /**
   * Listener for changes of currently available device-resolutions of front,
   * back, external cameras.
   * @type {function(?DeviceIdResols, ?DeviceIdResols, Array<DeviceIdResols>)}
   * @private
   */
  this.deviceResolListener_ = () => {};
};

/**
 * Registers handler for requests of changing user-preferred resolution used in
 * photo taking.
 * @param {function(string, number, number)} handler Called with device id of
 *     video device to be changed and width, height of new resolution.
 */
cca.ResolutionEventBroker.prototype.registerChangePhotoResolHandler = function(
    handler) {
  this.photoChangeResolHandler_ = handler;
};

/**
 * Registers handler for requests of changing user-preferred resolution used in
 * video recording.
 * @param {function(string, number, number)} handler Called with device id of
 *     video device to be changed and width, height of new resolution.
 */
cca.ResolutionEventBroker.prototype.registerChangeVideoResolHandler = function(
    handler) {
  this.videoChangeResolHandler_ = handler;
};

/**
 * Requests for changing user-preferred resolution used in photo taking.
 * @param {string} deviceId Device id of video device to be changed.
 * @param {number} width Change to resolution width.
 * @param {number} height Change to resolution height.
 */
cca.ResolutionEventBroker.prototype.requestChangePhotoResol = function(
    deviceId, width, height) {
  this.photoChangeResolHandler_(deviceId, width, height);
};

/**
 * Requests for changing user-preferred resolution used in video recording.
 * @param {string} deviceId Device id of video device to be changed.
 * @param {number} width Change to resolution width.
 * @param {number} height Change to resolution height.
 */
cca.ResolutionEventBroker.prototype.requestChangeVideoResol = function(
    deviceId, width, height) {
  this.videoChangeResolHandler_(deviceId, width, height);
};

/**
   Adds listener for changes of resolution currently used in photo taking.
 * @param {function(string, number, number)} listener Called with changed device
 *     id and new resolution width, height.
 */
cca.ResolutionEventBroker.prototype.addPhotoResolChangeListener = function(
    listener) {
  this.photoResolChangeListener_ = listener;
};

/**
 * Adds listener for changes of resolution currently used in video recording.
 * @param {function(string, number, number)} listener Called with changed device
 *     id and new resolution width, height.
 */
cca.ResolutionEventBroker.prototype.addVideoResolChangeListener = function(
    listener) {
  this.videoResolChangeListener_ = listener;
};

/**
 * Notifies the change of resolution currently used in photo taking.
 * @param {string} deviceId Device id of changed video device.
 * @param {number} width New resolution width.
 * @param {number} height New resolution height.
 */
cca.ResolutionEventBroker.prototype.notifyPhotoResolChange = function(
    deviceId, width, height) {
  this.photoResolChangeListener_(deviceId, width, height);
};

/**
 * Notifies the change of resolution currently used in video recording.
 * @param {string} deviceId Device id of changed video device.
 * @param {number} width New resolution width.
 * @param {number} height New resolution height.
 */
cca.ResolutionEventBroker.prototype.notifyVideoResolChange = function(
    deviceId, width, height) {
  this.videoResolChangeListener_(deviceId, width, height);
};

/**
 * Adds listener for changes of currently available device-resolutions of
 * all cameras.
 * @param {function(?DeviceIdResols, ?DeviceIdResols, Array<DeviceIdResols>)}
 *     listener Listener function called with deviceId-resolutions of front,
 *     back and external cameras.
 */
cca.ResolutionEventBroker.prototype.addUpdateDeviceResolutionsListener =
    function(listener) {
  this.deviceResolListener_ = listener;
};

/**
 * Notifies the change of currently available device-resolutions of all cameras.
 * @param {?DeviceIdResols} front DeviceId-resolutions of front camera.
 * @param {?DeviceIdResols} back DeviceId-resolutions of back camera.
 * @param {Array<DeviceIdResols>} externals DeviceId-resolutions of external
 *     cameras.
 */
cca.ResolutionEventBroker.prototype.notifyUpdateDeviceResolutions = function(
    front, back, externals) {
  this.deviceResolListener_(front, back, externals);
};
