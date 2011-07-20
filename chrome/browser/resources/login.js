// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login Screen
 * This is the main code for the login screen.
 */

/**
 * Simple common assertion API
 * @param {*} condition The condition to test.  Note that this may be used to
 *     test whether a value is defined or not, and we don't want to force a
 *     cast to Boolean.
 * @param {string=} opt_message A message to use in any error.
 */
function assert(condition, opt_message) {
  if (condition)
    return;

  var msg = 'Assertion failed';
  if (opt_message)
    msg = msg + ': ' + opt_message;
  throw new Error(msg);
}

var LoginScreen = (function() {
  'use strict';

  function LoginScreen() {
  }

  LoginScreen.prototype = {
    /**
     * The guest card object
     * @type {Slider|undefined}
     * private
     */
    guestCard_: null,

    /**
     * The Slider object to use for scrolling through users.
     * @type {Slider|undefined}
     */
    slider_: null,

    /**
     * Template to use for creating new 'user-card' elements
     * @type {!Element|undefined}
     */
    userCardTemplate_: null,

    /**
     * Indicates whether the shift key is down.
     * @type {bool}
     */
    isShiftPressed_: false,

    /**
     * Indicates whether the alt key is down.
     * @type {bool}
     */
    isAltPressed_: false,

    /**
     * Indicates whether the ctrl key is down.
     * @type {bool}
     */
    isCtrlPressed_: false,

    /**
     * Holds all event handlers tied to users (and so subject to removal when
     * the user list is refreshed)
     * @type {!EventTracker}
     */
    userEvents_: null,

    /**
     * Removes all children of an element which have a given class in
     * their classList.
     * @param {!Element} element The parent element to examine.
     * @param {string} className The class to look for.
     */
    removeChildrenByClassName_: function (element, className) {
      var child = element.firstElementChild;
      while (child) {
        var prev = child;
        child = child.nextElementSibling;
        if (prev.classList.contains(className))
          element.removeChild(prev);
      }
    },

    /**
     * Search an elements ancestor chain for the nearest element that is a
     * member of the specified class.
     * @param {!Element} element The element to start searching from.
     * @param {string} className The name of the class to locate.
     * @return {Element} The first ancestor of the specified class or null.
     */
    getParentByClassName_: function(element, className) {
      for (var e = element; e; e = e.parentElement) {
        if (e.classList.contains(className))
          return e;
      }
      return null;
    },

    /**
     * Create a new user element and attach it to the end of the slider.
     * @param {!Element} parent The element where the user should be inserted.
     * @param {!Object} user The user object to user box for.
     */
    addUser: function(parent, user, index) {
      // Make a deep copy of the template and clear its ID
      var containerElement = this.userCardTemplate_.cloneNode(true);
      this.userCardTemplate_.id = null;

      var userElement = containerElement.getElementsByClassName('user')[0];
      assert(userElement, 'Expected user-template to have a user child');
      assert(typeof(user.name) == 'string',
            'Expected every user to have a name.');

      userElement.setAttribute('display-name', user.name);
      userElement.setAttribute('user-name', user.emailAddress);
      userElement.setAttribute('user-index', index);

      // Find the span element (if any) and fill it in with the app name
      var span = userElement.querySelector('span');
      if (span)
        span.textContent = user.name;

      // Fill in the image
      var userImg = userElement.getElementsByClassName('user-image')[0];
      if (userImg) {
        userImg.style.backgroundImage = "url('" + user.imageUrl + "')";
        // We put a click handler just on the user image - so clicking on the
        // margins between users doesn't do anything
        var self = this;
        this.userEvents_.add(userImg, 'click', function(e) {
          self.userClick(e);
        }, false);
      }
      var userLoginBox =
          userElement.getElementsByClassName('user-login-box')[0];
      var guestLoginBox =
          userElement.getElementsByClassName('guest-login-box')[0];
      var isGuest = !user.email_address;
      if (!isGuest) {
        containerElement.addEventListener(Slider.EventType.ACTIVATE,
                                          this.userActivateCard);
        containerElement.addEventListener(Slider.EventType.DEACTIVATE,
                                          this.userDeactivateCard);

        var deleteButton =
            userElement.getElementsByClassName('delete-button')[0];
        deleteButton.addEventListener('click', this.userDeleteCard);

        userLoginBox.addEventListener('keypress', function(e) {
          userLoginKeyPress(e, userElement);
        });
      } else {
        this.guestCard_ = containerElement;

        containerElement.addEventListener(Slider.EventType.ACTIVATE,
                                          this.guestActivateCard);
        containerElement.addEventListener(Slider.EventType.DEACTIVATE,
                                          this.guestDeactivateCard);

        userLoginBox.hidden = true;

        var loginButton =
          guestLoginBox.getElementsByClassName('loginbutton')[0];
        loginButton.addEventListener('click', function(e) {
          chrome.send('launchIncognito', []);
          // Don't allow the click to trigger a link or anything
          e.preventDefault();
          // Prevent the user-frame from receiving the event.
          e.stopPropagation();
        }, true);
      }

      // Insert at the end of the provided page
      parent.appendChild(containerElement);
      // Display the container element
      containerElement.hidden = false;
    },

    /**
     * Invoked when a user is clicked
     * @param {Event} e The click event.
     */
    userClick: function(e) {
      var target = e.currentTarget;
      var user = this.getParentByClassName_(target, 'user');
      assert(user, 'userClick should have been on a descendant of a user');

      var userIndex = user.getAttribute('user-index');
      assert(userIndex, 'unexpected user without userIndex');

      if (this.slider.currentCardIndex == null) {
        this.slider.currentCardIndex = userIndex;
      } else {
        // get out of user selected mode.
        this.slider.currentCardIndex = null;
      }

      // Don't allow the click to trigger a link or anything
      e.preventDefault();
      // Prevent the user-frame from receiving the event.
      e.stopPropagation();
    },

    /**
     * Invoked when the selected user card changes
     * @param {Event} e The selection changed event.
     */
    userFrameSelectionChanged: function(e) {
      var userCards = $('user-list').getElementsByClassName('user-card');
      for (var i = 0; i < userCards.length; i++) {
        if (e.activeCardIndex == null || i == e.activeCardIndex) {
          userCards[i].classList.remove('user-card-grayed');
        } else {
          userCards[i].classList.add('user-card-grayed');
        }
      }
    },

    /**
     * Invoked when the guest card is activated.
     * @param {Event} e The Activate Card event
     */
    guestActivateCard: function(e) {
      var card = e.target;
      var user = card.getElementsByClassName('user')[0];

      // Focus the card.
      user.classList.add('focus');

      assert(user, 'user element not found');

      // Hide the guest box.
      var guestBox = user.querySelector('span');
      guestBox.hidden = true;

      // This is a guest.
      var guestLoginBox = user.getElementsByClassName('guest-login-box')[0];
      assert(guestLoginBox, 'guest login box not found');
      // Show the guest login box.
      guestLoginBox.hidden = false;
    },

    /**
     * Invoked when the guest card is deactivated.
     * @param {Event} e The Deactivate Card event
     */
    guestDeactivateCard: function(e) {
      var card = e.target;
      var user = card.getElementsByClassName('user')[0];
      assert(user, 'user element not found');
      // Show the guest box.
      var guestBox = user.querySelector('span');
      guestBox.hidden = false;
      // Unfocus the card.
      user.classList.remove('focus');
      // Hide the guest login box
      var guestLoginBox = user.getElementsByClassName('guest-login-box')[0];
      assert(guestLoginBox, 'guest login box not found');
      guestLoginBox.hidden = true;
    },

    /**
     * Invoked when the user card delete button is clicked
     * @param {Event} e The Click event.
     */
    userDeleteCard: function(e) {
      var deleteButton = e.target;
      var userImageBorderBox = deleteButton.parentNode;
      var userElement = userImageBorderBox.parentNode;
      var userCard = userElement.parentNode;
      $('user-list').removeChild(userCard);

      // Update the user indicies
      var usersOnList = $('user-list').getElementsByClassName('user-card');
      for (var userIndex = 0; userIndex < usersOnList.length; userIndex++) {
        var uElement = usersOnList[userIndex].getElementsByClassName('user')[0];
        uElement.setAttribute('user-index', userIndex);
      }
      this.updateSliderCards();

      chrome.send('removeUser', [getUserName(userElement)]);
    },

    /**
     * Invoked when the user card is activated.
     * @param {Event} e The Activate Card event
     */
    userActivateCard: function(e) {
      var card = e.target;
      var user = card.getElementsByClassName('user')[0];
      assert(user, 'user element not found');

      // Focus the card.
      user.classList.add('focus');

      // Hide the email box.
      var emailBox = user.querySelector('span');
      emailBox.hidden = true;

      // Show the login box
      var userLoginBox = user.getElementsByClassName('user-login-box')[0];
      assert(userLoginBox, 'user login box not found');
      userLoginBox.hidden = false;

      // Clear and focus the password box.
      var passwordBox = userLoginBox.getElementsByClassName('passwordbox')[0];
      assert(passwordBox, 'passwordBox element not found');
      passwordBox.value = '';
      passwordBox.focus();

      // Show the delete button.
      var deleteButton = user.getElementsByClassName('delete-button')[0];
      deleteButton.hidden = false;
    },

    /**
     * Invoked when the user card is deactivated.
     * @param {Event} e The Deactivate Card event
     */
    userDeactivateCard: function(e) {
      var card = e.target;
      var user = card.getElementsByClassName('user')[0];
      assert(user, 'user element not found');

      // Unfocus the card.
      user.classList.remove('focus');

      // Hide the user login box
      var userLoginBox = user.getElementsByClassName('user-login-box')[0];
      assert(userLoginBox, 'user login box not found');
      userLoginBox.hidden = true;

      // Show the email box.
      var emailBox = user.querySelector('span');
      emailBox.hidden = false;

      // Hide the delete button.
      var deleteButton = user.getElementsByClassName('delete-button')[0];
      deleteButton.hidden = true;
    },

    /**
     * Invoked whenever the users in user-list have changed so that
     * the Slider knows about the new elements.
     */
    updateSliderCards: function() {
      var userCards = $('user-list').getElementsByClassName('user-card');
      var cardArray = [];
      for (var i = 0; i < userCards.length; i++)
        cardArray[i] = userCards[i];
      this.slider.setCards(cardArray, 0);
    },

    getUsersCallback: function(users) {
      this.userEvents_.removeAll();
      // grab the add-user-card before we remove it from the list.
      var addUserCard = $('add-user-card');
      // remove all user cards
      this.removeChildrenByClassName_($('user-list'), 'user-card');

      // Add the "Add User" card to the user list.
      $('user-list').appendChild(addUserCard);
      // Add the users to the user list.
      for (var i = 0; i < users.length; i++) {
        var user = users[i];
        this.addUser($('user-list'), user, i + 1);
      }

      // Tell the slider about the pages
      this.updateSliderCards();

      var dummy = $('user-frame').offsetWidth;
      $('user-frame').style.visibility = 'visible';
    },

    /**
     * Returns the 'display-name' attribute of a 'user' element.
     * @param {!Element} element The 'user' element.
     */
    getDisplayName: function(userElement) {
      // TODO: verify type of user element
      return userElement.getAttribute('display-name');
    },

    /**
     * Returns the 'user-name' attribute of a 'user' element.
     * @param {!Element} element The 'user' element.
     */
    getUserName: function(userElement) {
      // TODO: verify type of user element
      return userElement.getAttribute('user-name');
    },

    /**
     * Handles keyboard navigation of the login screen.
     * @param {!Event} element The keypress event.
     */
    userLoginKeyPress: function(e, userElement) {
      if (e.keyCode == 13) {
        e.preventDefault();
        assert(userElement.classList.contains('user'),
            'The parent element is not a user element.');

        var userImg = userElement.getElementsByClassName('user-image')[0];
        var userLoginBox =
            userElement.getElementsByClassName('user-login-box')[0];
        var passwordBox = userLoginBox.getElementsByClassName('passwordbox')[0];

        var userName = getUserName(userElement);
        var password = passwordBox.value;
        if (password.length > 0) {
          chrome.send('authenticateUser', [userName, password]);
          return false;
        }
      }
      return true;
    },

    loginKeyDown: function(e) {
      switch (e.keyCode) {
        // shift key down
        case 16:
          this.isShiftPressed_ = true;
          break;
        // ctrl key down
        case 17:
          this.isCtrlPressed_ = true;
          break;
        // alt key down
        case 18:
          this.isAltPressed_ = true;
        break;
      }
      var keystroke = String.fromCharCode(e.keyCode);
      switch (keystroke) {
        case 'U':
          if (this.isAltPressed_) {
            this.slider.currentCardIndex = 0;
            // focus the email box.
            var emailBox =
              $('add-user-card').getElementsByClassName('emailbox')[0];
            assert(emailBox, 'Add User e-mail box not found');
            emailBox.focus();
          }
          break;
        case 'P':
          if (this.isAltPressed_) {
            if (this.slider.currentCard == null) {
              this.slider.currentCardIndex = 0;
            }
            var passwordBox =
              this.slider.currentCard.getElementsByClassName('passwordbox')[0];
            assert(passwordBox, 'password box not found');
            passwordBox.focus();
          }
          break;
        case 'B':
          if (this.isAltPressed_) {
            var guestElement =
              this.guestCard_.getElementsByClassName('user')[0];
            var guestIndex = guestElement.getAttribute('user-index');
            assert(guestIndex, 'unexpected user without userIndex');
            this.slider.currentCardIndex = guestIndex;
            setTimeout(function() {
              // TODO(fsamuel): Make this call a common function.
              chrome.send('launchIncognito', []);
            }, 50);
          }
          break;
        default:
          switch (e.keyCode) {
            // enter pressed.
            case 13:
              if (this.slider.currentCard == guestCard_) {
                // TODO(fsamuel): Enter Incognito mode
              }
              break;
            // escape pressed.
            case 27:
              // If we pressed the escape key then don't select any user.
              this.slider.currentCardIndex = null;
              break;
            // left arrow pressed.
            case 37:
              if (this.slider.currentCardIndex == null) {
                // TODO(fsamuel): This should be go to last active card + 1
                this.slider.currentCardIndex = this.slider.cardCount() - 1;
              } else if (this.slider.currentCardIndex > 0) {
                this.slider.currentCardIndex -= 1;
              }
              break;
            // right arrow pressed.
            case 39:
              if (this.slider.currentCardIndex == null) {
                // TODO(fsamuel): This should be go to last active card - 1
                this.slider.currentCardIndex = 0;
              } else if (this.slider.currentCardIndex <
                           this.slider.cardCount() - 1) {
                this.slider.currentCardIndex += 1;
              }
              break;
          } // switch
      } // switch
    },

    loginKeyUp: function(e) {
      switch (e.keyCode) {
        // shift key up.
        case 16:
          this.isShiftPressed_ = false;
          break;
        // ctrl key up.
        case 17:
          this.isCtrlPressed_ = false;
          break;
        // alt key up.
        case 18:
          this.isAltPressed_ = false;
        break;
      }
    },

    /**
     * Creates the Add User card and inserts it into the slider.
     * TODO(fsamuel): This is here only temporarily. We should eventually remove
     * this card.
     */
    initializeAddUserCard: function(e) {
      var emailBox = $('add-user-card').getElementsByClassName('emailbox')[0];
      assert(emailBox, 'Add User e-mail box not found');
      var passwordBox =
        $('add-user-card').getElementsByClassName('passwordbox')[0];
      assert(passwordBox, 'Add User password box not found');

      var user = $('add-user-card').getElementsByClassName('user')[0];
      // Clicking the Add User icon hides the icon and shows
      // the user name and password input boxes
      var self = this;
      $('add-user').addEventListener('click', function(e) {
        self.userClick(e);
      });
      $('add-user-card').addEventListener(Slider.EventType.ACTIVATE,
                                          function(e) {
        //$('add-user-spacer').style.display = 'block';
        // Focus the card.
        user.classList.add('focus');

        // Hide the Add User Icon, and show the input boxes.
        $('add-user').parentNode.hidden = true;
        $('add-user-box').hidden = false;

        // Focus the email box and clear the password box.
        emailBox.focus();
        passwordBox.value = '';
      }, false);

      $('add-user-card').addEventListener(Slider.EventType.DEACTIVATE,
                                          function(e) {
        //$('add-user-spacer').style.display = 'none';
        // Unfocus the card.
        user.classList.remove('focus');

        $('add-user').parentNode.hidden = false;
        $('add-user-box').hidden = true;
        document.body.focus();
      }, false);

      // Prevent blurring of a textfield if focus is shifted to the body.
      // This makes sure that the keyboard remains up until we hide the
      // Add User box.
      var focusedElement = null;
      emailBox.addEventListener('focus', function(e) {
        focusedElement = emailBox;
      });
      passwordBox.addEventListener('focus', function(e) {
        focusedElement = passwordBox;
      });
      document.body.addEventListener('focus', function(e) {
        if (focusedElement) {
          focusedElement.focus();
          e.preventDefault();
        }
      });

      // Prevent the slider from receiving click events and canceling
      // the Add User card.
      $('add-user-box').addEventListener('click', function(e) {
        e.preventDefault();
        e.stopPropagation();
      }, false);

      emailBox.addEventListener('keypress', function(e) {
        e.stopPropagation();
        if (e.keyCode == 13) {
          if (emailBox.value.length > 0) {
            passwordBox.focus();
          }
        }
      });

      passwordBox.addEventListener('keypress', function(e) {
        if (e.keyCode == 13) {
          chrome.send('authenticateUser', [emailBox.value, passwordBox.value]);
        }
      }, true);
    },

    initialize: function() {
      this.userEvents_ = new EventTracker();
      // Request data on the users so we can fill them in.
      // Note that this is kicked off asynchronously.  'getUsersCallback' will
      // be invoked at some point after this function returns.
      chrome.send('getUsers');

      // Prevent touch events from triggering any sort of native scrolling
      document.addEventListener('touchmove', function(e) {
        e.preventDefault();
      }, true);

      this.userCardTemplate_ = $('user-template');
      this.userCardTemplate_.id = null;
      $('user-list').removeChild(this.userCardTemplate_);
      $('user-frame').addEventListener(Slider.EventType.SELECTION_CHANGED,
                                       this.userFrameSelectionChanged);

      // Initialize the slider without any cards at the moment
      this.slider = new Slider($('user-frame'), $('user-list'), []);
      this.slider.initialize();

      this.initializeAddUserCard();

      // Bind on bubble phase
      var self = this;
      $('user-frame').addEventListener('click', function() {
        self.slider.currentCardIndex = null;
      }, false);
      document.addEventListener('keydown', function(e) {
        self.loginKeyDown(e);
      }, true);
      document.addEventListener('keyup', function(e) {
        self.loginKeyUp(e);
      }, true);
    }
  };

  return LoginScreen;
})();


var getUsersCallback = null;

document.addEventListener('DOMContentLoaded', function() {
  var loginScreenObj = new LoginScreen();
  getUsersCallback = function(users) {
    // call with the appropriate this.
    loginScreenObj.getUsersCallback(users);
  }
  loginScreenObj.initialize();
}, false);

// Disable text selection.
document.onselectstart = function(e) {
  e.preventDefault();
}

// Disable dragging.
document.ondragstart = function(e) {
  e.preventDefault();
}
