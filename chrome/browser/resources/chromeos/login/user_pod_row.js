// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview User pod row implementation.
 */

cr.define('login', function() {
  /**
   * Pod width. 170px Pod + 10px padding + 10px margin on both sides.
   * @type {number}
   * @const
   */
  var POD_WIDTH = 170 + 2 * (10 + 10);

  /**
   * Number of displayed columns depending on user pod count.
   * @type {Array.<number>}
   * @const
   */
  var COLUMNS = [0, 1, 2, 3, 4, 5, 4, 4, 4, 5, 5, 6, 6, 5, 5, 6, 6, 6, 6];

  /**
   * Whether to preselect the first pod automatically on login screen.
   * @type {boolean}
   * @const
   */
  var PRESELECT_FIRST_POD = true;

  /**
   * Wallpaper load delay in milliseconds.
   * @type {number}
   * @const
   */
  var WALLPAPER_LOAD_DELAY_MS = 500;

  /**
   * Wallpaper load delay in milliseconds. TODO(nkostylev): Tune this constant.
   * @type {number}
   * @const
   */
  var WALLPAPER_BOOT_LOAD_DELAY_MS = 100;

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
  var HELP_TOPIC_PUBLIC_SESSION = 3017014;

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
     * Gets action box menu.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuElement() {
      return this.querySelector('.action-box-menu');
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
     * Updates the user pod element.
     */
    update: function() {
      this.imageElement.src = 'chrome://userimage/' + this.user.username +
          '?id=' + UserPod.userImageSalt_[this.user.username];

      this.nameElement.textContent = this.user_.displayName;
      this.actionBoxMenuRemoveElement.hidden = !this.user_.canRemove;
      this.signedInIndicatorElement.hidden = !this.user_.signedIn;

      var needSignin = this.needGaiaSignin;
      this.passwordElement.hidden = needSignin;
      this.actionBoxAreaElement.setAttribute(
          'aria-label', loadTimeData.getStringF(
              'podMenuButtonAccessibleName', this.user_.emailAddress));
      this.actionBoxMenuRemoveElement.setAttribute(
          'aria-label', loadTimeData.getString(
               'podMenuRemoveItemAccessibleName'));
      this.actionBoxMenuTitleNameElement.textContent = !this.user_.canRemove ?
          loadTimeData.getStringF('ownerUserPattern', this.user_.displayName) :
          this.user_.displayName;
      this.actionBoxMenuTitleEmailElement.textContent = this.user_.emailAddress;
      this.actionBoxMenuCommandElement.textContent =
          loadTimeData.getString('removeUser');
      this.passwordElement.setAttribute('aria-label', loadTimeData.getStringF(
          'passwordFieldAccessibleName', this.user_.emailAddress));
      this.signinButtonElement.hidden = !needSignin;
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
     * Whether Gaia signin is required for this user.
     */
    get needGaiaSignin() {
      // Gaia signin is performed if the user has an invalid oauth token and is
      // not currently signed in (i.e. not the lock screen).
      // Locally managed users never require GAIA signin.
      return this.user.oauthTokenStatus != OAuthTokenStatus.VALID_OLD &&
          this.user.oauthTokenStatus != OAuthTokenStatus.VALID_NEW &&
          !this.user.signedIn && !this.user.locallyManagedUser;
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
    get activeActionBoxMenu() {
      return this.actionBoxAreaElement.classList.contains('active');
    },
    set activeActionBoxMenu(active) {
      if (active == this.activeActionBoxMenu)
        return;

      if (active) {
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
      var needSignin = this.needGaiaSignin;
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
        // Switch to Gaia signin.
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

    /**
     * Shows signin UI for this user.
     */
    showSigninUI: function() {
      this.parentNode.showSigninUI(this.user.emailAddress);
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
      this.activeActionBoxMenu = !this.activeActionBoxMenu;
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
          if (this.parentNode.focusedPod_ && !this.activeActionBoxMenu)
            this.activeActionBoxMenu = true;
          e.stopPropagation();
          break;
        case 'Up':
        case 'Down':
          if (this.activeActionBoxMenu) {
            this.actionBoxMenuRemoveElement.tabIndex =
                UserPodTabOrder.PAD_MENU_ITEM;
            this.actionBoxMenuRemoveElement.focus();
          }
          e.stopPropagation();
          break;
        case 'U+001B':  // Esc
          this.activeActionBoxMenu = false;
          e.stopPropagation();
          break;
        default:
          this.activeActionBoxMenu = false;
          break;
      }
    },

    /**
     * Handles a click event on remove user command.
     * @param {Event} e Click event.
     */
    handleRemoveCommandClick_: function(e) {
      if (this.activeActionBoxMenu)
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
          this.activeActionBoxMenu = false;
          e.stopPropagation();
          break;
        default:
          this.actionBoxAreaElement.focus();
          this.activeActionBoxMenu = false;
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
      if (!this.signinButtonElement.hidden) {
        this.showSigninUI();
        // Prevent default so that we don't trigger 'focus' event.
        e.preventDefault();
      }
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
    get needGaiaSignin() {
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
   * Creates a new pod row element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var PodRow = cr.ui.define('podrow');

  PodRow.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Whether this user pod row is shown for the first time.
    firstShown_: true,

    // Whether the initial wallpaper load after boot has been requested. Used
    // only if |Oobe.getInstance().shouldLoadWallpaperOnBoot()| is true.
    bootWallpaperLoaded_: false,

    // True if inside focusPod().
    insideFocusPod_: false,

    // True if user pod has been activated with keyboard.
    // In case of activation with keyboard we delay wallpaper change.
    keyboardActivated_: false,

    // Focused pod.
    focusedPod_: undefined,

    // Activated pod, i.e. the pod of current login attempt.
    activatedPod_: undefined,

    // Pod that was most recently focused, if any.
    lastFocusedPod_: undefined,

    // When moving through users quickly at login screen, set a timeout to
    // prevent loading intermediate wallpapers.
    loadWallpaperTimeout_: null,

    // Pods whose initial images haven't been loaded yet.
    podsWithPendingImages_: [],

    /** @override */
    decorate: function() {
      this.style.left = 0;

      // Event listeners that are installed for the time period during which
      // the element is visible.
      this.listeners_ = {
        focus: [this.handleFocus_.bind(this), true],
        click: [this.handleClick_.bind(this), false],
        keydown: [this.handleKeyDown.bind(this), false]
      };
    },

    /**
     * Returns all the pods in this pod row.
     * @type {NodeList}
     */
    get pods() {
      return this.children;
    },

    /**
     * Return true if user pod row has only single user pod in it.
     * @type {boolean}
     */
    get isSinglePod() {
      return this.children.length == 1;
    },

    hideTitles: function() {
      for (var i = 0, pod; pod = this.pods[i]; ++i)
        pod.imageElement.title = '';
    },

    updateTitles: function() {
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        pod.imageElement.title = pod.user.nameTooltip || '';
      }
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
      if (user.publicAccount)
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

      // loadPods is called after user list update (for ex. after deleting user)
      // so make sure that tooltips are updated.
      $('pod-row').updateTitles();

      var columns = users.length < COLUMNS.length ?
          COLUMNS[users.length] : COLUMNS[COLUMNS.length - 1];
      var rows = Math.floor((users.length - 1) / columns) + 1;

      // Cancel any pending resize operation.
      this.removeEventListener('mouseout', this.deferredResizeListener_);

      if (!this.columns || !this.rows) {
        // Set initial dimensions.
        this.resize_(columns, rows);
      } else if (columns != this.columns || rows != this.rows) {
        // Defer the resize until mouse cursor leaves the pod row.
        this.deferredResizeListener_ = function(e) {
          if (!findAncestorByClass(e.toElement, 'podrow')) {
            this.resize_(columns, rows);
          }
        }.bind(this);
        this.addEventListener('mouseout', this.deferredResizeListener_);
      }

      this.focusPod(this.preselectedPod);
    },

    /**
     * Resizes the pod row and cancel any pending resize operations.
     * @param {number} columns Number of columns.
     * @param {number} rows Number of rows.
     * @private
     */
    resize_: function(columns, rows) {
      this.removeEventListener('mouseout', this.deferredResizeListener_);
      this.columns = columns;
      this.rows = rows;
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

      clearTimeout(this.loadWallpaperTimeout_);
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (!this.isSinglePod)
          pod.activeActionBoxMenu = false;
        if (pod != podToFocus) {
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
        if (hadFocus && this.keyboardActivated_) {
          // Delay wallpaper loading to let user tab through pods without lag.
          this.loadWallpaperTimeout_ = window.setTimeout(
              this.loadWallpaper_.bind(this), WALLPAPER_LOAD_DELAY_MS);
        } else if (!this.firstShown_) {
          // Load wallpaper immediately if there no pod was focused
          // previously, and it is not a boot into user pod list case.
          this.loadWallpaper_();
        }
        this.firstShown_ = false;
        this.lastFocusedPod_ = podToFocus;
      }
      this.insideFocusPod_ = false;
      this.keyboardActivated_ = false;
    },

    /**
     * Loads wallpaper for the active user pod, if any.
     * @private
     */
    loadWallpaper_: function() {
      if (this.focusedPod_)
        chrome.send('loadWallpaper', [this.focusedPod_.user.username]);
    },

    /**
     * Resets wallpaper to the last active user's wallpaper, if any.
     */
    loadLastWallpaper: function() {
      if (this.lastFocusedPod_)
        chrome.send('loadWallpaper', [this.lastFocusedPod_.user.username]);
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
          pod.activeActionBoxMenu = false;
      }

      // Clears focus if not clicked on a pod and if there's more than one pod.
      var pod = findAncestorByClass(e.target, 'pod');
      if ((!pod || pod.parentNode != this) && !this.isSinglePod) {
        this.focusPod();
      }

      // Return focus back to single pod.
      if (this.isSinglePod) {
        this.focusPod(this.focusedPod_, true /* force */);
      }
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
            // Boot transition: load wallpaper.
            if (!self.bootWallpaperLoaded_ &&
                Oobe.getInstance().shouldLoadWallpaperOnBoot()) {
              self.loadWallpaperTimeout_ = window.setTimeout(
                  self.loadWallpaper_.bind(self), WALLPAPER_BOOT_LOAD_DELAY_MS);
              self.bootWallpaperLoaded_ = true;
            }
          }
        });
      }
    },

    /**
     * Called right before the pod row is shown.
     */
    handleBeforeShow: function() {
      for (var event in this.listeners_) {
        this.ownerDocument.addEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
      $('login-header-bar').buttonsTabIndex = UserPodTabOrder.HEADER_BAR;
      this.updateTitles();
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
      this.hideTitles();
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
        chrome.send('userImagesLoaded');
      }
    }
  };

  return {
    PodRow: PodRow
  };
});
