// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Handler of device event.
 * @constructor
 */
function DeviceHandler() {
  /**
   * Map of device path and mount status of devices.
   * @type {Object.<string, DeviceHandler.MountStatus>}
   * @private
   */
  this.mountStatus_ = {};

  chrome.fileBrowserPrivate.onDeviceChanged.addListener(
      this.onDeviceChanged_.bind(this));
  chrome.fileBrowserPrivate.onMountCompleted.addListener(
      this.onMountCompleted_.bind(this));

  Object.seal(this);
}

/**
 * Notification type.
 * @param {string} prefix Prefix of notification ID.
 * @param {string} title String ID of title.
 * @param {string} message String ID of message.
 * @constructor
 */
DeviceHandler.Notification = function(prefix, title, message) {
  /**
   * Prefix of notification ID.
   * @type {string}
   */
  this.prefix = prefix;

  /**
   * String ID of title.
   * @type {string}
   */
  this.title = title;

  /**
   * String ID of message.
   * @type {string}
   */
  this.message = message;

  Object.freeze(this);
};

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE = new DeviceHandler.Notification(
    'device',
    'REMOVABLE_DEVICE_DETECTION_TITLE',
    'REMOVABLE_DEVICE_SCANNING_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_FAIL = new DeviceHandler.Notification(
    'deviceFail',
    'REMOVABLE_DEVICE_DETECTION_TITLE',
    'DEVICE_UNSUPPORTED_DEFAULT_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_EXTERNAL_STORAGE_DISABLED =
    new DeviceHandler.Notification(
        'deviceFail',
        'REMOVABLE_DEVICE_DETECTION_TITLE',
        'EXTERNAL_STORAGE_DISABLED_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.FORMAT_START = new DeviceHandler.Notification(
    'formatStart',
    'FORMATTING_OF_DEVICE_PENDING_TITLE',
    'FORMATTING_OF_DEVICE_PENDING_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.FORMAT_SUCCESS = new DeviceHandler.Notification(
    'formatSuccess',
    'FORMATTING_OF_DEVICE_FINISHED_TITLE',
    'FORMATTING_FINISHED_SUCCESS_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.FORMAT_FAIL = new DeviceHandler.Notification(
    'formatFail',
    'FORMATTING_OF_DEVICE_FAILED_TITLE',
    'FORMATTING_FINISHED_FAILURE_MESSAGE');

/**
 * Shows the notification for the device path.
 * @param {string} devicePath Device path.
 * @param {string=} opt_message Message overrides the default message.
 */
DeviceHandler.Notification.prototype.show = function(devicePath, opt_message) {
  chrome.notifications.create(
      this.makeId_(devicePath),
      {
        type: 'basic',
        title: str(this.title),
        message: opt_message || str(this.message),
        iconUrl: chrome.runtime.getURL('/common/images/icon96.png')
      },
      function() {});
};

/**
 * Hides the notification for the device path.
 * @param {string} devicePath Device path.
 */
DeviceHandler.Notification.prototype.hide = function(devicePath) {
  chrome.notifications.clear(this.makeId_(devicePath), function() {});
};

/**
 * Makes a notification ID for the device path.
 * @param {string} devicePath Device path.
 * @return {string} Notification ID.
 * @private
 */
DeviceHandler.Notification.prototype.makeId_ = function(devicePath) {
  return this.prefix + ':' + devicePath;
};

/**
 * Handles notifications from C++ sides.
 * @param {DeviceEvent} event Device event.
 * @private
 */
DeviceHandler.prototype.onDeviceChanged_ = function(event) {
  switch (event.type) {
    case 'added':
      DeviceHandler.Notification.DEVICE.show(event.devicePath);
      this.mountStatus_[event.devicePath] = DeviceHandler.MountStatus.NO_RESULT;
      break;
    case 'disabled':
      DeviceHandler.Notification.DEVICE_EXTERNAL_STORAGE_DISABLED.show(
          event.devicePath);
      break;
    case 'scan_canceled':
      DeviceHandler.Notification.DEVICE.hide(event.devicePath);
      break;
    case 'removed':
      DeviceHandler.Notification.DEVICE.hide(event.devicePath);
      DeviceHandler.Notification.DEVICE_FAIL.hide(event.devicePath);
      DeviceHandler.Notification.DEVICE_EXTERNAL_STORAGE_DISABLED.hide(
          event.devicePath);
      delete this.mountStatus_[event.devicePath];
      break;
    case 'format_start':
      DeviceHandler.Notification.FORMAT_START.show(event.devicePath);
      break;
    case 'format_success':
      DeviceHandler.Notification.FORMAT_START.hide(event.devicePath);
      DeviceHandler.Notification.FORMAT_SUCCESS.show(event.devicePath);
      break;
    case 'format_fail':
      DeviceHandler.Notification.FORMAT_START.hide(event.devicePath);
      DeviceHandler.Notification.FORMAT_FAIL.show(event.devicePath);
      break;
  }
};

/**
 * Mount status for the device.
 * Each multi-partition devices can obtain multiple mount completed events.
 * This status shows what results are already obtained for the device.
 * @enum {string}
 * @const
 */
DeviceHandler.MountStatus = Object.freeze({
  // There is no mount results on the device.
  NO_RESULT: 'noResult',
  // There is no error on the device.
  SUCCESS: 'success',
  // There is only parent errors, that can be overridden by child results.
  ONLY_PARENT_ERROR: 'onlyParentError',
  // There is one child error.
  CHILD_ERROR: 'childError',
  // There is multiple child results and at least one is failure.
  MULTIPART_ERROR: 'multipartError'
});

/**
 * Handles mount completed events to show notifications for removable devices.
 * @param {MountCompletedEvent} event Mount completed event.
 * @private
 */
DeviceHandler.prototype.onMountCompleted_ = function(event) {
  // If this is remounting, which happens when resuming ChromeOS, the device has
  // already inserted to the computer. So we suppress the notification.
  var volume = event.volumeMetadata;
  if (!volume.deviceType || event.isRemounting)
    return;

  var getFirstStatus = function(event) {
    if (event.status === 'success')
      return DeviceHandler.MountStatus.SUCCESS;
    else if (event.volumeMetadata.isParentDevice)
      return DeviceHandler.MountStatus.ONLY_PARENT_ERROR;
    else
      return DeviceHandler.MountStatus.CHILD_ERROR;
  };

  // Update the current status.
  switch (this.mountStatus_[volume.devicePath]) {
    // If there is no related device, do nothing.
    case undefined:
      return;
    // If the multipart error message has already shown, do nothing because the
    // message does not changed by the following mount results.
    case DeviceHandler.MULTIPART_ERROR:
      return;
    // If this is the first result, hide the scanning notification.
    case DeviceHandler.MountStatus.NO_RESULT:
      DeviceHandler.Notification.DEVICE.hide(volume.devicePath);
      this.mountStatus_[volume.devicePath] = getFirstStatus(event);
      break;
    // If there are only parent errors, and the new result is child's one, hide
    // the parent error. (parent device contains partition table, which is
    // unmountable)
    case DeviceHandler.MountStatus.ONLY_PARENT_ERROR:
      if (!volume.isParentDevice)
        DeviceHandler.Notification.DEVICE_FAIL.hide(volume.devicePath);
      this.mountStatus_[volume.devicePath] = getFirstStatus(event);
      break;
    // We have a multi-partition device for which at least one mount
    // failed.
    case DeviceHandler.MountStatus.SUCCESS:
    case DeviceHandler.MountStatus.CHILD_ERROR:
      if (this.mountStatus_[volume.devicePath] ===
              DeviceHandler.MountStatus.SUCCESS &&
          event.status === 'success') {
        this.mountStatus_[volume.devicePath] =
            DeviceHandler.MountStatus.SUCCESS;
      } else {
        this.mountStatus_[volume.devicePath] =
            DeviceHandler.MountStatus.MULTIPART_ERROR;
      }
      break;
  }

  // Show the notification for the current errors.
  // If there is no error, do not show/update the notification.
  var message;
  switch (this.mountStatus_[volume.devicePath]) {
    case DeviceHandler.MountStatus.MULTIPART_ERROR:
      message = volume.deviceLabel ?
          strf('MULTIPART_DEVICE_UNSUPPORTED_MESSAGE', volume.deviceLabel) :
          str('MULTIPART_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE');
      break;
    case DeviceHandler.MountStatus.CHILD_ERROR:
    case DeviceHandler.MountStatus.ONLY_PARENT_ERROR:
      if (event.status === 'error_unsuported_filesystem') {
        message = volume.deviceLabel ?
            strf('DEVICE_UNSUPPORTED_MESSAGE', volume.deviceLabel) :
            str('DEVICE_UNSUPPORTED_DEFAULT_MESSAGE');
      } else {
        message = volume.deviceLabel ?
            strf('DEVICE_UNKNOWN_MESSAGE', volume.deviceLabel) :
            str('DEVICE_UNKNOWN_DEFAULT_MESSAGE');
      }
      break;
  }
  if (message) {
    DeviceHandler.Notification.DEVICE_FAIL.hide(volume.devicePath);
    DeviceHandler.Notification.DEVICE_FAIL.show(volume.devicePath, message);
  }
};
