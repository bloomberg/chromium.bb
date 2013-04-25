// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Locally managed user creation flow screen.
 */

login.createScreen('LocallyManagedUserCreationScreen',
                   'managed-user-creation-flow', function() {
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
        screen.updateNextButtonForManager_();
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
      chrome.send('managerSelectedOnLocallyManagedUserCreationFlow',
          [podToSelect.user.emailAddress]);

    },
  };

  return {
    EXTERNAL_API: [
      'loadManagers',
      'managedUserNameError',
      'managedUserNameOk',
      'showErrorPage',
      'showIntroPage',
      'showManagerPage',
      'showManagerPasswordError',
      'showPasswordError',
      'showProgressPage',
      'showTutorialPage',
      'showUsernamePage',
    ],

    lastVerifiedName_: null,
    lastIncorrectUserName_: null,
    managerList_: null,

    currentPage_: null,

    // Contains data that can be auto-shared with handler.
    context_: {},

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
            creationScreen.updateNextButtonForUser_();
          }
          e.stopPropagation();
        }
      });

      password2Field.addEventListener('keydown', function(e) {
        creationScreen.passwordErrorVisible = false;
        if (e.keyIdentifier == 'Enter') {
          if (passwordField.value.length > 0) {
            if (creationScreen.managerList_.selectedPod_)
              creationScreen.managerList_.selectedPod_.focusInput();
            creationScreen.updateNextButtonForUser_();
          }
          e.stopPropagation();
        }
      });

      password2Field.addEventListener('keyup', function(e) {
        creationScreen.updateNextButtonForUser_();
      });

      passwordField.addEventListener('keyup', function(e) {
        creationScreen.updateNextButtonForUser_();
      });
    },

    /**
     * Screen controls.
     * @type {!Array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var startButton = this.ownerDocument.createElement('button');
      startButton.id = 'managed-user-creation-flow-start-button';

      startButton.textContent = loadTimeData.
          getString('managedUserCreationFlowStartButtonTitle');
      startButton.hidden = true;
      buttons.push(startButton);

      var previousButton = this.ownerDocument.createElement('button');
      previousButton.id = 'managed-user-creation-flow-prev-button';

      previousButton.textContent = loadTimeData.
          getString('managedUserCreationFlowPreviousButtonTitle');
      previousButton.hidden = true;
      buttons.push(previousButton);

      var nextButton = this.ownerDocument.createElement('button');
      nextButton.id = 'managed-user-creation-flow-next-button';

      nextButton.textContent = loadTimeData.
          getString('managedUserCreationFlowNextButtonTitle');
      nextButton.hidden = true;
      buttons.push(nextButton);

      var finishButton = this.ownerDocument.createElement('button');
      finishButton.id = 'managed-user-creation-flow-finish-button';

      finishButton.textContent = loadTimeData.
          getString('managedUserCreationFlowFinishButtonTitle');
      finishButton.hidden = true;
      buttons.push(finishButton);

      var cancelButton = this.ownerDocument.createElement('button');
      cancelButton.id = 'managed-user-creation-flow-cancel-button';

      var creationFlowScreen = this;
      finishButton.addEventListener('click', function(e) {
        creationFlowScreen.finishButtonPressed_();
        e.stopPropagation();
      });
      startButton.addEventListener('click', function(e) {
        creationFlowScreen.startButtonPressed_();
        e.stopPropagation();
      });
      nextButton.addEventListener('click', function(e) {
        creationFlowScreen.nextButtonPressed_();
        e.stopPropagation();
      });
      previousButton.addEventListener('click', function(e) {
        creationFlowScreen.prevButtonPressed_();
        e.stopPropagation();
      });

      return buttons;
    },

    /**
     * Does sanity check and calls backend with current user name/password pair
     * to authenticate manager. May result in showManagerPasswordError.
     * @private
     */
    validateAndLogInAsManager_: function() {
      var selectedPod = this.managerList_.selectedPod_;
      if (null == selectedPod)
        return;

      var managerId = selectedPod.user.emailAddress;
      var managerPassword = selectedPod.passwordElement.value;
      if (managerPassword.empty)
        return;

      this.disabled = true;
      this.context_.managerId = managerId;
      chrome.send('authenticateManagerInLocallyManagedUserCreationFlow',
          [managerId, managerPassword]);
    },


    /**
     * Does sanity check and calls backend with user display name/password pair
     * to create a user.
     * @private
     */
    validateAndCreateLocallyManagedUser_: function() {
      var firstPassword = $('managed-user-creation-flow-password').value;
      var secondPassword =
          $('managed-user-creation-flow-password-confirm').value;
      var userName = $('managed-user-creation-flow-name').value;
      if (firstPassword != secondPassword) {
        this.showPasswordError(
            loadTimeData.getString('createManagedUserPasswordMismatchError'));
        return;
      }
      this.disabled = true;
      this.context_.managedName = userName;
      chrome.send('specifyLocallyManagedUserCreationFlowUserData',
          [userName, firstPassword]);
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
      this.updateNextButtonForManager_();
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

        this.setButtonDisabledStatus('next', true);
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

      this.setButtonDisabledStatus('next', true);
    },

    /**
     * True if user name error should be displayed.
     * @type {boolean}
     */
    set nameErrorVisible(value) {
      $('managed-user-creation-flow-name-error').
          classList.toggle('error', value);
      $('managed-user-creation-flow-name').
          classList.toggle('duplicate-name', value);
      if (!value)
        $('managed-user-creation-flow-name-error').textContent = '';
    },

    /**
     * True if user name error should be displayed.
     * @type {boolean}
     */
    set passwordErrorVisible(value) {
      $('managed-user-creation-flow-password-error').
          classList.toggle('error', value);
      if (!value)
        $('managed-user-creation-flow-password-error').textContent = '';
    },

    /**
     * Updates state of Continue button after minimal checks.
     * @return {boolean} true, if form seems to be valid.
     * @private
     */
    updateNextButtonForManager_: function() {
      var selectedPod = this.managerList_.selectedPod_;
      canProceed = null != selectedPod &&
                   selectedPod.passwordElement.value.length > 0;

      this.setButtonDisabledStatus('next', !canProceed);
      return canProceed;
    },

    /**
     * Updates state of Continue button after minimal checks.
     * @return {boolean} true, if form seems to be valid.
     * @private
     */
    updateNextButtonForUser_: function() {
      var firstPassword = $('managed-user-creation-flow-password').value;
      var secondPassword =
          $('managed-user-creation-flow-password-confirm').value;
      var userName = $('managed-user-creation-flow-name').value;

      var canProceed =
          (firstPassword.length > 0) &&
          (firstPassword.length == secondPassword.length) &&
           this.lastVerifiedName_ &&
           (userName == this.lastVerifiedName_);

      this.setButtonDisabledStatus('next', !canProceed);
      return canProceed;
    },

    showSelectedManagerPasswordError_: function() {
      var selectedPod = this.managerList_.selectedPod_;
      selectedPod.showPasswordError();
      selectedPod.passwordElement.value = '';
      selectedPod.focusInput();
    },

    /**
     * Enables one particular subpage and hides the rest.
     * @param {string} visiblePage - name of subpage.
     * @private
     */
    setVisiblePage_: function(visiblePage) {
      this.disabled = false;
      this.updateText_();
      var screenNames = ['intro',
                         'manager',
                         'username',
                         'progress',
                         'error',
                         'tutorial'];
      for (i in screenNames) {
        var screenName = screenNames[i];
        var screen = $('managed-user-creation-flow-' + screenName);
        screen.hidden = (screenName != visiblePage);
        if (screenName == visiblePage) {
          $('step-logo').hidden = screen.classList.contains('step-no-logo');
        }
      }
      this.currentPage_ = visiblePage;
    },

    /**
     * Enables specific control buttons.
     * @param {List of strings} buttonsList - list of buttons to display.
     * @private
     */
    setVisibleButtons_: function(buttonsList) {
      var buttonNames = ['start',
                         'prev',
                         'next',
                         'cancel',
                         'finish'];
      for (i in buttonNames) {
        var buttonName = buttonNames[i];
        var button;
        if ('cancel' == buttonName)
          button = $('cancel-add-user-button');
        else
          button = $('managed-user-creation-flow-' + buttonName + '-button');
        if (button) {
          button.hidden = buttonsList.indexOf(buttonName) < 0;
          button.disabled = false;
        }
      }
    },

    setButtonDisabledStatus: function(buttonName, status) {
      var button = $('managed-user-creation-flow-' + buttonName + '-button');
      button.disabled = status;
    },

    finishButtonPressed_: function() {
      chrome.send('finishLocalManagedUserCreation');
    },

    startButtonPressed_: function() {
      this.setVisiblePage_('manager');
      this.setVisibleButtons_(['next', 'prev', 'cancel']);
      this.setButtonDisabledStatus('next', true);
    },

    nextButtonPressed_: function() {
      if (this.currentPage_ == 'manager') {
        this.validateAndLogInAsManager_();
        return;
      }
      if (this.currentPage_ == 'username') {
        this.validateAndCreateLocallyManagedUser_();
        return;
      }
    },

    prevButtonPressed_: function() {
      this.setVisiblePage_('intro');
      this.setVisibleButtons_(['start', 'cancel']);
      return;
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
      this.managerList_.clearPods();
      for (var i = 0; i < userList.length; ++i)
        this.managerList_.addPod(userList[i]);

      var usersInPane = Math.min(userList.length, 5);
      $('managed-user-creation-flow-managers-pane').style.height =
          (usersInPane * 60 + 28) + 'px';

      if (userList.length == 1)
        this.managerList_.selectPod(this.managerList_.pods[0]);
    },

    /**
     * Cancels user creation and drops to user screen (either sign).
     */
    cancel: function() {
      var notSignedInScreens = ['intro', 'manager'];
      if (notSignedInScreens.indexOf(this.currentPage_) >= 0) {
        // Make sure no manager password is kept:
        this.managerList_.clearPods();

        $('pod-row').loadLastWallpaper();

        Oobe.showScreen({id: SCREEN_ACCOUNT_PICKER});
        Oobe.resetSigninUI(true);
        return;
      }
      chrome.send('abortLocalManagedUserCreation');
    },

    updateText_: function() {
      $('managed-user-creation-flow-tutorial-created-text').textContent =
          loadTimeData.getStringF('managedUserProfileCreatedMessageTemplate',
              this.context_.managedName);

      var boldEmail = '<b>' + this.context_.managerId + '</b>';
      $('managed-user-creation-flow-tutorial-instructions-text').innerHTML =
          loadTimeData.getStringF('managedUserInstructionTemplate',
              boldEmail);
    },

    showIntroPage: function() {
      $('managed-user-creation-flow-password').value = '';
      $('managed-user-creation-flow-password-confirm').value = '';
      $('managed-user-creation-flow-name').value = '';

      this.lastVerifiedName_ = null;
      this.lastIncorrectUserName_ = null;
      this.passwordErrorVisible = false;
      this.nameErrorVisible = false;

      this.setVisiblePage_('intro');
      this.setVisibleButtons_(['start', 'cancel']);
    },

    showProgressPage: function() {
      this.setVisiblePage_('progress');
      this.setVisibleButtons_(['cancel']);
    },

    showManagerPage: function() {
      this.setVisiblePage_('manager');
      this.setVisibleButtons_(['cancel']);
    },

    showUsernamePage: function() {
      this.setVisiblePage_('username');
      this.setVisibleButtons_(['next', 'cancel']);
    },

    showTutorialPage: function() {
      this.setVisiblePage_('tutorial');
      this.setVisibleButtons_(['finish']);
    },

    showErrorPage: function(errorText, recoverable) {
      this.disabled = false;
      $('managed-user-creation-flow-error-value').innerHTML = errorText;
      this.setVisiblePage_('error');
      this.setVisibleButtons_(['cancel']);
    },

    showManagerPasswordError: function() {
      this.disabled = false;
      this.showSelectedManagerPasswordError_();
    }
  };
});

