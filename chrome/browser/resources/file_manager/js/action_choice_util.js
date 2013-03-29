// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Namespace object for the utilities.
var ActionChoiceUtil = {};

/**
 * Gets list of defined actions.
 * @param {Object} loadTimeData Load time data for i18n.
 * @param {function(Array.<Object>)} callback Callback with list of defined
 *     actions.
 */
ActionChoiceUtil.getDefinedActions = function(loadTimeData, callback) {
  // Fill out predefined actions first.
  var result = [{
    id: 'view-files',
    title: loadTimeData.getString('ACTION_CHOICE_VIEW_FILES'),
    class: 'view-files-icon'
  }, {
    id: 'import-photos-to-drive',
    title: loadTimeData.getString('ACTION_CHOICE_PHOTOS_DRIVE'),
    class: 'import-photos-to-drive-icon',
    disabled: true,
    disabledTitle: loadTimeData.getString('ACTION_CHOICE_DRIVE_NOT_REACHED')
  }, {
    id: 'watch-single-video',
    class: 'watch-single-video-icon',
    hidden: true,
    disabled: true
  }];
  chrome.mediaGalleriesPrivate.getHandlers(function(handlers) {
    for (var i = 0; i < handlers.length; i++) {
      var action = {
        id: handlers[i].extensionId + ':' + handlers[i].id,
        title: handlers[i].title,
        // TODO(mtomasz): Get the passed icon instead of the extension icon.
        icon100:
            'chrome://extension-icon/' + handlers[i].extensionId + '/32/1',
        icon200:
            'chrome://extension-icon/' + handlers[i].extensionId + '/64/1',
        extensionId: handlers[i].extensionId,
        actionId: handlers[i].id
      };
      result.push(action);
    }
    callback(result);
  }.bind(this));
};

/**
 * Gets the remembered action's identifier.
 * @param {function(string=)} callback Callback with the identifier.
 */
ActionChoiceUtil.getRememberedActionId = function(callback) {
  util.storage.local.get(['action-choice'], function(result) {
    callback(result['action-choice']);
  });
};

/**
 * Sets the remembered action's identifier.
 * @param {string=} opt_actionId Action's identifier. If undefined, then forgets
 *     the remembered choice.
 */
ActionChoiceUtil.setRememberedActionId = function(opt_actionId) {
  util.storage.local.set({'action-choice': opt_actionId});
};
