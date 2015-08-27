// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for implementing earcons.
 *
 * When adding earcons, please add them to getEarconName and getEarconId.
 *
 */

goog.provide('cvox.AbstractEarcons');


/**
 * @constructor
 */
cvox.AbstractEarcons = function() {
  /**
   * Public flag set to enable or disable earcons. Callers should prefer
   * toggle(); however, this member is public for initialization.
   * @type {boolean}
   */
  this.enabled = true;
};


/**
 * Plays the specified earcon sound.
 * @param {number} earcon An earcon index.
 */
cvox.AbstractEarcons.prototype.playEarcon = function(earcon) {
};


/**
 * Plays the specified earcon sound, given the name of the earcon.
 * @param {string} earconName The name of the earcon.
 */
cvox.AbstractEarcons.prototype.playEarconByName = function(earconName) {
  this.playEarcon(this.getEarconId(earconName));
};


/**
 * Whether or not earcons are available.
 * @return {boolean} True if earcons are available.
 */
cvox.AbstractEarcons.prototype.earconsAvailable = function() {
  return true;
};


/**
 * @param {number} earcon An earcon index.
 * @return {string} The readable earcon name.
 */
cvox.AbstractEarcons.prototype.getEarconName = function(earcon) {
  if (!this.earconNames) {
    this.earconNames = new Array();
    this.earconNames.push('ALERT_MODAL');
    this.earconNames.push('ALERT_NONMODAL');
    this.earconNames.push('BULLET');
    this.earconNames.push('BUSY_PROGRESS_LOOP');
    this.earconNames.push('BUTTON');
    this.earconNames.push('CHECK_OFF');
    this.earconNames.push('CHECK_ON');
    this.earconNames.push('EDITABLE_TEXT');
    this.earconNames.push('FONT_CHANGE');
    this.earconNames.push('INVALID_KEYPRESS');
    this.earconNames.push('LINK');
    this.earconNames.push('LISTBOX');
    this.earconNames.push('LIST_ITEM');
    this.earconNames.push('LONG_DESC');
    this.earconNames.push('OBJECT_CLOSE');
    this.earconNames.push('OBJECT_ENTER');
    this.earconNames.push('OBJECT_EXIT');
    this.earconNames.push('OBJECT_OPEN');
    this.earconNames.push('OBJECT_SELECT');
    this.earconNames.push('PARAGRAPH_BREAK');
    this.earconNames.push('SELECTION');
    this.earconNames.push('SELECTION_REVERSE');
    this.earconNames.push('SPECIAL_CONTENT');
    this.earconNames.push('TASK_SUCCESS');
    this.earconNames.push('WRAP');
    this.earconNames.push('WRAP_EDGE');
  }
  return this.earconNames[earcon];
};


/**
 * @param {string} earconName An earcon name.
 * @return {number} The earcon ID.
 */
cvox.AbstractEarcons.prototype.getEarconId = function(earconName) {
  if (!this.earconNamesToIds) {
    this.earconNamesToIds = new Object();
    this.earconNamesToIds['ALERT_MODAL'] =
        cvox.AbstractEarcons.ALERT_MODAL;
    this.earconNamesToIds['ALERT_NONMODAL'] =
        cvox.AbstractEarcons.ALERT_NONMODAL;
    this.earconNamesToIds['BULLET'] = cvox.AbstractEarcons.BULLET;
    this.earconNamesToIds['BUSY_PROGRESS_LOOP'] =
        cvox.AbstractEarcons.BUSY_PROGRESS_LOOP;
    this.earconNamesToIds['BUTTON'] = cvox.AbstractEarcons.BUTTON;
    this.earconNamesToIds['CHECK_OFF'] = cvox.AbstractEarcons.CHECK_OFF;
    this.earconNamesToIds['CHECK_ON'] = cvox.AbstractEarcons.CHECK_ON;
    this.earconNamesToIds['EDITABLE_TEXT'] = cvox.AbstractEarcons.EDITABLE_TEXT;
    this.earconNamesToIds['FONT_CHANGE'] = cvox.AbstractEarcons.FONT_CHANGE;
    this.earconNamesToIds['INVALID_KEYPRESS'] =
        cvox.AbstractEarcons.INVALID_KEYPRESS;
    this.earconNamesToIds['LINK'] = cvox.AbstractEarcons.LINK;
    this.earconNamesToIds['LISTBOX'] = cvox.AbstractEarcons.LISTBOX;
    this.earconNamesToIds['LIST_ITEM'] = cvox.AbstractEarcons.LIST_ITEM;
    this.earconNamesToIds['LONG_DESC'] = cvox.AbstractEarcons.LONG_DESC;
    this.earconNamesToIds['OBJECT_CLOSE'] = cvox.AbstractEarcons.OBJECT_CLOSE;
    this.earconNamesToIds['OBJECT_ENTER'] = cvox.AbstractEarcons.OBJECT_ENTER;
    this.earconNamesToIds['OBJECT_EXIT'] = cvox.AbstractEarcons.OBJECT_EXIT;
    this.earconNamesToIds['OBJECT_OPEN'] = cvox.AbstractEarcons.OBJECT_OPEN;
    this.earconNamesToIds['OBJECT_SELECT'] = cvox.AbstractEarcons.OBJECT_SELECT;
    this.earconNamesToIds['PARAGRAPH_BREAK'] =
        cvox.AbstractEarcons.PARAGRAPH_BREAK;
    this.earconNamesToIds['SELECTION'] = cvox.AbstractEarcons.SELECTION;
    this.earconNamesToIds['SELECTION_REVERSE'] =
        cvox.AbstractEarcons.SELECTION_REVERSE;
    this.earconNamesToIds['SPECIAL_CONTENT'] =
        cvox.AbstractEarcons.SPECIAL_CONTENT;
    this.earconNamesToIds['TASK_SUCCESS'] = cvox.AbstractEarcons.TASK_SUCCESS;
    this.earconNamesToIds['WRAP'] = cvox.AbstractEarcons.WRAP;
    this.earconNamesToIds['WRAP_EDGE'] = cvox.AbstractEarcons.WRAP_EDGE;
  }
  return this.earconNamesToIds[earconName];
};


