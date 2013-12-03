// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Mock class of FileOperationManager.
 * @constructor
 */
function MockFileOperationManager() {
  cr.EventTarget.call(this);

  /**
   * Event to be dispatched when requestTaskCancel is called.
   * @type {Event}
   */
  this.cancelEvent = null;
}

MockFileOperationManager.prototype = {
  __proto__: cr.EventTarget.prototype
};

/**
 * Dispatches a pre-specified cancel event.
 */
MockFileOperationManager.prototype.requestTaskCancel = function() {
  this.dispatchEvent(this.cancelEvent);
};
