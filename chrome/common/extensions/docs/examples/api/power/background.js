// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Available levels of power-saving-overriding.
 */
var LevelEnum = {
  DISABLED: '',
  DISPLAY: 'display',
  SYSTEM: 'system'
};

/**
 * Key used for storing the current level in {localStorage}.
 */
var LEVEL_KEY = 'level';

/**
 * Current {LevelEnum}.
 */
var currentLevel = LevelEnum.DISABLED;

/**
 * Should the old {chrome.experimental.power} API be used rather than
 * {chrome.power}?
 */
var useOldApi = !chrome.power;

/**
 * Returns the previously-used level.
 * @return {string} Saved {LevelEnum} from local storage.
 */
function getInitialLevel() {
  if (LEVEL_KEY in localStorage) {
    var savedLevel = localStorage[LEVEL_KEY];
    for (var key in LevelEnum) {
      if (savedLevel == LevelEnum[key]) {
        return savedLevel;
      }
    }
  }
  return LevelEnum.DISABLED;
}

/**
 * Switches to a new power-saving-overriding level.
 * @param {string} newLevel New {LevelEnum} to use.
 */
function setLevel(newLevel) {
  var imagePrefix = 'night';
  var title = '';

  // The old API doesn't support the "system" level.
  if (useOldApi && newLevel == LevelEnum.SYSTEM)
    newLevel = LevelEnum.DISPLAY;

  switch (newLevel) {
    case LevelEnum.DISABLED:
      (useOldApi ? chrome.experimental.power : chrome.power).releaseKeepAwake();
      imagePrefix = 'night';
      title = chrome.i18n.getMessage('disabledTitle');
      break;
    case LevelEnum.DISPLAY:
      if (useOldApi)
        chrome.experimental.power.requestKeepAwake(function() {});
      else
        chrome.power.requestKeepAwake('display');
      imagePrefix = 'day';
      title = chrome.i18n.getMessage('displayTitle');
      break;
    case LevelEnum.SYSTEM:
      chrome.power.requestKeepAwake('system');
      imagePrefix = 'sunset';
      title = chrome.i18n.getMessage('systemTitle');
      break;
    default:
      throw 'Invalid level "' + newLevel + '"';
  }

  currentLevel = newLevel;
  localStorage[LEVEL_KEY] = currentLevel;

  chrome.browserAction.setIcon({
    path: {
      '19': 'images/' + imagePrefix + '-19.png',
      '38': 'images/' + imagePrefix + '-38.png'
    }
  });
  chrome.browserAction.setTitle({title: title});
}

/**
 * Cycles levels in response to browser action icon clicks.
 */
function handleClicked() {
  switch (currentLevel) {
    case LevelEnum.DISABLED:
      setLevel(LevelEnum.DISPLAY);
      break;
    case LevelEnum.DISPLAY:
      setLevel(useOldApi ? LevelEnum.DISABLED : LevelEnum.SYSTEM);
      break;
    case LevelEnum.SYSTEM:
      setLevel(LevelEnum.DISABLED);
      break;
    default:
      throw 'Invalid level "' + currentLevel + '"';
  }
}

chrome.browserAction.onClicked.addListener(handleClicked);
setLevel(getInitialLevel());
