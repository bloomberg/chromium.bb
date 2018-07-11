// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const Buttons = {
  LEFT_BUTTON: 0,
  RIGHT_BUTTON: 2
};

const RecorderStateEnum = {
  // Not recording.
  STOPPED: 'stopped',
  // Recording, extension is showing the recorder UI on the page.
  SHOWN: 'shown',
  // Recording, extension is hiding the recorder UI on the page.
  HIDDEN: 'hidden'
};

const RecorderUiMsgEnum = {
  CREATE_UI: 'add-ui',
  DESTROY_UI: 'remove-ui',
  HIDE_UI: 'hide-ui',
  SHOW_UI: 'show-ui',
  ADD_ACTION: 'add-action',
  REMOVE_ACTION: 'remove-action',
  GET_RECIPE: 'get-recipe'
};

const RecorderMsgEnum = {
  SAVE: 'save-recording',
  START: 'start-recording',
  STOP: 'stop-recording',
  CANCEL: 'cancel-recording',
  GET_IFRAME_NAME: 'get-iframe-name',
  ADD_ACTION: 'record-action',
  MEMORIZE_PASSWORD_FORM: 'memorize-password-form',
};

const Local_Storage_Vars = {
  RECORDING_STATE: 'state',
  RECORDING_TAB_ID: 'target_tab_id',
  RECORDING_UI_FRAME_ID: 'ui_frame_id'
}

const Indexed_DB_Vars = {
  RECIPE_DB: 'Action_Recorder_Extension_Recipe',
  VERSION: 1,
  ACTIONS: 'Actions',
  ATTRIBUTES: 'Attributes',
  NAME: 'name',
  URL: 'url'
};
