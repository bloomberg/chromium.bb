// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';


function ActionRepairDialog(service, actions) {
  var actionRepairDialog = this;

  var templateData = {
    actions: actions
  };

  this.service = service;
  this.actionsToSubmit = [];

  this.dialogElement_ = $(
      renderTemplate('actionrepairdialog', templateData)).dialog({
    autoOpen: false,
    height: 525,
    width: 550,
    modal: true,

    close: function(event, ui) {
      $(this).dialog('destroy').remove();
    },

    buttons: {
      'Clear Actions': function() {
        actionRepairDialog.clear();
      },
      'Stage Action': function() {
        actionRepairDialog.stage();
      },
      'Submit': function() {
        actionRepairDialog.submit();
      }
    }
  });

  var d = this.dialogElement_;
  this.dialogActionListDropdown = $(d).find('.actionlist-dropdown')[0];
  this.dialogArgs = $(d).find('#args')[0];
  this.dialogKwargs = $(d).find('#kwargs')[0];
  this.dialogStagedActions = $(d).find('#stagedActions')[0];
}

ActionRepairDialog.prototype.open = function() {
  this.dialogElement_.dialog('open');
};

ActionRepairDialog.prototype.close = function() {
  this.dialogElement_.dialog('close');
};

ActionRepairDialog.prototype.clear = function() {
  this.dialogActionListDropdown.value = '';
  this.dialogArgs.value = '';
  this.dialogKwargs.value = '';
  this.dialogStagedActions.value = '';
  this.actionsToSubmit = [];
};

ActionRepairDialog.prototype.stage = function() {
  var action = this.dialogActionListDropdown.value;
  var args = this.dialogArgs.value;
  var kwargs = this.dialogKwargs.value;

  // Validate the action input.
  if (!action) {
    alert('An action must be selected to stage.');
    return;
  }

  if (args && !/^([^,]+,)*[^,]+$/g.test(args)) {
    alert('Arguments are not well-formed.\n' +
          'Expected form: a1,a2,...,aN');
    return;
  }

  if (kwargs && !/^([^,=]+=[^,=]+,)*[^,]+=[^,=]+$/g.test(kwargs)) {
    alert('Keyword argumetns are not well-formed.\n' +
          'Expected form: kw1=foo,...,kwN=bar');
    return;
  }

  // Store the action and add it to the staged action display.
  var storedArgs = args ? args.split(',') : [];
  var storedKwargs = {};
  kwargs.split(',').forEach(function(elem, index, array) {
    var kv = elem.split('=');
    storedKwargs[kv[0]] = kv[1];
  });

  var stagedAction = {
    action: action,
    args: storedArgs,
    kwargs: storedKwargs
  };

  this.actionsToSubmit.push(stagedAction);
  this.dialogStagedActions.value += JSON.stringify(stagedAction);
};

ActionRepairDialog.prototype.submit = function() {
  // Caller must define the function 'submitHandler' on the created dialog.
  // The submitHandler will be passed the following arguments:
  //  service: A string.
  //  action: A string.
  //  args: An array.
  //  kwargs: An object.

  if (!this.submitHandler) {
    alert('Caller must define submitHandler for ActionRepairDialog.');
    return;
  }

  if (isEmpty(this.actionsToSubmit)) {
    alert('Actions must be staged prior to submission.');
    return;
  }

  for (var i = 0; i < this.actionsToSubmit.length; i++) {
    var stagedAction = this.actionsToSubmit[i];
    this.submitHandler(this.service, stagedAction.action, stagedAction.args,
                       stagedAction.kwargs);
  }

  this.close();
};
