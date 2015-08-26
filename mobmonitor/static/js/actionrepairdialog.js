// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';


var ARGS_HELP_TEXT = 'Enter arguments as a comma separated list of the form: ' +
                     'arg1,arg2,...,argN.';

var ARGS_ACTION_HELP_TEXT = '\n\nYou must enter the arguments: ';

var KWARGS_HELP_TEXT = 'Enter default arguments as a comma separated list of ' +
                       'equal sign separated values of the form: ' +
                       'default1=value1,default2=value2,...,defaultN=valueN.';

var KWARGS_ACTION_HELP_TEXT = '\n\nYou may enter zero or more of the' +
                              ' following arguments: ';

var NO_ARGS_TEXT = '\n\nNo arguments for you to add.';


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
    width: 575,
    modal: true,

    close: function(event, ui) {
      $(this).dialog('destroy').remove();
    },

    buttons: {
      'Clear Actions': function() {
        actionRepairDialog.clear();
      },
      'Submit': function() {
        actionRepairDialog.submit();
      }
    }
  });

  // Commonly used elements of the dialog ui.
  var d = this.dialogElement_;
  this.stageButton = $(d).find('#stageButton')[0];
  this.dialogActionListDropdown = $(d).find('.actionlist-dropdown')[0];
  this.dialogActionDescription = $(d).find('#actionDescription')[0];
  this.dialogArgs = $(d).find('#args')[0];
  this.dialogArgsHelp = $(d).find('#argsHelp')[0];
  this.dialogKwargs = $(d).find('#kwargs')[0];
  this.dialogKwargsHelp = $(d).find('#kwargsHelp')[0];
  this.dialogStagedActions = $(d).find('#stagedActions')[0];

  // Set up click handlers.
  var self = this;
  $(this.dialogActionListDropdown).on('input change', function() {
    self.constructActionInfo(this.value);
  });
  $(this.stageButton).on('click', function() {
    self.stage();
  });

  // Set default action info.
  this.clear();
}

ActionRepairDialog.prototype.open = function() {
  this.dialogElement_.dialog('open');
};

ActionRepairDialog.prototype.close = function() {
  this.dialogElement_.dialog('close');
};

ActionRepairDialog.prototype.clearActionInfo = function() {
  // Clear action specific text.
  this.dialogActionListDropdown.value = '';
  this.dialogActionDescription.value = '';
  this.dialogArgs.value = '';
  this.dialogKwargs.value = '';
  $(this.dialogArgsHelp).text(ARGS_HELP_TEXT);
  $(this.dialogKwargsHelp).text(KWARGS_HELP_TEXT);

  // Disable action argument input.
  this.dialogArgs.disabled = true;
  this.dialogKwargs.disabled = true;
};

ActionRepairDialog.prototype.clear = function() {
  this.clearActionInfo();
  this.dialogStagedActions.value = '';
  this.actionsToSubmit = [];
};

ActionRepairDialog.prototype.constructActionInfo = function(action) {
  this.clearActionInfo();

  if (!action) {
    return;
  }

  var self = this;
  rpcActionInfo(this.service, action, function(response) {
    self.dialogActionListDropdown.value = action;
    self.dialogActionDescription.value = response.info;

    if (!isEmpty(response.args)) {
      $(self.dialogArgsHelp).text(ARGS_HELP_TEXT + ARGS_ACTION_HELP_TEXT +
                                  response.args.join(','));
      self.dialogArgs.disabled = false;
    }
    else {
      $(self.dialogArgsHelp).text(ARGS_HELP_TEXT + NO_ARGS_TEXT);
    }

    if (!isEmpty(response.kwargs)) {
      var defaults = [];
      Object.keys(response.kwargs).forEach(function(key) {
        defaults.push(key + '=' + response.kwargs[key]);
      });

      self.dialogKwargs.value = defaults.join(',');
      self.dialogKwargs.disabled = false;
      $(self.dialogKwargsHelp).text(KWARGS_HELP_TEXT + KWARGS_ACTION_HELP_TEXT +
                                    Object.keys(response.kwargs).join(','));
    }
    else {
      $(self.dialogKwargsHelp).text(KWARGS_HELP_TEXT + NO_ARGS_TEXT);
    }
  });
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

  // Store the validated action.
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

  // Add the action to the display.
  var stagedActionDisplay = 'action: ' + action +
                            ', args: ' + args +
                            ', defaults: ' + kwargs;

  if (!isEmpty(this.dialogStagedActions.value)) {
    this.dialogStagedActions.value += '\n';
  }

  this.dialogStagedActions.value += stagedActionDisplay;
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
