// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview User pod row implementation.
 */

cr.define('login', function() {
  // Pod width. 170px Pod + 10px padding + 10px margin on both sides.
  const POD_WIDTH = 170 + 2 * (10 + 10);

  // Oauth token status. These must match UserManager::OAuthTokenStatus.
  const OAUTH_TOKEN_STATUS_UNKNOWN = 0;
  const OAUTH_TOKEN_STATUS_INVALID = 1;
  const OAUTH_TOKEN_STATUS_VALID = 2;

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
    return $('user-pod-template').cloneNode(true);
  });

  UserPod.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      // Make this focusable
      if (!this.hasAttribute('tabindex'))
        this.tabIndex = 0;

      this.addEventListener('click', this.handleClick_.bind(this));

      this.enterButtonElement.addEventListener('click',
          this.activate.bind(this));
      this.removeUserButtonElement.addEventListener('mouseout',
          this.handleRemoveButtonMouseOut_.bind(this));
    },

    /**
     * Initializes the pod after its properties set and added to a pod row.
     */
    initialize: function() {
      if (!this.isGuest) {
        this.passwordElement.addEventListener('keydown',
            this.parentNode.handleKeyDown.bind(this.parentNode));
      }
    },

    /**
     * Gets image element.
     * @type {!HTMLImageElement}
     */
    get imageElement() {
      return this.firstElementChild;
    },

    /**
     * Gets name element.
     * @type {!HTMLDivElement}
     */
    get nameElement() {
      return this.imageElement.nextElementSibling;
    },

    /**
     * Gets password field.
     * @type {!HTMLInputElement}
     */
    get passwordElement() {
      return this.nameElement.nextElementSibling;
    },

    /**
     * Gets guest enter button.
     * @type {!HTMLInputElement}
     */
    get enterButtonElement() {
      return this.passwordElement.nextElementSibling;
    },

    /**
     * Gets remove user button.
     * @type {!HTMLInputElement}
     */
    get removeUserButtonElement() {
      return this.lastElementChild;
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

      this.nameElement.textContent = userDict.name;
      this.imageElement.src = userDict.imageUrl;
      this.removeUserButtonElement.hidden = !userDict.canRemove;

      if (this.isGuest) {
        this.imageElement.title = userDict.name;
        this.enterButtonElement.hidden = false;
        this.passwordElement.hidden = true;
      } else {
        this.imageElement.title = userDict.emailAddress;
        this.passwordElement.hidden = false;
        this.enterButtonElement.hidden = true;
      }
    },

    /**
     * Whether we are a guest pod or not.
     */
    get isGuest() {
      return !this.user.emailAddress;
    },

    /**
     * Gets main input element.
     */
    get mainInput() {
      if (this.isGuest) {
        return this.enterButtonElement;
      } else {
        return this.passwordElement;
      }
    },

    /**
     * Whether remove button is active state.
     * @type {boolean}
     */
    get activeRemoveButton() {
      return this.removeUserButtonElement.classList.contains('active');
    },
    set activeRemoveButton(active) {
      if (active == this.activeRemoveButton)
        return;

      if (active) {
        this.removeUserButtonElement.classList.add('active');
        this.removeUserButtonElement.textContent =
            localStrings.getString('removeUser');
      } else {
        this.removeUserButtonElement.classList.remove('active');
        this.removeUserButtonElement.textContent = '';
      }
    },

    /**
     * Focuses on input element.
     */
    focusInput: function() {
      this.mainInput.focus();
    },

    /**
     * Activates the pod.
     * @return {boolean} True if activated successfully.
     */
    activate: function() {
      if (this.isGuest) {
        chrome.send('launchIncognito');
      } else {
        if (!this.passwordElement.value)
          return false;

        chrome.send('authenticateUser',
            [this.user.emailAddress, this.passwordElement.value]);
      }
      this.parentNode.rowEnabled = false;

      return true;
    },

    /**
     * Resets input field.
     * @param {boolean} takeFocus True to take focus.
     */
    reset: function(takeFocus) {
      this.parentNode.rowEnabled = true;
      this.passwordElement.value = '';

      if (takeFocus)
        this.mainInput.focus();
    },

    /**
     * Handles mouseout on remove button button.
     */
    handleRemoveButtonMouseOut_: function(e) {
      this.activeRemoveButton = false;
    },

    /**
     * Handles a click event on remove user button.
     */
    handleRemoveButtonClick_: function(e) {
      if (this.activeRemoveButton)
        chrome.send('removeUser', [this.user.emailAddress]);
      else
        this.activeRemoveButton = true;
    },

    /**
     * Handles click event.
     */
    handleClick_: function(e) {
      if (e.target == this.removeUserButtonElement) {
        this.handleRemoveButtonClick_(e);

        // Prevent default so that we don't trigger 'focus' event.
        e.preventDefault();
      }
    }
  };


  /**
   * Creates a new pod row element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var PodRow = cr.ui.define('podrow');

  PodRow.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Focused pod.
    focusedPod_ : undefined,

    // Activated pod, i.e. the pod of current login attempt.
    activatedPod_: undefined,

    /** @inheritDoc */
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
     */
    get pods() {
      return this.children;
    },

    // True when clicking on pods is enabled.
    rowEnabled_ : true,
    get rowEnabled() {
      return this.rowEnabled_;
    },
    set rowEnabled(enabled) {
      this.rowEnabled_ = enabled;
    },

    /**
     * Creates a user pod from given email.
     * @param {string} email User's email.
     */
    createUserPod: function(user) {
      var userPod = new UserPod({user: user});
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
     * Ensures the given pod is visible.
     * @param {UserPod} pod Pod to scroll into view.
     */
    scrollPodIntoView: function(pod) {
      var podIndex = this.findIndex_(pod);
      if (podIndex == -1)
        return;

      var left = podIndex * POD_WIDTH;
      var right = left + POD_WIDTH;

      var viewportLeft = -parseInt(this.style.left);
      var viewportRight = viewportLeft + this.parentNode.clientWidth;

      if (left < viewportLeft) {
        this.style.left = -left + 'px';
      } else if (right > viewportRight) {
        var offset = right - viewportRight;
        this.style.left = (viewportLeft - offset) + 'px';
      }
    },

    /**
     * Gets index of given pod or -1 if not found.
     * @param {UserPod} pod Pod to look up.
     * @private
     */
    findIndex_: function(pod) {
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
      for (var i = 0; i < this.pods.length; ++i) {
        window.setTimeout(removeClass, 500 + i * 70,
            this.pods[i], 'init');
        window.setTimeout(removeClass, 700 + i * 70,
            this.pods[i].nameElement, 'init');
      }
    },

    /**
     * Populates pod row with given existing users and
     * kick start init animiation.
     * @param {array} users Array of existing user emails.
     * @param {boolean} animated Whether to use init animation.
     */
    loadPods: function(users, animated) {
      // Clear existing pods.
      this.innerHTML = '';
      this.focusedPod_ = undefined;
      this.activatedPod_ = undefined;

      // Popoulate the pod row.
      for (var i = 0; i < users.length; ++i) {
        this.addUserPod(users[i], animated);
      }
    },

    /**
     * Focuses a given user pod or clear focus when given null.
     * @param {UserPod} pod User pod to focus or null to clear focus.
     */
    focusPod: function(pod) {
      if (this.focusedPod_ == pod)
        return;

      if (pod) {
        if (pod.isGuest ||
            localStrings.getString('authType') != 'ext' ||
            pod.user.oauthTokenStatus == OAUTH_TOKEN_STATUS_VALID ||
            !window.navigator.onLine) {
          // Focus the pod if it is guest pod, or
          // we are not using gaia ext for signin or
          // the user has a valid oauth token or
          // device is offline.
          for (var i = 0; i < this.pods.length; ++i) {
            this.pods[i].activeRemoveButton = false;
            if (this.pods[i] == pod) {
              pod.classList.remove("faded");
              pod.classList.add("focused");
              pod.tabIndex = -1;  // Make it not keyboard focusable.
            } else {
              this.pods[i].classList.remove('focused');
              this.pods[i].classList.add('faded');
              this.pods[i].tabIndex = 0;
            }
          }
          pod.focusInput();

          this.focusedPod_ = pod;
          this.scrollPodIntoView(pod);
        } else {
          // Otherwise, switch to Gaia signin.
          Oobe.showSigninUI(pod.user.emailAddress);
          this.focusPod();  // Clears current focus.
        }
      } else {
        for (var i = 0; i < this.pods.length; ++i) {
          this.pods[i].classList.remove('focused');
          this.pods[i].classList.remove('faded');
          this.pods[i].activeRemoveButton = false;
          this.pods[i].tabIndex = 0;
        }
        this.focusedPod_ = undefined;
      }
    },

    /**
     * Returns the currently activated pod.
     */
    get activated() {
      return this.activatedPod_;
    },

    /**
     * Activates given pod.
     * @param {UserPod} pod Pod to activate.
     */
    activatePod: function(pod) {
      if (!pod)
        return;

      if (pod.activate()) {
        this.activatedPod_ = pod;

        for (var i = 0; i < this.pods.length; ++i)
          this.pods[i].mainInput.disabled = true;
      }
    },

    /**
     * Start login success animation.
     */
    startAuthenticatedAnimation: function() {
      var activated = this.findIndex_(this.activatedPod_);
      if (activated == -1)
        return;

      for (var i = 0; i < this.pods.length; ++i) {
        if (i < activated)
          this.pods[i].classList.add('left');
        else if (i < activated)
          this.pods[i].classList.add('right');
        else
          this.pods[i].classList.add('zoom');
      }
    },

    /**
     * Resets input UI.
     * @param {boolean} takeFocus True to take focus.
     */
    reset: function(takeFocus) {
      this.rowEnabled = true;
      for (var i = 0; i < this.pods.length; ++i)
        this.pods[i].mainInput.disabled = false;

      if (this.activatedPod_)
        this.activatedPod_.reset(takeFocus);
    },

    /**
     * Handler of click event.
     * @param {Event} e Click Event object.
     * @private
     */
    handleClick_: function(e) {
      if (!this.rowEnabled)
        return;
      // Clears focus if not clicked on a pod.
      if (e.target.parentNode != this &&
          e.target.parentNode.parentNode != this)
        this.focusPod();
    },

    /**
     * Handles focus event.
     * @param {Event} e Focus Event object.
     * @private
     */
    handleFocus_: function(e) {
      if (!this.rowEnabled)
        return;
      if (e.target.parentNode == this) {
        // Focus on a pod
        if (e.target.classList.contains('focused'))
          e.target.focusInput();
        else
          this.focusPod(e.target);
      } else if (e.target.parentNode.parentNode == this) {
        // Focus on a control of a pod.
        if (!e.target.parentNode.classList.contains('focused')) {
          this.focusPod(e.target.parentNode);
          e.target.focus();
        }
      } else {
        // Clears pod focus when we reach here. It means new focus is neither
        // on a pod nor on a button/input for a pod.
        this.focusPod();
      }
    },

    /**
     * Handler of keydown event.
     * @param {Event} e KeyDown Event object.
     * @public
     */
    handleKeyDown: function(e) {
      var editing = false;
      if (e.target.tagName == 'INPUT' && e.target.value)
        editing = true;

      switch (e.keyIdentifier) {
        case 'Left':
          if (!editing) {
            if (this.focusedPod_ && this.focusedPod_.previousElementSibling)
              this.focusPod(this.focusedPod_.previousElementSibling);
            else
              this.focusPod(this.lastElementChild);

            e.stopPropagation();
          }
          break;
        case 'Right':
          if (!editing) {
            if (this.focusedPod_ && this.focusedPod_.nextElementSibling)
              this.focusPod(this.focusedPod_.nextElementSibling);
            else
              this.focusPod(this.firstElementChild);

            e.stopPropagation();
          }
          break;
        case 'Enter':
          if (this.focusedPod_) {
            this.activatePod(this.focusedPod_);
            e.stopPropagation();
          }
          break;
      }
    },

    /**
     * Called when the element is shown.
     */
    handleShow: function() {
      for (var event in this.listeners_) {
        this.ownerDocument.addEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
    },

    /**
     * Called when the element is hidden.
     */
    handleHide: function() {
      for (var event in this.listeners_) {
        this.ownerDocument.removeEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
    }
  };

  return {
    PodRow: PodRow
  };
});
