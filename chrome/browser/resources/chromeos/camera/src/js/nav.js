// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for navigation of views.
 */
cca.nav = cca.nav || {};

/**
 * All views stacked in ascending z-order (DOM order) for navigation, and only
 * the topmost visible view is active (clickable/focusable).
 * @type {Array.<cca.views.View>}
 */
cca.nav.views_ = [];

/**
 * Index of the current topmost visible view in the stacked views.
 * @type {number}
 */
cca.nav.topmostIndex_ = -1;

/**
 * Sets up navigation for all views, e.g. camera-view, dialog-view, etc.
 * @param {Array.<cca.views.View>} views All views in ascending z-order.
 */
cca.nav.setup = function(views) {
  cca.nav.views_ = views;
  // Manage all tabindex usages in cca.nav for navigation.
  document.querySelectorAll('[tabindex]').forEach(
      (element) => cca.util.makeUnfocusableByMouse(element));
};

/**
 * Finds the view by its id in the stacked views.
 * @param {string} id View identifier.
 * @return {number} Index of the view found; otherwise, -1.
 * @private
 */
cca.nav.findIndex_ = function(id) {
  return cca.nav.views_.findIndex((view) => view.root.id == id);
};

/**
 * Finds the next topmost visible view in the stacked views.
 * @return {number} Index of the view found; otherwise, -1.
 * @private
 */
cca.nav.findNextTopmostIndex_ = function() {
  for (var i = cca.nav.topmostIndex_ - 1; i >= 0; i--) {
    if (cca.nav.isShown_(i)) {
      return i;
    }
  }
  return -1;
};

/**
 * Checks if the view is already shown.
 * @param {number} index Index of the view.
 * @return {boolean} Whether the view is shown or not.
 * @private
 */
cca.nav.isShown_ = function(index) {
  return document.body.classList.contains(cca.nav.views_[index].root.id);
};

/**
 * Shows the view indexed in the stacked views and activates the view only if
 * it becomes the topmost visible view.
 * @param {number} index Index of the view.
 * @return {cca.views.View} View shown.
 * @private
 */
cca.nav.show_ = function(index) {
  var view = cca.nav.views_[index];
  if (!cca.nav.isShown_(index)) {
    document.body.classList.add(view.root.id);
    view.layout();
    if (index > cca.nav.topmostIndex_) {
      if (cca.nav.topmostIndex_ >= 0) {
        cca.nav.inactivate_(cca.nav.topmostIndex_);
      }
      cca.nav.activate_(index);
      cca.nav.topmostIndex_ = index;
    }
  }
  return view;
};

/**
 * Hides the view indexed in the stacked views and inactivate the view if it was
 * the topmost visible view.
 * @param {number} index Index of the view.
 * @private
 */
cca.nav.hide_ = function(index) {
  if (index == cca.nav.topmostIndex_) {
    cca.nav.inactivate_(index);
    var next = cca.nav.findNextTopmostIndex_();
    if (next >= 0) {
      cca.nav.activate_(next);
    }
    cca.nav.topmostIndex_ = next;
  }
  document.body.classList.remove(cca.nav.views_[index].root.id);
}

/**
 * Activates the view to be focusable.
 * @param {number} index Index of the view.
 */
cca.nav.activate_ = function(index) {
  // Restore the view's child elements' tabindex and then focus the view.
  var view = cca.nav.views_[index];
  view.root.querySelectorAll('[data-tabindex]').forEach((element) => {
    element.setAttribute('tabindex', element.dataset.tabindex);
    element.removeAttribute('data-tabindex');
  });
  view.focus();
};

/**
 * Inactivates the view to be unfocusable.
 * @param {number} index Index of the view.
 */
cca.nav.inactivate_ = function(index) {
  var view = cca.nav.views_[index];
  view.root.querySelectorAll('[tabindex]').forEach((element) => {
    element.dataset.tabindex = element.getAttribute('tabindex');
    element.setAttribute('tabindex', '-1');
  });
  document.activeElement.blur();
};

/**
 * Sets the element's tabindex on the view.
 * @param {cca.views.View} view View that the element is on.
 * @param {HTMLElement} element Element whose tabindex to be set.
 * @param {number} tabindex Tab-index of the element.
 */
cca.nav.setTabIndex = function(view, element, tabIndex) {
  if ((cca.nav.topmostIndex_ >= 0) &&
      (cca.nav.views_[cca.nav.topmostIndex_] == view)) {
    element.tabIndex = tabIndex;
  } else {
    // Remember tabindex by data attribute if the view isn't active.
    element.tabIndex = -1;
    element.dataset.tabindex = tabIndex + '';
  }
};

/**
 * Opens a navigation session of the view; shows/hides a view upon entering and
 * leaving the view.
 * @param {string} id View identifier.
 * @param {...*} args Optional rest parameters for navigating the view.
 * @return {!Promise<*>} Promise for the operation or result.
 * @private
 */
cca.nav.open_ = function(id, ...args) {
  // Enter the view shown; its navigation ends after leaving the view.
  var index = cca.nav.findIndex_(id);
  return cca.nav.show_(index).enter(...args).finally(() => {
    cca.nav.hide_(index);
  });
};

/**
 * Opens a navigation session of camera-view.
 * @return {!Promise} Promise for the operation.
 */
cca.nav.camera = function() {
  return cca.nav.open_('camera');
};

/**
 * Opens a navigation session of browser-view.
 * @param {cca.models.Gallery.Picture} picture Picture to be selected.
 * @return {!Promise} Promise for the operation.
 */
cca.nav.browser = function(picture) {
  return cca.nav.open_('browser', picture);
};

/**
 * Opens a navigation session of dialog-view.
 * @param {string} message Message of the dialog.
 * @param {boolean} cancellable Whether the dialog is cancellable.
 * @return {!Promise<boolean>} Promise for the result.
 */
cca.nav.dialog = function(message, cancellable) {
  return cca.nav.open_('dialog', message, cancellable);
};

/**
 * Handles pressed keys on the topmost visible view.
 * @param {Event} event Key press event.
 */
cca.nav.onKeyPressed = function(event) {
  if (cca.nav.topmostIndex_ >= 0 &&
      !document.body.classList.contains('has-error')) {
    cca.nav.views_[cca.nav.topmostIndex_].onKeyPressed(event);
  }
};

/**
 * Handles resized window on current all visible views.
 */
cca.nav.onWindowResized = function() {
  // All visible views need being relayout after window is resized.
  for (var i = cca.nav.views_.length - 1; i >= 0; i--) {
    if (cca.nav.isShown_(i)) {
      cca.nav.views_[i].layout();
    }
  }
};
