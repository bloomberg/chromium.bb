// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI header bar implementation. The header bar is shown on
 * top of login UI (not to be confused with login.HeaderBar which is shown at
 * the bottom and contains login shelf elements).
 * It contains device version labels, and new-note action button if it's
 * enabled (which should only be the case on lock screen).
 */

cr.define('login', function() {
  /**
   * Creates a header bar element shown at the top of the login screen.
   *
   * @constructor
   * @extends {HTMLDivElement}
   */
  var TopHeaderBar = cr.ui.define('div');

  /**
   * Calculates diagonal length of a rectangle with the provided sides.
   * @param {!number} x The rectangle width.
   * @param {!number} y The rectangle height.
   * @return {!number} The rectangle diagonal.
   */
  function diag(x, y) {
    return Math.sqrt(x * x + y * y);
  }

  TopHeaderBar.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * The current state of lock screen apps.
     * @private {!LockScreenAppsState}
     */
    lockScreenAppsState_: LOCK_SCREEN_APPS_STATE.NONE,

    set lockScreenAppsState(state) {
      if (this.lockScreenAppsState_ == state)
        return;

      this.lockScreenAppsState_ = state;
      this.updateUi_();
    },

    /** override */
    decorate: function() {
      $('new-note-action')
          .addEventListener('click', this.activateNoteAction_.bind(this));
      $('new-note-action')
          .addEventListener(
              'keydown', this.handleNewNoteActionKeyDown_.bind(this));
    },

    /**
     * @private
     */
    updateUi_: function() {
      // Shorten transition duration when moving to available state, in
      // particular when moving from foreground state - when moving from
      // foreground state container gets cropped to top right corner
      // immediately, while action element is updated from full screen with
      // a transition. If the transition is too long, the action would be
      // displayed as full square for a fraction of a second, which can look
      // janky.
      if (this.lockScreenAppsState_ == LOCK_SCREEN_APPS_STATE.AVAILABLE) {
        $('new-note-action')
            .classList.toggle('new-note-action-short-transition', true);
        this.runOnNoteActionTransitionEnd_(function() {
          $('new-note-action')
              .classList.toggle('new-note-action-short-transition', false);
        });
      }

      $('top-header-bar')
          .classList.toggle(
              'version-labels-unset',
              this.lockScreenAppsState_ == LOCK_SCREEN_APPS_STATE.FOREGROUND ||
                  this.lockScreenAppsState_ ==
                      LOCK_SCREEN_APPS_STATE.BACKGROUND);

      $('new-note-action-container').hidden =
          this.lockScreenAppsState_ != LOCK_SCREEN_APPS_STATE.AVAILABLE &&
          this.lockScreenAppsState_ != LOCK_SCREEN_APPS_STATE.FOREGROUND;

      $('new-note-action')
          .classList.toggle(
              'disabled',
              this.lockScreenAppsState_ != LOCK_SCREEN_APPS_STATE.AVAILABLE);

      $('new-note-action-icon').hidden =
          this.lockScreenAppsState_ != LOCK_SCREEN_APPS_STATE.AVAILABLE;

      // This might get set when the action is activated - reset it when the
      // lock screen action is updated.
      $('new-note-action-container')
          .classList.toggle('new-note-action-above-login-header', false);

      if (this.lockScreenAppsState_ != LOCK_SCREEN_APPS_STATE.FOREGROUND) {
        // Reset properties that might have been set as a result of activating
        // new note action.
        $('new-note-action-container')
            .classList.toggle('new-note-action-container-activated', false);
        $('new-note-action').style.removeProperty('border-bottom-left-radius');
        $('new-note-action').style.removeProperty('border-bottom-right-radius');
        $('new-note-action').style.removeProperty('height');
        $('new-note-action').style.removeProperty('width');
      }
    },

    /**
     * Handler for key down event.
     * @param {!KeyboardEvent} evt The key down event.
     * @private
     */
    handleNewNoteActionKeyDown_: function(evt) {
      if (evt.code != 'Enter')
        return;
      this.activateNoteAction_();
    },

    /**
     * @private
     */
    activateNoteAction_: function() {
      $('new-note-action').classList.toggle('disabled', true);
      $('new-note-action-icon').hidden = true;
      $('top-header-bar').classList.toggle('version-labels-unset', true);

      this.runOnNoteActionTransitionEnd_(
          (function() {
            if (this.lockScreenAppsState_ != LOCK_SCREEN_APPS_STATE.AVAILABLE)
              return;
            chrome.send(
                'setLockScreenAppsState',
                [LOCK_SCREEN_APPS_STATE.LAUNCH_REQUESTED]);
          }).bind(this));

      var container = $('new-note-action-container');
      container.classList.toggle('new-note-action-container-activated', true);
      container.classList.toggle('new-note-action-above-login-header', true);

      var newNoteAction = $('new-note-action');
      // Update new-note-action size to cover full screen - the element is a
      // circle quadrant, intent is for the whole quadrant to cover the screen
      // area, which means that the element size (radius) has to be set to the
      // container diagonal. Note that, even though the final new-note-action
      // element UI state is full-screen, the element is kept as circle quadrant
      // for purpose of transition animation (to get the effect of the element
      // radius increasing until it covers the whole screen).
      var targetSize =
          '' + diag(container.clientWidth, container.clientHeight) + 'px';
      newNoteAction.style.setProperty('width', targetSize);
      newNoteAction.style.setProperty('height', targetSize);
      newNoteAction.style.setProperty(
          isRTL() ? 'border-bottom-right-radius' : 'border-bottom-left-radius',
          targetSize);
    },

    /**
     * Waits for new note action element transition to end (the element expands
     * from top right corner to whole screen when the action is activated) and
     * then runs the provided closure.
     *
     * @param {!function()} callback Closure to run on transition end.
     */
    runOnNoteActionTransitionEnd_: function(callback) {
      $('new-note-action').addEventListener('transitionend', function listen() {
        $('new-note-action').removeEventListener('transitionend', listen);
        callback();
      });
      ensureTransitionEndEvent($('new-note-action'));
    }
  };

  return {TopHeaderBar: TopHeaderBar};
});
