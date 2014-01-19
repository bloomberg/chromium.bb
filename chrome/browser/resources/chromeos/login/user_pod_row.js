// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="wallpaper_loader.js"></include>

/**
 * @fileoverview User pod row implementation.
 */

cr.define('login', function() {
  /**
   * Number of displayed columns depending on user pod count.
   * @type {Array.<number>}
   * @const
   */
  var COLUMNS = [0, 1, 2, 3, 4, 5, 4, 4, 4, 5, 5, 6, 6, 5, 5, 6, 6, 6, 6];

  /**
   * Mapping between number of columns in pod-row and margin between user pods
   * for such layout.
   * @type {Array.<number>}
   * @const
   */
  var MARGIN_BY_COLUMNS = [undefined, 40, 40, 40, 40, 40, 12];

  /**
   * Maximal number of columns currently supported by pod-row.
   * @type {number}
   * @const
   */
  var MAX_NUMBER_OF_COLUMNS = 6;

  /**
   * Variables used for pod placement processing.
   * Width and height should be synced with computed CSS sizes of pods.
   */
  var POD_WIDTH = 180;
  var POD_HEIGHT = 217;
  var POD_ROW_PADDING = 10;

  /**
   * Whether to preselect the first pod automatically on login screen.
   * @type {boolean}
   * @const
   */
  var PRESELECT_FIRST_POD = true;

  /**
   * Maximum time for which the pod row remains hidden until all user images
   * have been loaded.
   * @type {number}
   * @const
   */
  var POD_ROW_IMAGES_LOAD_TIMEOUT_MS = 3000;

  /**
   * Public session help topic identifier.
   * @type {number}
   * @const
   */
  var HELP_TOPIC_PUBLIC_SESSION = 3041033;

  /**
   * Oauth token status. These must match UserManager::OAuthTokenStatus.
   * @enum {number}
   * @const
   */
  var OAuthTokenStatus = {
    UNKNOWN: 0,
    INVALID_OLD: 1,
    VALID_OLD: 2,
    INVALID_NEW: 3,
    VALID_NEW: 4
  };

  /**
   * Tab order for user pods. Update these when adding new controls.
   * @enum {number}
   * @const
   */
  var UserPodTabOrder = {
    POD_INPUT: 1,     // Password input fields (and whole pods themselves).
    HEADER_BAR: 2,    // Buttons on the header bar (Shutdown, Add User).
    ACTION_BOX: 3,    // Action box buttons.
    PAD_MENU_ITEM: 4  // User pad menu items (Remove this user).
  };

  // Focus and tab order are organized as follows:
  //
  // (1) all user pods have tab index 1 so they are traversed first;
  // (2) when a user pod is activated, its tab index is set to -1 and its
  //     main input field gets focus and tab index 1;
  // (3) buttons on the header bar have tab index 2 so they follow user pods;
  // (4) Action box buttons have tab index 3 and follow header bar buttons;
  // (5) lastly, focus jumps to the Status Area and back to user pods.
  //
  // 'Focus' event is handled by a capture handler for the whole document
  // and in some cases 'mousedown' event handlers are used instead of 'click'
  // handlers where it's necessary to prevent 'focus' event from being fired.

  /**
   * Helper function to remove a class from given element.
   * @param {!HTMLElement} el Element whose class list to change.
   * @param {string} cl Class to remove.
   */
  function removeClass(el, cl) {
    el.classList.remove(cl);
  }

  /**
   * Creates a user pod.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UserPod = cr.ui.define(function() {
    var node = $('user-pod-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  /**
   * Stops event propagation from the any user pod child element.
   * @param {Event} e Event to handle.
   */
  function stopEventPropagation(e) {
    // Prevent default so that we don't trigger a 'focus' event.
    e.preventDefault();
    e.stopPropagation();
  }

  /**
   * Unique salt added to user image URLs to prevent caching. Dictionary with
   * user names as keys.
   * @type {Object}
   */
  UserPod.userImageSalt_ = {};

  UserPod.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      this.tabIndex = UserPodTabOrder.POD_INPUT;
      this.customButton.tabIndex = UserPodTabOrder.POD_INPUT;
      this.actionBoxAreaElement.tabIndex = UserPodTabOrder.ACTION_BOX;

      // Mousedown has to be used instead of click to be able to prevent 'focus'
      // event later.
      this.addEventListener('mousedown',
          this.handleMouseDown_.bind(this));

      this.signinButtonElement.addEventListener('click',
          this.activate.bind(this));

      this.actionBoxAreaElement.addEventListener('mousedown',
                                                 stopEventPropagation);
      this.actionBoxAreaElement.addEventListener('click',
          this.handleActionAreaButtonClick_.bind(this));
      this.actionBoxAreaElement.addEventListener('keydown',
          this.handleActionAreaButtonKeyDown_.bind(this));

      this.actionBoxMenuRemoveElement.addEventListener('click',
          this.handleRemoveCommandClick_.bind(this));
      this.actionBoxMenuRemoveElement.addEventListener('keydown',
          this.handleRemoveCommandKeyDown_.bind(this));
      this.actionBoxMenuRemoveElement.addEventListener('blur',
          this.handleRemoveCommandBlur_.bind(this));

      if (this.actionBoxRemoveUserWarningButtonElement) {
        this.actionBoxRemoveUserWarningButtonElement.addEventListener(
            'click',
            this.handleRemoveUserConfirmationClick_.bind(this));
      }

      this.customButton.addEventListener('click',
          this.handleCustomButtonClick_.bind(this));
    },

    /**
     * Initializes the pod after its properties set and added to a pod row.
     */
    initialize: function() {
      this.passwordElement.addEventListener('keydown',
          this.parentNode.handleKeyDown.bind(this.parentNode));
      this.passwordElement.addEventListener('keypress',
          this.handlePasswordKeyPress_.bind(this));

      this.imageElement.addEventListener('load',
          this.parentNode.handlePodImageLoad.bind(this.parentNode, this));
    },

    /**
     * Resets tab order for pod elements to its initial state.
     */
    resetTabOrder: function() {
      this.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.tabIndex = -1;
    },

    /**
     * Handles keypress event (i.e. any textual input) on password input.
     * @param {Event} e Keypress Event object.
     * @private
     */
    handlePasswordKeyPress_: function(e) {
      // When tabbing from the system tray a tab key press is received. Suppress
      // this so as not to type a tab character into the password field.
      if (e.keyCode == 9) {
        e.preventDefault();
        return;
      }
    },

    /**
     * Top edge margin number of pixels.
     * @type {?number}
     */
    set top(top) {
      this.style.top = cr.ui.toCssPx(top);
    },
    /**
     * Left edge margin number of pixels.
     * @type {?number}
     */
    set left(left) {
      this.style.left = cr.ui.toCssPx(left);
    },

    /**
     * Gets signed in indicator element.
     * @type {!HTMLDivElement}
     */
    get signedInIndicatorElement() {
      return this.querySelector('.signed-in-indicator');
    },

    /**
     * Gets image element.
     * @type {!HTMLImageElement}
     */
    get imageElement() {
      return this.querySelector('.user-image');
    },

    /**
     * Gets name element.
     * @type {!HTMLDivElement}
     */
    get nameElement() {
      return this.querySelector('.name');
    },

    /**
     * Gets password field.
     * @type {!HTMLInputElement}
     */
    get passwordElement() {
      return this.querySelector('.password');
    },

    /**
     * Gets Caps Lock hint image.
     * @type {!HTMLImageElement}
     */
    get capslockHintElement() {
      return this.querySelector('.capslock-hint');
    },

    /**
     * Gets user signin button.
     * @type {!HTMLInputElement}
     */
    get signinButtonElement() {
      return this.querySelector('.signin-button');
    },

    /**
     * Gets action box area.
     * @type {!HTMLInputElement}
     */
    get actionBoxAreaElement() {
      return this.querySelector('.action-box-area');
    },

    /**
     * Gets user type icon area.
     * @type {!HTMLInputElement}
     */
    get userTypeIconAreaElement() {
      return this.querySelector('.user-type-icon-area');
    },

    /**
     * Gets action box menu.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuElement() {
      return this.querySelector('.action-box-menu');
    },

    /**
     * Gets action box menu title.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuTitleElement() {
      return this.querySelector('.action-box-menu-title');
    },

    /**
     * Gets action box menu title, user name item.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuTitleNameElement() {
      return this.querySelector('.action-box-menu-title-name');
    },

    /**
     * Gets action box menu title, user email item.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuTitleEmailElement() {
      return this.querySelector('.action-box-menu-title-email');
    },

    /**
     * Gets action box menu, remove user command item.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuCommandElement() {
      return this.querySelector('.action-box-menu-remove-command');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuRemoveElement() {
      return this.querySelector('.action-box-menu-remove');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxRemoveUserWarningElement() {
      return this.querySelector('.action-box-remove-user-warning');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxRemoveUserWarningButtonElement() {
      return this.querySelector(
          '.remove-warning-button');
    },

    /**
     * Gets the locked user indicator box.
     * @type {!HTMLInputElement}
     */
    get lockedIndicatorElement() {
      return this.querySelector('.locked-indicator');
    },

    /**
     * Gets the custom button. This button is normally hidden, but can be
     * shown using the chrome.screenlockPrivate API.
     * @type {!HTMLInputElement}
     */
    get customButton() {
      return this.querySelector('.custom-button');
    },

    /**
     * Updates the user pod element.
     */
    update: function() {
      this.imageElement.src = 'chrome://userimage/' + this.user.username +
          '?id=' + UserPod.userImageSalt_[this.user.username];

      this.nameElement.textContent = this.user_.displayName;
      this.signedInIndicatorElement.hidden = !this.user_.signedIn;

      var needSignin = this.needSignin;
      this.passwordElement.hidden = needSignin;
      this.signinButtonElement.hidden = !needSignin;

      this.updateActionBoxArea();
    },

    updateActionBoxArea: function() {
      this.actionBoxAreaElement.hidden = this.user_.publicAccount;
      this.actionBoxMenuRemoveElement.hidden = !this.user_.canRemove;

      this.actionBoxAreaElement.setAttribute(
          'aria-label', loadTimeData.getStringF(
              'podMenuButtonAccessibleName', this.user_.emailAddress));
      this.actionBoxMenuRemoveElement.setAttribute(
          'aria-label', loadTimeData.getString(
               'podMenuRemoveItemAccessibleName'));
      this.actionBoxMenuTitleNameElement.textContent = this.user_.isOwner ?
          loadTimeData.getStringF('ownerUserPattern', this.user_.displayName) :
          this.user_.displayName;
      this.actionBoxMenuTitleEmailElement.textContent = this.user_.emailAddress;
      this.actionBoxMenuTitleEmailElement.hidden =
          this.user_.locallyManagedUser;

      this.actionBoxMenuCommandElement.textContent =
          loadTimeData.getString('removeUser');
      this.passwordElement.setAttribute('aria-label', loadTimeData.getStringF(
          'passwordFieldAccessibleName', this.user_.emailAddress));
      this.userTypeIconAreaElement.hidden = !this.user_.locallyManagedUser;
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

    /**
     * Whether signin is required for this user.
     */
    get needSignin() {
      // Signin is performed if the user has an invalid oauth token and is
      // not currently signed in (i.e. not the lock screen).
      return this.user.oauthTokenStatus != OAuthTokenStatus.VALID_OLD &&
          this.user.oauthTokenStatus != OAuthTokenStatus.VALID_NEW &&
          !this.user.signedIn;
    },

    /**
     * Gets main input element.
     * @type {(HTMLButtonElement|HTMLInputElement)}
     */
    get mainInput() {
      if (!this.signinButtonElement.hidden)
        return this.signinButtonElement;
      else
        return this.passwordElement;
    },

    /**
     * Whether action box button is in active state.
     * @type {boolean}
     */
    get isActionBoxMenuActive() {
      return this.actionBoxAreaElement.classList.contains('active');
    },
    set isActionBoxMenuActive(active) {
      if (active == this.isActionBoxMenuActive)
        return;

      if (active) {
        this.actionBoxMenuRemoveElement.hidden = !this.user_.canRemove;
        if (this.actionBoxRemoveUserWarningElement)
          this.actionBoxRemoveUserWarningElement.hidden = true;

        // Clear focus first if another pod is focused.
        if (!this.parentNode.isFocused(this)) {
          this.parentNode.focusPod(undefined, true);
          this.actionBoxAreaElement.focus();
        }
        this.actionBoxAreaElement.classList.add('active');
      } else {
        this.actionBoxAreaElement.classList.remove('active');
      }
    },

    /**
     * Whether action box button is in hovered state.
     * @type {boolean}
     */
    get isActionBoxMenuHovered() {
      return this.actionBoxAreaElement.classList.contains('hovered');
    },
    set isActionBoxMenuHovered(hovered) {
      if (hovered == this.isActionBoxMenuHovered)
        return;

      if (hovered) {
        this.actionBoxAreaElement.classList.add('hovered');
        this.classList.add('hovered');
      } else {
        this.actionBoxAreaElement.classList.remove('hovered');
        this.classList.remove('hovered');
      }
    },

    /**
     * Updates the image element of the user.
     */
    updateUserImage: function() {
      UserPod.userImageSalt_[this.user.username] = new Date().getTime();
      this.update();
    },

    /**
     * Focuses on input element.
     */
    focusInput: function() {
      var needSignin = this.needSignin;
      this.signinButtonElement.hidden = !needSignin;
      this.passwordElement.hidden = needSignin;

      // Move tabIndex from the whole pod to the main input.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /**
     * Activates the pod.
     * @return {boolean} True if activated successfully.
     */
    activate: function() {
      if (!this.signinButtonElement.hidden) {
        this.showSigninUI();
      } else if (!this.passwordElement.value) {
        return false;
      } else {
        Oobe.disableSigninUI();
        chrome.send('authenticateUser',
                    [this.user.username, this.passwordElement.value]);
      }

      return true;
    },

    showSupervisedUserSigninWarning: function() {
      // Locally managed user token has been invalidated.
      // Make sure that pod is focused i.e. "Sign in" button is seen.
      this.parentNode.focusPod(this);

      var error = document.createElement('div');
      var messageDiv = document.createElement('div');
      messageDiv.className = 'error-message-bubble';
      messageDiv.textContent =
          loadTimeData.getString('supervisedUserExpiredTokenWarning');
      error.appendChild(messageDiv);

      $('bubble').showContentForElement(
          this.signinButtonElement,
          cr.ui.Bubble.Attachment.TOP,
          error,
          this.signinButtonElement.offsetWidth / 2,
          4);
    },

    /**
     * Shows signin UI for this user.
     */
    showSigninUI: function() {
      if (this.user.locallyManagedUser) {
        this.showSupervisedUserSigninWarning();
      } else {
        this.parentNode.showSigninUI(this.user.emailAddress);
      }
    },

    /**
     * Resets the input field and updates the tab order of pod controls.
     * @param {boolean} takeFocus If true, input field takes focus.
     */
    reset: function(takeFocus) {
      this.passwordElement.value = '';
      if (takeFocus)
        this.focusInput();  // This will set a custom tab order.
      else
        this.resetTabOrder();
    },

    /**
     * Handles a click event on action area button.
     * @param {Event} e Click event.
     */
    handleActionAreaButtonClick_: function(e) {
      if (this.parentNode.disabled)
        return;
      this.isActionBoxMenuActive = !this.isActionBoxMenuActive;
    },

    /**
     * Handles a keydown event on action area button.
     * @param {Event} e KeyDown event.
     */
    handleActionAreaButtonKeyDown_: function(e) {
      if (this.disabled)
        return;
      switch (e.keyIdentifier) {
        case 'Enter':
        case 'U+0020':  // Space
          if (this.parentNode.focusedPod_ && !this.isActionBoxMenuActive)
            this.isActionBoxMenuActive = true;
          e.stopPropagation();
          break;
        case 'Up':
        case 'Down':
          if (this.isActionBoxMenuActive) {
            this.actionBoxMenuRemoveElement.tabIndex =
                UserPodTabOrder.PAD_MENU_ITEM;
            this.actionBoxMenuRemoveElement.focus();
          }
          e.stopPropagation();
          break;
        case 'U+001B':  // Esc
          this.isActionBoxMenuActive = false;
          e.stopPropagation();
          break;
        case 'U+0009':  // Tab
          this.parentNode.focusPod();
        default:
          this.isActionBoxMenuActive = false;
          break;
      }
    },

    /**
     * Handles a click event on remove user command.
     * @param {Event} e Click event.
     */
    handleRemoveCommandClick_: function(e) {
      if (this.user.locallyManagedUser || this.user.isDesktopUser) {
        this.showRemoveWarning_();
        return;
      }
      if (this.isActionBoxMenuActive)
        chrome.send('removeUser', [this.user.username]);
    },

    /**
     * Shows remove warning for managed users.
     */
    showRemoveWarning_: function() {
      this.actionBoxMenuRemoveElement.hidden = true;
      this.actionBoxRemoveUserWarningElement.hidden = false;
    },

    /**
     * Handles a click event on remove user confirmation button.
     * @param {Event} e Click event.
     */
    handleRemoveUserConfirmationClick_: function(e) {
      if (this.isActionBoxMenuActive)
        chrome.send('removeUser', [this.user.username]);
    },

    /**
     * Handles a keydown event on remove command.
     * @param {Event} e KeyDown event.
     */
    handleRemoveCommandKeyDown_: function(e) {
      if (this.disabled)
        return;
      switch (e.keyIdentifier) {
        case 'Enter':
          chrome.send('removeUser', [this.user.username]);
          e.stopPropagation();
          break;
        case 'Up':
        case 'Down':
          e.stopPropagation();
          break;
        case 'U+001B':  // Esc
          this.actionBoxAreaElement.focus();
          this.isActionBoxMenuActive = false;
          e.stopPropagation();
          break;
        default:
          this.actionBoxAreaElement.focus();
          this.isActionBoxMenuActive = false;
          break;
      }
    },

    /**
     * Handles a blur event on remove command.
     * @param {Event} e Blur event.
     */
    handleRemoveCommandBlur_: function(e) {
      if (this.disabled)
        return;
      this.actionBoxMenuRemoveElement.tabIndex = -1;
    },

    /**
     * Handles mousedown event on a user pod.
     * @param {Event} e Mousedown event.
     */
    handleMouseDown_: function(e) {
      if (this.parentNode.disabled)
        return;

      if (!this.signinButtonElement.hidden && !this.isActionBoxMenuActive) {
        this.showSigninUI();
        // Prevent default so that we don't trigger 'focus' event.
        e.preventDefault();
      }
    },

    /**
     * Called when the custom button is clicked.
     */
    handleCustomButtonClick_: function() {
      chrome.send('customButtonClicked', [this.user.username]);
    }
  };

  /**
   * Creates a public account user pod.
   * @constructor
   * @extends {UserPod}
   */
  var PublicAccountUserPod = cr.ui.define(function() {
    var node = UserPod();

    var extras = $('public-account-user-pod-extras-template').children;
    for (var i = 0; i < extras.length; ++i) {
      var el = extras[i].cloneNode(true);
      node.appendChild(el);
    }

    return node;
  });

  PublicAccountUserPod.prototype = {
    __proto__: UserPod.prototype,

    /**
     * "Enter" button in expanded side pane.
     * @type {!HTMLButtonElement}
     */
    get enterButtonElement() {
      return this.querySelector('.enter-button');
    },

    /**
     * Boolean flag of whether the pod is showing the side pane. The flag
     * controls whether 'expanded' class is added to the pod's class list and
     * resets tab order because main input element changes when the 'expanded'
     * state changes.
     * @type {boolean}
     */
    get expanded() {
      return this.classList.contains('expanded');
    },
    set expanded(expanded) {
      if (this.expanded == expanded)
        return;

      this.resetTabOrder();
      this.classList.toggle('expanded', expanded);

      var self = this;
      this.classList.add('animating');
      this.addEventListener('webkitTransitionEnd', function f(e) {
        self.removeEventListener('webkitTransitionEnd', f);
        self.classList.remove('animating');

        // Accessibility focus indicator does not move with the focused
        // element. Sends a 'focus' event on the currently focused element
        // so that accessibility focus indicator updates its location.
        if (document.activeElement)
          document.activeElement.dispatchEvent(new Event('focus'));
      });
    },

    /** @override */
    get needSignin() {
      return false;
    },

    /** @override */
    get mainInput() {
      if (this.expanded)
        return this.enterButtonElement;
      else
        return this.nameElement;
    },

    /** @override */
    decorate: function() {
      UserPod.prototype.decorate.call(this);

      this.classList.remove('need-password');
      this.classList.add('public-account');

      this.nameElement.addEventListener('keydown', (function(e) {
        if (e.keyIdentifier == 'Enter') {
          this.parentNode.activatedPod = this;
          // Stop this keydown event from bubbling up to PodRow handler.
          e.stopPropagation();
          // Prevent default so that we don't trigger a 'click' event on the
          // newly focused "Enter" button.
          e.preventDefault();
        }
      }).bind(this));

      var learnMore = this.querySelector('.learn-more');
      learnMore.addEventListener('mousedown', stopEventPropagation);
      learnMore.addEventListener('click', this.handleLearnMoreEvent);
      learnMore.addEventListener('keydown', this.handleLearnMoreEvent);

      learnMore = this.querySelector('.side-pane-learn-more');
      learnMore.addEventListener('click', this.handleLearnMoreEvent);
      learnMore.addEventListener('keydown', this.handleLearnMoreEvent);

      this.enterButtonElement.addEventListener('click', (function(e) {
        this.enterButtonElement.disabled = true;
        chrome.send('launchPublicAccount', [this.user.username]);
      }).bind(this));
    },

    /**
     * Updates the user pod element.
     */
    update: function() {
      UserPod.prototype.update.call(this);
      this.querySelector('.side-pane-name').textContent =
          this.user_.displayName;
      this.querySelector('.info').textContent =
          loadTimeData.getStringF('publicAccountInfoFormat',
                                  this.user_.enterpriseDomain);
    },

    /** @override */
    focusInput: function() {
      // Move tabIndex from the whole pod to the main input.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /** @override */
    reset: function(takeFocus) {
      if (!takeFocus)
        this.expanded = false;
      this.enterButtonElement.disabled = false;
      UserPod.prototype.reset.call(this, takeFocus);
    },

    /** @override */
    activate: function() {
      this.expanded = true;
      this.focusInput();
      return true;
    },

    /** @override */
    handleMouseDown_: function(e) {
      if (this.parentNode.disabled)
        return;

      this.parentNode.focusPod(this);
      this.parentNode.activatedPod = this;
      // Prevent default so that we don't trigger 'focus' event.
      e.preventDefault();
    },

    /**
     * Handle mouse and keyboard events for the learn more button.
     * Triggering the button causes information about public sessions to be
     * shown.
     * @param {Event} event Mouse or keyboard event.
     */
    handleLearnMoreEvent: function(event) {
      switch (event.type) {
        // Show informaton on left click. Let any other clicks propagate.
        case 'click':
          if (event.button != 0)
            return;
          break;
        // Show informaton when <Return> or <Space> is pressed. Let any other
        // key presses propagate.
        case 'keydown':
          switch (event.keyCode) {
            case 13:  // Return.
            case 32:  // Space.
              break;
            default:
              return;
          }
          break;
      }
      chrome.send('launchHelpApp', [HELP_TOPIC_PUBLIC_SESSION]);
      stopEventPropagation(event);
    },
  };

  /**
   * Creates a user pod to be used only in desktop chrome.
   * @constructor
   * @extends {UserPod}
   */
  var DesktopUserPod = cr.ui.define(function() {
    // Don't just instantiate a UserPod(), as this will call decorate() on the
    // parent object, and add duplicate event listeners.
    var node = $('user-pod-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  DesktopUserPod.prototype = {
    __proto__: UserPod.prototype,

    /** @override */
    get mainInput() {
      if (!this.passwordElement.hidden)
        return this.passwordElement;
      else
        return this.nameElement;
    },

    /** @override */
    decorate: function() {
      UserPod.prototype.decorate.call(this);
    },

    /** @override */
    update: function() {
      // TODO(noms): Use the actual profile avatar for local profiles once the
      // new, non-pixellated avatars are available.
      this.imageElement.src = this.user.emailAddress == '' ?
          'chrome://theme/IDR_USER_MANAGER_DEFAULT_AVATAR' :
          this.user.userImage;
      this.nameElement.textContent = this.user.displayName;

      var isLockedUser = this.user.needsSignin;
      this.signinButtonElement.hidden = true;
      this.lockedIndicatorElement.hidden = !isLockedUser;
      this.passwordElement.hidden = !isLockedUser;
      this.nameElement.hidden = isLockedUser;

      UserPod.prototype.updateActionBoxArea.call(this);
    },

    /** @override */
    focusInput: function() {
      // For focused pods, display the name unless the pod is locked.
      var isLockedUser = this.user.needsSignin;
      this.signinButtonElement.hidden = true;
      this.lockedIndicatorElement.hidden = !isLockedUser;
      this.passwordElement.hidden = !isLockedUser;
      this.nameElement.hidden = isLockedUser;

      // Move tabIndex from the whole pod to the main input.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /** @override */
    reset: function(takeFocus) {
      // Always display the user's name for unfocused pods.
      if (!takeFocus)
        this.nameElement.hidden = false;
      UserPod.prototype.reset.call(this, takeFocus);
    },

    /** @override */
    activate: function() {
      if (this.passwordElement.hidden) {
        Oobe.launchUser(this.user.emailAddress, this.user.displayName);
      } else if (!this.passwordElement.value) {
        return false;
      } else {
        chrome.send('authenticatedLaunchUser',
                    [this.user.emailAddress,
                     this.user.displayName,
                     this.passwordElement.value]);
      }
      this.passwordElement.value = '';
      return true;
    },

    /** @override */
    handleMouseDown_: function(e) {
      if (this.parentNode.disabled)
        return;

      Oobe.clearErrors();
      this.parentNode.lastFocusedPod_ = this;

      // If this is an unlocked pod, then open a browser window. Otherwise
      // just activate the pod and show the password field.
      if (!this.user.needsSignin && !this.isActionBoxMenuActive)
        this.activate();
    },

    /** @override */
    handleRemoveUserConfirmationClick_: function(e) {
      chrome.send('removeUser', [this.user.profilePath]);
    },
  };

  /**
   * Creates a new pod row element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var PodRow = cr.ui.define('podrow');

  PodRow.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Whether this user pod row is shown for the first time.
    firstShown_: true,

    // True if inside focusPod().
    insideFocusPod_: false,

    // Focused pod.
    focusedPod_: undefined,

    // Activated pod, i.e. the pod of current login attempt.
    activatedPod_: undefined,

    // Pod that was most recently focused, if any.
    lastFocusedPod_: undefined,

    // Note: created only in decorate() !
    wallpaperLoader_: undefined,

    // Pods whose initial images haven't been loaded yet.
    podsWithPendingImages_: [],

    /** @override */
    decorate: function() {
      // Event listeners that are installed for the time period during which
      // the element is visible.
      this.listeners_ = {
        focus: [this.handleFocus_.bind(this), true /* useCapture */],
        click: [this.handleClick_.bind(this), true],
        mousemove: [this.handleMouseMove_.bind(this), false],
        keydown: [this.handleKeyDown.bind(this), false]
      };
      this.wallpaperLoader_ = new login.WallpaperLoader();
    },

    /**
     * Returns all the pods in this pod row.
     * @type {NodeList}
     */
    get pods() {
      return Array.prototype.slice.call(this.children);
    },

    /**
     * Return true if user pod row has only single user pod in it.
     * @type {boolean}
     */
    get isSinglePod() {
      return this.children.length == 1;
    },

    /**
     * Returns pod with the given username (null if there is no such pod).
     * @param {string} username Username to be matched.
     * @return {Object} Pod with the given username. null if pod hasn't been
     *                  found.
     */
    getPodWithUsername_: function(username) {
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (pod.user.username == username)
          return pod;
      }
      return null;
    },

    /**
     * True if the the pod row is disabled (handles no user interaction).
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
    },

    /**
     * Creates a user pod from given email.
     * @param {string} email User's email.
     */
    createUserPod: function(user) {
      var userPod;
      if (user.isDesktopUser)
        userPod = new DesktopUserPod({user: user});
      else if (user.publicAccount)
        userPod = new PublicAccountUserPod({user: user});
      else
        userPod = new UserPod({user: user});

      userPod.hidden = false;
      return userPod;
    },

    /**
     * Add an existing user pod to this pod row.
     * @param {!Object} user User info dictionary.
     * @param {boolean} animated Whether to use init animation.
     */
    addUserPod: function(user, animated) {
      var userPod = this.createUserPod(user);
      if (animated) {
        userPod.classList.add('init');
        userPod.nameElement.classList.add('init');
      }

      this.appendChild(userPod);
      userPod.initialize();
    },

    /**
     * Removes user pod from pod row.
     * @param {string} email User's email.
     */
    removeUserPod: function(username) {
      var podToRemove = this.getPodWithUsername_(username);
      if (podToRemove == null) {
        console.warn('Attempt to remove not existing pod for ' + username +
            '.');
        return;
      }
      this.removeChild(podToRemove);
      this.placePods_();
    },

    /**
     * Returns index of given pod or -1 if not found.
     * @param {UserPod} pod Pod to look up.
     * @private
     */
    indexOf_: function(pod) {
      for (var i = 0; i < this.pods.length; ++i) {
        if (pod == this.pods[i])
          return i;
      }
      return -1;
    },

    /**
     * Start first time show animation.
     */
    startInitAnimation: function() {
      // Schedule init animation.
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        window.setTimeout(removeClass, 500 + i * 70, pod, 'init');
        window.setTimeout(removeClass, 700 + i * 70, pod.nameElement, 'init');
      }
    },

    /**
     * Start login success animation.
     */
    startAuthenticatedAnimation: function() {
      var activated = this.indexOf_(this.activatedPod_);
      if (activated == -1)
        return;

      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (i < activated)
          pod.classList.add('left');
        else if (i > activated)
          pod.classList.add('right');
        else
          pod.classList.add('zoom');
      }
    },

    /**
     * Populates pod row with given existing users and start init animation.
     * @param {array} users Array of existing user emails.
     * @param {boolean} animated Whether to use init animation.
     */
    loadPods: function(users, animated) {
      // Clear existing pods.
      this.innerHTML = '';
      this.focusedPod_ = undefined;
      this.activatedPod_ = undefined;
      this.lastFocusedPod_ = undefined;

      // Switch off animation
      Oobe.getInstance().toggleClass('flying-pods', false);

      // Populate the pod row.
      for (var i = 0; i < users.length; ++i) {
        this.addUserPod(users[i], animated);
      }
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        this.podsWithPendingImages_.push(pod);
      }
      // Make sure we eventually show the pod row, even if some image is stuck.
      setTimeout(function() {
        $('pod-row').classList.remove('images-loading');
      }, POD_ROW_IMAGES_LOAD_TIMEOUT_MS);

      this.placePods_();

      // Without timeout changes in pods positions will be animated even though
      // it happened when 'flying-pods' class was disabled.
      setTimeout(function() {
        Oobe.getInstance().toggleClass('flying-pods', true);
      }, 0);

      this.focusPod(this.preselectedPod);
    },

    /**
     * Shows a button on a user pod with an icon. Clicking on this button
     * triggers an event used by the chrome.screenlockPrivate API.
     * @param {string} username Username of pod to add button
     * @param {string} iconURL URL of the button icon
     */
    showUserPodButton: function(username, iconURL) {
      var pod = this.getPodWithUsername_(username);
      if (pod == null) {
        console.error('Unable to show user pod button for ' + username +
                      ': user pod not found.');
        return;
      }

      pod.customButton.hidden = false;
      var icon =
          pod.customButton.querySelector('.custom-button-icon');
      icon.src = iconURL;
    },

    /**
     * Called when window was resized.
     */
    onWindowResize: function() {
      var layout = this.calculateLayout_();
      if (layout.columns != this.columns || layout.rows != this.rows)
        this.placePods_();
    },

    /**
     * Returns width of podrow having |columns| number of columns.
     * @private
     */
    columnsToWidth_: function(columns) {
      var margin = MARGIN_BY_COLUMNS[columns];
      return 2 * POD_ROW_PADDING + columns * POD_WIDTH + (columns - 1) * margin;
    },

    /**
     * Returns height of podrow having |rows| number of rows.
     * @private
     */
    rowsToHeight_: function(rows) {
      return 2 * POD_ROW_PADDING + rows * POD_HEIGHT;
    },

    /**
     * Calculates number of columns and rows that podrow should have in order to
     * hold as much its pods as possible for current screen size. Also it tries
     * to choose layout that looks good.
     * @return {{columns: number, rows: number}}
     */
    calculateLayout_: function() {
      var preferredColumns = this.pods.length < COLUMNS.length ?
          COLUMNS[this.pods.length] : COLUMNS[COLUMNS.length - 1];
      var maxWidth = Oobe.getInstance().clientAreaSize.width;
      var columns = preferredColumns;
      while (maxWidth < this.columnsToWidth_(columns) && columns > 1)
        --columns;
      var rows = Math.floor((this.pods.length - 1) / columns) + 1;
      var maxHeigth = Oobe.getInstance().clientAreaSize.height;
      while (maxHeigth < this.rowsToHeight_(rows) && rows > 1)
        --rows;
      // One more iteration if it's not enough cells to place all pods.
      while (maxWidth >= this.columnsToWidth_(columns + 1) &&
             columns * rows < this.pods.length &&
             columns < MAX_NUMBER_OF_COLUMNS) {
         ++columns;
      }
      return {columns: columns, rows: rows};
    },

    /**
     * Places pods onto their positions onto pod grid.
     * @private
     */
    placePods_: function() {
      var layout = this.calculateLayout_();
      var columns = this.columns = layout.columns;
      var rows = this.rows = layout.rows;
      var maxPodsNumber = columns * rows;
      var margin = MARGIN_BY_COLUMNS[columns];
      this.parentNode.setPreferredSize(
          this.columnsToWidth_(columns), this.rowsToHeight_(rows));
      this.pods.forEach(function(pod, index) {
        if (pod.offsetHeight != POD_HEIGHT)
          console.error('Pod offsetHeight and POD_HEIGHT are not equal.');
        if (pod.offsetWidth != POD_WIDTH)
          console.error('Pod offsetWidht and POD_WIDTH are not equal.');
        if (index >= maxPodsNumber) {
           pod.hidden = true;
           return;
        }
        pod.hidden = false;
        var column = index % columns;
        var row = Math.floor(index / columns);
        pod.left = POD_ROW_PADDING + column * (POD_WIDTH + margin);
        pod.top = POD_ROW_PADDING + row * POD_HEIGHT;
      });
      Oobe.getInstance().updateScreenSize(this.parentNode);
    },

    /**
     * Number of columns.
     * @type {?number}
     */
    set columns(columns) {
      // Cannot use 'columns' here.
      this.setAttribute('ncolumns', columns);
    },
    get columns() {
      return this.getAttribute('ncolumns');
    },

    /**
     * Number of rows.
     * @type {?number}
     */
    set rows(rows) {
      // Cannot use 'rows' here.
      this.setAttribute('nrows', rows);
    },
    get rows() {
      return this.getAttribute('nrows');
    },

    /**
     * Whether the pod is currently focused.
     * @param {UserPod} pod Pod to check for focus.
     * @return {boolean} Pod focus status.
     */
    isFocused: function(pod) {
      return this.focusedPod_ == pod;
    },

    /**
     * Focuses a given user pod or clear focus when given null.
     * @param {UserPod=} podToFocus User pod to focus (undefined clears focus).
     * @param {boolean=} opt_force If true, forces focus update even when
     *                             podToFocus is already focused.
     */
    focusPod: function(podToFocus, opt_force) {
      if (this.isFocused(podToFocus) && !opt_force) {
        this.keyboardActivated_ = false;
        return;
      }

      // Make sure there's only one focusPod operation happening at a time.
      if (this.insideFocusPod_) {
        this.keyboardActivated_ = false;
        return;
      }
      this.insideFocusPod_ = true;

      this.wallpaperLoader_.reset();
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (!this.isSinglePod) {
          pod.isActionBoxMenuActive = false;
        }
        if (pod != podToFocus) {
          pod.isActionBoxMenuHovered = false;
          pod.classList.remove('focused');
          pod.classList.remove('faded');
          pod.reset(false);
        }
      }

      // Clear any error messages for previous pod.
      if (!this.isFocused(podToFocus))
        Oobe.clearErrors();

      var hadFocus = !!this.focusedPod_;
      this.focusedPod_ = podToFocus;
      if (podToFocus) {
        podToFocus.classList.remove('faded');
        podToFocus.classList.add('focused');
        podToFocus.reset(true);  // Reset and give focus.
        chrome.send('focusPod', [podToFocus.user.username]);

        this.wallpaperLoader_.scheduleLoad(podToFocus.user.username, opt_force);
        this.firstShown_ = false;
        this.lastFocusedPod_ = podToFocus;
      }
      this.insideFocusPod_ = false;
      this.keyboardActivated_ = false;
    },

    /**
     * Focuses a given user pod by index or clear focus when given null.
     * @param {int=} podToFocus index of User pod to focus.
     * @param {boolean=} opt_force If true, forces focus update even when
     *                             podToFocus is already focused.
     */
    focusPodByIndex: function(podToFocus, opt_force) {
      if (podToFocus < this.pods.length)
        this.focusPod(this.pods[podToFocus], opt_force);
    },

    /**
     * Resets wallpaper to the last active user's wallpaper, if any.
     */
    loadLastWallpaper: function() {
      if (this.lastFocusedPod_)
        this.wallpaperLoader_.scheduleLoad(this.lastFocusedPod_.user.username,
                                           true /* force */);
    },

    /**
     * Handles 'onWallpaperLoaded' event. Recalculates statistics and
     * [re]schedules next wallpaper load.
     */
    onWallpaperLoaded: function(username) {
      this.wallpaperLoader_.onWallpaperLoaded(username);
    },

    /**
     * Returns the currently activated pod.
     * @type {UserPod}
     */
    get activatedPod() {
      return this.activatedPod_;
    },
    set activatedPod(pod) {
      if (pod && pod.activate())
        this.activatedPod_ = pod;
    },

    /**
     * The pod of the signed-in user, if any; null otherwise.
     * @type {?UserPod}
     */
    get lockedPod() {
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (pod.user.signedIn)
          return pod;
      }
      return null;
    },

    /**
     * The pod that is preselected on user pod row show.
     * @type {?UserPod}
     */
    get preselectedPod() {
      var lockedPod = this.lockedPod;
      var preselectedPod = PRESELECT_FIRST_POD ?
          lockedPod || this.pods[0] : lockedPod;
      return preselectedPod;
    },

    /**
     * Resets input UI.
     * @param {boolean} takeFocus True to take focus.
     */
    reset: function(takeFocus) {
      this.disabled = false;
      if (this.activatedPod_)
        this.activatedPod_.reset(takeFocus);
    },

    /**
     * Restores input focus to current selected pod, if there is any.
     */
    refocusCurrentPod: function() {
      if (this.focusedPod_) {
        this.focusedPod_.focusInput();
      }
    },

    /**
     * Clears focused pod password field.
     */
    clearFocusedPod: function() {
      if (!this.disabled && this.focusedPod_)
        this.focusedPod_.reset(true);
    },

    /**
     * Shows signin UI.
     * @param {string} email Email for signin UI.
     */
    showSigninUI: function(email) {
      // Clear any error messages that might still be around.
      Oobe.clearErrors();
      this.disabled = true;
      this.lastFocusedPod_ = this.getPodWithUsername_(email);
      Oobe.showSigninUI(email);
    },

    /**
     * Updates current image of a user.
     * @param {string} username User for which to update the image.
     */
    updateUserImage: function(username) {
      var pod = this.getPodWithUsername_(username);
      if (pod)
        pod.updateUserImage();
    },

    /**
     * Resets OAuth token status (invalidates it).
     * @param {string} username User for which to reset the status.
     */
    resetUserOAuthTokenStatus: function(username) {
      var pod = this.getPodWithUsername_(username);
      if (pod) {
        pod.user.oauthTokenStatus = OAuthTokenStatus.INVALID_OLD;
        pod.update();
      } else {
        console.log('Failed to update Gaia state for: ' + username);
      }
    },

    /**
     * Handler of click event.
     * @param {Event} e Click Event object.
     * @private
     */
    handleClick_: function(e) {
      if (this.disabled)
        return;

      // Clear all menus if the click is outside pod menu and its
      // button area.
      if (!findAncestorByClass(e.target, 'action-box-menu') &&
          !findAncestorByClass(e.target, 'action-box-area')) {
        for (var i = 0, pod; pod = this.pods[i]; ++i)
          pod.isActionBoxMenuActive = false;
      }

      // Clears focus if not clicked on a pod and if there's more than one pod.
      var pod = findAncestorByClass(e.target, 'pod');
      if ((!pod || pod.parentNode != this) && !this.isSinglePod) {
        this.focusPod();
      }

      if (pod)
        pod.isActionBoxMenuHovered = true;

      // Return focus back to single pod.
      if (this.isSinglePod) {
        this.focusPod(this.focusedPod_, true /* force */);
        if (!pod)
          this.focusedPod_.isActionBoxMenuHovered = false;
      }

      // Also stop event propagation.
      if (pod && e.target == pod.imageElement)
        e.stopPropagation();
    },

    /**
     * Handler of mouse move event.
     * @param {Event} e Click Event object.
     * @private
     */
    handleMouseMove_: function(e) {
      if (this.disabled)
        return;
      if (e.webkitMovementX == 0 && e.webkitMovementY == 0)
        return;

      // Defocus (thus hide) action box, if it is focused on a user pod
      // and the pointer is not hovering over it.
      var pod = findAncestorByClass(e.target, 'pod');
      if (document.activeElement &&
          document.activeElement.parentNode != pod &&
          document.activeElement.classList.contains('action-box-area')) {
        document.activeElement.parentNode.focus();
      }

      if (pod)
        pod.isActionBoxMenuHovered = true;

      // Hide action boxes on other user pods.
      for (var i = 0, p; p = this.pods[i]; ++i)
        if (p != pod && !p.isActionBoxMenuActive)
          p.isActionBoxMenuHovered = false;
    },

    /**
     * Handles focus event.
     * @param {Event} e Focus Event object.
     * @private
     */
    handleFocus_: function(e) {
      if (this.disabled)
        return;
      if (e.target.parentNode == this) {
        // Focus on a pod
        if (e.target.classList.contains('focused'))
          e.target.focusInput();
        else
          this.focusPod(e.target);
        return;
      }

      var pod = findAncestorByClass(e.target, 'pod');
      if (pod && pod.parentNode == this) {
        // Focus on a control of a pod but not on the action area button.
        if (!pod.classList.contains('focused') &&
            !e.target.classList.contains('action-box-button')) {
          this.focusPod(pod);
          e.target.focus();
        }
        return;
      }

      // Clears pod focus when we reach here. It means new focus is neither
      // on a pod nor on a button/input for a pod.
      // Do not "defocus" user pod when it is a single pod.
      // That means that 'focused' class will not be removed and
      // input field/button will always be visible.
      if (!this.isSinglePod)
        this.focusPod();
    },

    /**
     * Handler of keydown event.
     * @param {Event} e KeyDown Event object.
     */
    handleKeyDown: function(e) {
      if (this.disabled)
        return;
      var editing = e.target.tagName == 'INPUT' && e.target.value;
      switch (e.keyIdentifier) {
        case 'Left':
          if (!editing) {
            this.keyboardActivated_ = true;
            if (this.focusedPod_ && this.focusedPod_.previousElementSibling)
              this.focusPod(this.focusedPod_.previousElementSibling);
            else
              this.focusPod(this.lastElementChild);

            e.stopPropagation();
          }
          break;
        case 'Right':
          if (!editing) {
            this.keyboardActivated_ = true;
            if (this.focusedPod_ && this.focusedPod_.nextElementSibling)
              this.focusPod(this.focusedPod_.nextElementSibling);
            else
              this.focusPod(this.firstElementChild);

            e.stopPropagation();
          }
          break;
        case 'Enter':
          if (this.focusedPod_) {
            this.activatedPod = this.focusedPod_;
            e.stopPropagation();
          }
          break;
        case 'U+001B':  // Esc
          if (!this.isSinglePod)
            this.focusPod();
          break;
      }
    },

    /**
     * Called right after the pod row is shown.
     */
    handleAfterShow: function() {
      // Without timeout changes in pods positions will be animated even though
      // it happened when 'flying-pods' class was disabled.
      setTimeout(function() {
        Oobe.getInstance().toggleClass('flying-pods', true);
      }, 0);
      // Force input focus for user pod on show and once transition ends.
      if (this.focusedPod_) {
        var focusedPod = this.focusedPod_;
        var screen = this.parentNode;
        var self = this;
        focusedPod.addEventListener('webkitTransitionEnd', function f(e) {
          if (e.target == focusedPod) {
            focusedPod.removeEventListener('webkitTransitionEnd', f);
            focusedPod.reset(true);
            // Notify screen that it is ready.
            screen.onShow();
            self.wallpaperLoader_.scheduleLoad(focusedPod.user.username,
                                               true /* force */);
          }
        });
        // Guard timer for 1 second -- it would conver all possible animations.
        ensureTransitionEndEvent(focusedPod, 1000);
      }
    },

    /**
     * Called right before the pod row is shown.
     */
    handleBeforeShow: function() {
      Oobe.getInstance().toggleClass('flying-pods', false);
      for (var event in this.listeners_) {
        this.ownerDocument.addEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
      $('login-header-bar').buttonsTabIndex = UserPodTabOrder.HEADER_BAR;
    },

    /**
     * Called when the element is hidden.
     */
    handleHide: function() {
      for (var event in this.listeners_) {
        this.ownerDocument.removeEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
      $('login-header-bar').buttonsTabIndex = 0;
    },

    /**
     * Called when a pod's user image finishes loading.
     */
    handlePodImageLoad: function(pod) {
      var index = this.podsWithPendingImages_.indexOf(pod);
      if (index == -1) {
        return;
      }

      this.podsWithPendingImages_.splice(index, 1);
      if (this.podsWithPendingImages_.length == 0) {
        this.classList.remove('images-loading');
      }
    }
  };

  return {
    PodRow: PodRow
  };
});