/**
 * @param {number} earconId The earcon ID.
 * @return {string} The filename for the earcon.
 */
cvox.AbstractEarcons.prototype.getEarconFilename = function(earconId) {
  return cvox.AbstractEarcons.earconMap[earconId];
};


/**
 * Toggles earcons on or off.
 * @return {boolean} True if earcons are now enabled; false otherwise.
 */
cvox.AbstractEarcons.prototype.toggle = function() {
  this.enabled = !this.enabled;
  return this.enabled;
};


/**
 * @type {number}
 */
cvox.AbstractEarcons.ALERT_MODAL = 0;

/**
 * @type {number}
 */
cvox.AbstractEarcons.ALERT_NONMODAL = 1;

/**
 * @type {number}
 */
cvox.AbstractEarcons.BULLET = 2;

/**
 * @type {number}
 */
cvox.AbstractEarcons.BUSY_PROGRESS_LOOP = 3;

/**
 * @type {number}
 */
cvox.AbstractEarcons.BUTTON = 4;

/**
 * @type {number}
 */
cvox.AbstractEarcons.CHECK_OFF = 5;

/**
 * @type {number}
 */
cvox.AbstractEarcons.CHECK_ON = 6;

/**
 * @type {number}
 */
cvox.AbstractEarcons.EDITABLE_TEXT = 7;

/**
 * @type {number}
 */
cvox.AbstractEarcons.FONT_CHANGE = 8;

/**
 * @type {number}
 */
cvox.AbstractEarcons.INVALID_KEYPRESS = 9;

/**
 * @type {number}
 */
cvox.AbstractEarcons.LINK = 10;

/**
 * @type {number}
 */
cvox.AbstractEarcons.LISTBOX = 11;

/**
 * @type {number}
 */
cvox.AbstractEarcons.LIST_ITEM = 12;

/**
 * @type {number}
 */
cvox.AbstractEarcons.LONG_DESC = 13;

/**
 * @type {number}
 */
cvox.AbstractEarcons.OBJECT_CLOSE = 14;

/**
 * @type {number}
 */
cvox.AbstractEarcons.OBJECT_ENTER = 15;

/**
 * @type {number}
 */
cvox.AbstractEarcons.OBJECT_EXIT = 16;

/**
 * @type {number}
 */
cvox.AbstractEarcons.OBJECT_OPEN = 17;

/**
 * @type {number}
 */
cvox.AbstractEarcons.OBJECT_SELECT = 18;

/**
 * @type {number}
 */
cvox.AbstractEarcons.PARAGRAPH_BREAK = 19;

/**
 * @type {number}
 */
cvox.AbstractEarcons.SELECTION = 20;

/**
 * @type {number}
 */
cvox.AbstractEarcons.SELECTION_REVERSE = 21;

/**
 * @type {number}
 */
cvox.AbstractEarcons.SPECIAL_CONTENT = 22;

/**
 * @type {number}
 */
cvox.AbstractEarcons.TASK_SUCCESS = 23;

/**
 * @type {number}
 */
cvox.AbstractEarcons.WRAP = 24;

/**
 * @type {number}
 */
cvox.AbstractEarcons.WRAP_EDGE = 25;

/**
 * The earcon map.
 * @type {Object}
 */
cvox.AbstractEarcons.earconMap = new Object();
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.ALERT_NONMODAL] =
    'alert_nonmodal.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.BULLET] = 'bullet.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.BUSY_PROGRESS_LOOP] =
    'busy_progress_loop.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.BUTTON] = 'button.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.CHECK_OFF] =
    'check_off.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.CHECK_ON] = 'check_on.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.EDITABLE_TEXT] =
    'editable_text.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.FONT_CHANGE] =
    'font_change.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.LINK] = 'link.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.LISTBOX] = 'listbox.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.LIST_ITEM] = 'bullet.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.LONG_DESC] =
    'long_desc.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.OBJECT_CLOSE] =
    'object_close.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.OBJECT_ENTER] =
    'object_enter.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.OBJECT_EXIT] =
    'object_exit.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.OBJECT_OPEN] =
    'object_open.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.OBJECT_SELECT] =
    'object_select.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.PARAGRAPH_BREAK] =
    'paragraph_break.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.SELECTION] =
    'selection.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.SELECTION_REVERSE] =
    'selection_reverse.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.SPECIAL_CONTENT] =
    'special_content.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.TASK_SUCCESS] =
    'task_success.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.WRAP] = 'wrap.ogg';
cvox.AbstractEarcons.earconMap[cvox.AbstractEarcons.WRAP_EDGE] =
    'wrap_edge.ogg';
