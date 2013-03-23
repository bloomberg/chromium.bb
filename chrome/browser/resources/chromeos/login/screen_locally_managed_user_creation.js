// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Locally managed user creation flow screen.
 */

cr.define('login', function() {

  var ManagerPod = cr.ui.define(function() {
    var node = $('managed-user-creation-flow-manager-template').cloneNode(true);
    node.removeAttribute('id');
    node.removeAttribute('hidden');
    return node;
  });

  ManagerPod.userImageSalt_ = {};

  /**
   * UI element for displaying single account in list of possible managers for
   * new locally managed user.
   * @type {Object}
   */
  ManagerPod.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      // Mousedown has to be used instead of click to be able to prevent 'focus'
      // event later.
      this.addEventListener('mousedown',
                            this.handleMouseDown_.bind(this));
      var screen = $('managed-user-creation-flow');
      var managerPod = this;
      this.passwordElement.addEventListener('keydown', function(e) {
        managerPod.passwordErrorElement.hidden = true;
      });
      this.passwordElement.addEventListener('keyup', function(e) {
        screen.updateContinueButton_();
      });
    },

    /**
     * Updates UI elements from user data.
     */
    update: function() {
      this.imageElement.src = 'chrome://userimage/' + this.user.username +
          '?id=' + ManagerPod.userImageSalt_[this.user.username];

      this.nameElement.textContent = this.user.displayName;
      this.emailElement.textContent = this.user.emailAddress;
    },

    showPasswordError: function() {
      this.passwordErrorElement.hidden = false;
    },

    /**
     * Brings focus to password field.
     */
    focusInput: function() {
      this.passwordElement.focus();
    },

    /**
     * Gets image element.
     * @type {!HTMLImageElement}
     */
    get imageElement() {
      return this.querySelector('.managed-user-creation-flow-manager-image');
    },

    /**
     * Gets name element.
     * @type {!HTMLDivElement}
     */
    get nameElement() {
      return this.querySelector('.managed-user-creation-flow-manager-name');
    },

    /**
     * Gets e-mail element.
     * @type {!HTMLDivElement}
     */
    get emailElement() {
      return this.querySelector('.managed-user-creation-flow-manager-email');
    },

    /**
     * Gets password element.
     * @type {!HTMLDivElement}
     */
    get passwordElement() {
      return this.querySelector('.managed-user-creation-flow-manager-password');
    },

    /**
     * Gets password error element.
     * @type {!HTMLDivElement}
     */
    get passwordErrorElement() {
      return this.
          querySelector('.managed-user-creation-flow-manager-wrong-password');
    },

    /**
     * Gets password enclosing block.
     * @type {!HTMLDivElement}
     */
    get passwordBlock() {
      return this.querySelector(
          '.managed-user-creation-flow-manager-password-block');
    },

    /** @override */
    handleMouseDown_: function(e) {
      this.parentNode.selectPod(this);
      // Prevent default so that we don't trigger 'focus' event.
      e.preventDefault();
    },

    /**
     * The user that this pod represents.
     * @type {!Object}
     */
    user_: undefined,
    get user() {
      return this.user_;
    },
    set user(userDict) {
      this.user_ = userDict;
      this.update();
    },
  };

  var ManagerPodList = cr.ui.define('managerList');

  /**
   * UI element for selecting manager account for new managed user.
   * @type {Object}
   */
  ManagerPodList.prototype = {
    __proto__: HTMLDivElement.prototype,

    selectedPod_: null,

    /** @override */
    decorate: function() {
    },

    /**
     * Returns all the pods in this pod list.
     * @type {NodeList}
     */
    get pods() {
      return this.children;
    },

    addPod: function(manager) {
      var managerPod = new ManagerPod({user: manager});
      this.appendChild(managerPod);
      managerPod.update();
    },

    clearPods: function() {
      this.innerHTML = '';
      this.selectedPod_ = null;
    },

    selectPod: function(podToSelect) {
      if (this.selectedPod_ == podToSelect) {
        podToSelect.focusInput();
        return;
      }
      this.selectedPod_ = podToSelect;
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (pod != podToSelect) {
          pod.classList.remove('focused');
          pod.passwordElement.value = '';
          pod.passwordBlock.hidden = true;
        }
      }
      podToSelect.classList.add('focused');
      podToSelect.passwordBlock.hidden = false;
      podToSelect.passwordElement.value = '';
      podToSelect.focusInput();
    },
  };

  /**
   * Creates a new screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var LocallyManagedUserCreationScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  LocallyManagedUserCreationScreen.register = function() {
    var screen = $('managed-user-creation-flow');
    LocallyManagedUserCreationScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  LocallyManagedUserCreationScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    lastVerifiedName_: null,
    lastIncorrectUserName_: null,
    managerList_: null,
    useManagerBasedCreationFlow_: false,

    /** @override */
    decorate: function() {
      this.managerList_ = new ManagerPodList();
      $('managed-user-creation-flow-managers-pane').
          appendChild(this.managerList_);

      var userNameField = $('managed-user-creation-flow-name');
      var passwordField = $('managed-user-creation-flow-password');
      var password2Field = $('managed-user-creation-flow-password-confirm');

      var creationScreen = this;

      userNameField.addEventListener('keydown', function(e) {
        if (e.keyIdentifier == 'Enter') {
          if (userNameField.value.length > 0)
            passwordField.focus();
          e.stopPropagation();
          return;
        }
        creationScreen.clearUserNameError_();
      });

      userNameField.addEventListener('keyup', function(e) {
        creationScreen.checkUserName_();
      });

      passwordField.addEventListener('keydown', function(e) {
        creationScreen.passwordErrorVisible = false;
        if (e.keyIdentifier == 'Enter') {
          if (passwordField.value.length > 0) {
            password2Field.focus();
            creationScreen.updateContinueButton_();
          }
          e.stopPropagation();
        }
      });

      password2Field.addEventListener('keydown', function(e) {
        creationScreen.passwordErrorVisible = false;
        if (e.keyIdentifier == 'Enter') {
          if (creationScreen.useManagerBasedCreationFlow_) {
            if (passwordField.value.length > 0) {
              if (creationScreen.managerList_.selectedPod_)
                creationScreen.managerList_.selectedPod_.focusInput();
              creationScreen.updateContinueButton_();
            }
          } else {
            if (creationScreen.updateContinueButton_())
              creationScreen.validateInputAndStartFlow_();
          }
          e.stopPropagation();
        }
      });

      password2Field.addEventListener('keyup', function(e) {
        creationScreen.updateContinueButton_();
      });

      passwordField.addEventListener('keyup', function(e) {
        creationScreen.updateContinueButton_();
      });
    },

    /**
     * Screen controls.
     * @type {!Array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var proceedButton = this.ownerDocument.createElement('button');
      proceedButton.id = 'managed-user-creation-flow-proceed-button';

      proceedButton.textContent = loadTimeData.
          getString('managedUserCreationFlowProceedButtonTitle');
      proceedButton.hidden = true;
      buttons.push(proceedButton);

      var finishButton = this.ownerDocument.createElement('button');
      finishButton.id = 'managed-user-creation-flow-finish-button';

      finishButton.textContent = loadTimeData.
          getString('managedUserCreationFlowFinishButtonTitle');
      finishButton.hidden = true;
      buttons.push(finishButton);

      var retryButton = this.ownerDocument.createElement('button');
      retryButton.id = 'managed-user-creation-flow-retry-button';

      retryButton.textContent = loadTimeData.
          getString('managedUserCreationFlowRetryButtonTitle');
      retryButton.hidden = true;
      buttons.push(retryButton);

      var cancelButton = this.ownerDocument.createElement('button');
      cancelButton.id = 'managed-user-creation-flow-cancel-button';

      cancelButton.textContent = loadTimeData.
          getString('managedUserCreationFlowCancelButtonTitle');
      cancelButton.hidden = true;
      buttons.push(cancelButton);

      var creationFlowScreen = this;
      finishButton.addEventListener('click', function(e) {
        creationFlowScreen.finishFlow_();
        e.stopPropagation();
      });
      proceedButton.addEventListener('click', function(e) {
        creationFlowScreen.proceedFlow_();
        e.stopPropagation();
      });
      retryButton.addEventListener('click', function(e) {
        creationFlowScreen.retryFlow_();
        e.stopPropagation();
      });
      cancelButton.addEventListener('click', function(e) {
        creationFlowScreen.abortFlow_();
        e.stopPropagation();
      });

      return buttons;
    },

    /**
     * Does sanity check and calls backend with current user name/password pair
     * to create a user. May result in showPasswordError.
     * @private
     */
    validateInputAndStartFlow_: function() {
      var firstPassword = $('managed-user-creation-flow-password').value;
      var secondPassword =
          $('managed-user-creation-flow-password-confirm').value;
      var userName = $('managed-user-creation-flow-name').value;
      if (firstPassword != secondPassword) {
        this.showPasswordError(
            loadTimeData.getString('createManagedUserPasswordMismatchError'));
        return;
      }
      if (!this.useManagerBasedCreationFlow_) {
        this.disabled = true;
        chrome.send('tryCreateLocallyManagedUser', [userName, firstPassword]);
      } else {
        var selectedPod = this.managerList_.selectedPod_;
        if (null == selectedPod)
          return;

        var managerId = selectedPod.user.emailAddress;
        var managerPassword = selectedPod.passwordElement.value;
        this.disabled = true;
        // TODO(antrim) : we might use some minimal password validation
        // (e.g. non-empty etc.) here.
        chrome.send('runLocallyManagedUserCreationFlow',
            [userName, firstPassword, managerId, managerPassword]);
      }
    },

    /**
     * Calls backend part to check if current user name is valid/not taken.
     * Results in call to either managedUserNameOk or managedUserNameError.
     * @private
     */
    checkUserName_: function() {
      var userName = $('managed-user-creation-flow-name').value;

      // Avoid flickering
      if (userName == this.lastIncorrectUserName_ ||
          userName == this.lastVerifiedName_) {
        return;
      }
      if (userName.length > 0) {
        chrome.send('checkLocallyManagedUserName', [userName]);
      } else {
        this.nameErrorVisible = false;
        this.lastVerifiedName_ = null;
        this.lastIncorrectUserName_ = null;
      }
    },

    /**
     * Called by backend part in case of successful name validation.
     * @param {string} name - name that was validated.
     */
    managedUserNameOk: function(name) {
      this.lastVerifiedName_ = name;
      this.lastIncorrectUserName_ = null;
      if ($('managed-user-creation-flow-name').value == name)
        this.clearUserNameError_();
      this.updateContinueButton_();
    },

    /**
     * Called by backend part in case of name validation failure.
     * @param {string} name - name that was validated.
     * @param {string} errorText - reason why this name is invalid.
     */
    managedUserNameError: function(name, errorText) {
      this.lastIncorrectUserName_ = name;
      this.lastVerifiedName_ = null;

      var userNameField = $('managed-user-creation-flow-name');
      if (userNameField.value == this.lastIncorrectUserName_) {
        this.nameErrorVisible = true;
        $('managed-user-creation-flow-name-error').textContent = errorText;

        this.setButtonDisabledStatus('proceed', true);
      }
    },

    /**
     * Clears user name error, if name is no more guaranteed to be invalid.
     * @private
     */
    clearUserNameError_: function() {
      // Avoid flickering
      if ($('managed-user-creation-flow-name').value ==
              this.lastIncorrectUserName_) {
        return;
      }
      this.nameErrorVisible = false;
    },

    /**
     * Called by backend part in case of password validation failure.
     * @param {string} errorText - reason why this password is invalid.
     */
    showPasswordError: function(errorText) {
      $('managed-user-creation-flow-password-error').textContent = errorText;
      this.passwordErrorVisible = true;
      $('managed-user-creation-flow-password').focus();

      this.setButtonDisabledStatus('proceed', true);
    },

    /**
     * True if user name error should be displayed.
     * @type {boolean}
     */
    set nameErrorVisible(value) {
      $('managed-user-creation-flow-name-error').
          classList[value ? 'add' : 'remove']('error');
      $('managed-user-creation-flow-name').
          classList[value ? 'add' : 'remove']('duplicate-name');
      if (!value)
        $('managed-user-creation-flow-name-error').textContent = '';
    },

    /**
     * True if user name error should be displayed.
     * @type {boolean}
     */
    set passwordErrorVisible(value) {
      $('managed-user-creation-flow-password-error').
          classList[value ? 'add' : 'remove']('error');
      if (!value)
        $('managed-user-creation-flow-password-error').textContent = '';
    },

    /**
     * Updates state of Continue button after minimal checks.
     * @return {boolean} true, if form seems to be valid.
     * @private
     */
    updateContinueButton_: function() {
      var firstPassword = $('managed-user-creation-flow-password').value;
      var secondPassword =
          $('managed-user-creation-flow-password-confirm').value;
      var userName = $('managed-user-creation-flow-name').value;

      var canProceed =
          (firstPassword.length > 0) &&
          (firstPassword.length == secondPassword.length) &&
           this.lastVerifiedName_ &&
           (userName == this.lastVerifiedName_);

      if (this.useManagerBasedCreationFlow_) {
        var selectedPod = this.managerList_.selectedPod_;
        canProceed = canProceed &&
            null != selectedPod &&
            selectedPod.passwordElement.value.length > 0;
      }

      this.setButtonDisabledStatus('proceed', !canProceed);
      return canProceed;
    },

    showSelectedManagerPasswordError_: function() {
      var selectedPod = this.managerList_.selectedPod_;
      selectedPod.showPasswordError();
      selectedPod.passwordElement.value = '';
      selectedPod.focusInput();
    },

    /**
     * Show final splash screen with success message.
     */
    showFinishedMessage: function() {
      this.setVisiblePage_('success');
      this.setVisibleButtons_(['finish']);
    },

    /**
     * Show error message.
     * @param {string} errorText Text to be displayed.
     * @param {boolean} recoverable Indicates if error was transiend and process
     *     can be retried.
     */
    showErrorMessage: function(errorText, recoverable) {
      $('managed-user-creation-flow-error-value').innerHTML = errorText;
      this.setVisiblePage_('error');
      this.setVisibleButtons_(recoverable ? ['retry', 'cancel'] : ['cancel']);
    },

    /**
     * Enables one particular subpage and hides the rest.
     * @param {string} visiblePage - name of subpage (one of 'progress',
     * 'error', 'success')
     * @private
     */
    setVisiblePage_: function(visiblePage) {
      var screenNames = ['initial', 'progress', 'error', 'success'];
      for (i in screenNames) {
        var screenName = screenNames[i];
        var screen = $('managed-user-creation-flow-' + screenName);
        screen.hidden = (screenName != visiblePage);
      }
    },

    /**
     * Enables specific control buttons.
     * @param {List of strings} buttonsList - list of buttons to display (values
     * can be 'retry', 'finish', 'cancel')
     * @private
     */
    setVisibleButtons_: function(buttonsList) {
      var buttonNames = ['proceed', 'retry', 'finish', 'cancel'];
      for (i in buttonNames) {
        var buttonName = buttonNames[i];
        var button = $('managed-user-creation-flow-' + buttonName + '-button');
        if (button)
          button.hidden = buttonsList.indexOf(buttonName) < 0;
      }
    },

    setButtonDisabledStatus: function(buttonName, status) {
      var button = $('managed-user-creation-flow-' + buttonName + '-button');
      button.disabled = status;
    },

    finishFlow_: function() {
      chrome.send('finishLocalManagedUserCreation');
    },

    proceedFlow_: function() {
      this.validateInputAndStartFlow_();
    },

    retryFlow_: function() {
      this.setVisiblePage_('progress');
      chrome.send('retryLocalManagedUserCreation');
    },

    abortFlow_: function() {
      chrome.send('abortLocalManagedUserCreation');
    },

    /**
     * Updates state of login header so that necessary buttons are displayed.
     **/
    onBeforeShow: function(data) {
      $('login-header-bar').signinUIState =
          SIGNIN_UI_STATE.MANAGED_USER_CREATION_FLOW;
      if (data['managers']) {
        this.loadManagers(data['managers']);
      }
    },

    /**
     * Update state of login header so that necessary buttons are displayed.
     */
    onBeforeHide: function() {
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.HIDDEN;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('managed-user-creation-flow-name');
    },

    /**
     * True if the the screen is disabled (handles no user interaction).
     * @type {boolean}
     */
    disabled_: false,

    get disabled() {
      return this.disabled_;
    },

    set disabled(value) {
      this.disabled_ = value;
      var controls = this.querySelectorAll('button,input');
      for (var i = 0, control; control = controls[i]; ++i) {
        control.disabled = value;
      }
      $('login-header-bar').disabled = value;
    },

    /**
     * Called by backend part to propagate list of possible managers.
     * @param {Array} userList - list of users that can be managers.
     */
    loadManagers: function(userList) {
      $('managed-user-creation-flow-managers-block').hidden = false;
      this.useManagerBasedCreationFlow_ = true;
      this.managerList_.clearPods();
      for (var i = 0; i < userList.length; ++i)
        this.managerList_.addPod(userList[i]);
      if (userList.length > 0)
        this.managerList_.selectPod(this.managerList_.pods[0]);
    },
  };

  LocallyManagedUserCreationScreen.showProgressScreen = function() {
    var screen = $('managed-user-creation-flow');
    screen.disabled = false;
    screen.setVisiblePage_('progress');
    screen.setVisibleButtons_(['cancel']);
  };

  LocallyManagedUserCreationScreen.showIntialScreen = function() {
    var screen = $('managed-user-creation-flow');

    $('managed-user-creation-flow-password').value = '';
    $('managed-user-creation-flow-password-confirm').value = '';
    $('managed-user-creation-flow-name').value = '';

    screen.lastVerifiedName_ = null;
    screen.lastIncorrectUserName_ = null;
    screen.passwordErrorVisible = false;
    screen.nameErrorVisible = false;

    screen.setButtonDisabledStatus('proceed', true);

    screen.setVisiblePage_('initial');
    screen.setVisibleButtons_(['proceed', 'cancel']);
  };

  LocallyManagedUserCreationScreen.showFinishedMessage = function() {
    var screen = $('managed-user-creation-flow');
    screen.disabled = false;
    screen.showFinishedMessage();
  };

  LocallyManagedUserCreationScreen.showManagerPasswordError = function() {
    var screen = $('managed-user-creation-flow');
    screen.disabled = false;
    screen.showSelectedManagerPasswordError_();
  };

  LocallyManagedUserCreationScreen.showErrorMessage = function(errorText,
                                                               recoverable) {
    var screen = $('managed-user-creation-flow');
    screen.disabled = false;
    screen.showErrorMessage(errorText, recoverable);
  };

  LocallyManagedUserCreationScreen.managedUserNameOk = function(name) {
    var screen = $('managed-user-creation-flow');
    screen.managedUserNameOk(name);
  };

  LocallyManagedUserCreationScreen.managedUserNameError =
      function(name, error) {
    var screen = $('managed-user-creation-flow');
    screen.managedUserNameError(name, error);
  };

  LocallyManagedUserCreationScreen.showPasswordError = function(error) {
    var screen = $('managed-user-creation-flow');
    screen.showPasswordError(error);
  };

  LocallyManagedUserCreationScreen.loadManagers = function(userList) {
    var screen = $('managed-user-creation-flow');
    screen.loadManagers(userList);
  };

  return {
    LocallyManagedUserCreationScreen: LocallyManagedUserCreationScreen
  };
});

