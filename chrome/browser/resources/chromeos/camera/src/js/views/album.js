// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for views.
 */
camera.views = camera.views || {};

/**
 * Creates the Album view controller.
 *
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @extends {camera.views.GalleryBase}
 * @constructor
 */
camera.views.Album = function(context, router) {
  camera.views.GalleryBase.call(
      this, context, router, document.querySelector('#album'), 'album');

  /**
   * @type {camera.util.SmoothScroller}
   * @private
   */
  this.scroller_ = new camera.util.SmoothScroller(
      document.querySelector('#album'),
      document.querySelector('#album .padder'));

  /**
   * Makes the album scrollable by dragging with mouse.
   * @type {camera.util.MouseScroller}
   * @private
   */
  this.mouseScroller_ = new camera.util.MouseScroller(this.scroller_);

  /**
   * @type {camera.VerticalScrollBar}
   * @private
   */
  this.scrollBar_ = new camera.VerticalScrollBar(this.scroller_);

  /**
   * Sequential index of the last inserted picture. Used to generate unique
   * identifiers.
   *
   * @type {number}
   * @private
   */
  this.lastPictureIndex_ = 0;

  /**
   * Detects whether scrolling is being performed or not.
   * @type {camera.util.ScrollTracker}
   * @private
   */
  this.scrollTracker_ = new camera.util.ScrollTracker(
      this.scroller_, function() {}, this.onScrollEnded_.bind(this));

  // End of properties, seal the object.
  Object.seal(this);

  // Listen for clicking on the delete button.
  document.querySelector('#album-print').addEventListener(
      'click', this.onPrintButtonClicked_.bind(this));
  document.querySelector('#album-export').addEventListener(
      'click', this.onExportButtonClicked_.bind(this));
  document.querySelector('#album-delete').addEventListener(
      'click', this.onDeleteButtonClicked_.bind(this));
  document.querySelector('#album-back').addEventListener(
      'click', this.router.back.bind(this.router));
};

camera.views.Album.prototype = {
  __proto__: camera.views.GalleryBase.prototype
};

/**
 * Enters the view.
 * @override
 */
camera.views.Album.prototype.onEnter = function() {
  this.onResize();
  this.updateButtons_();
  this.scrollTracker_.start();
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Album.prototype.onLeave = function() {
  this.scrollTracker_.stop();
};

/**
 * @override
 */
camera.views.Album.prototype.onActivate = function() {
  camera.views.GalleryBase.prototype.onActivate.apply(this, arguments);
  if (!this.selectedIndexes.length && this.pictures.length)
    this.setSelectedIndex(this.pictures.length - 1);

  // Update the focus. If scrolling, then it will be updated once scrolling
  // is finished in onScrollEnd().
  if (!this.scrollTracker_.scrolling && !this.scroller_.animating)
    this.synchronizeFocus();
};

/**
 * @override
 */
camera.views.Album.prototype.onResize = function() {
  var selectedPicture = this.lastSelectedPicture();
  if (selectedPicture) {
    camera.util.ensureVisible(selectedPicture.element,
                              this.scroller_,
                              camera.util.SmoothScroller.Mode.INSTANT);
  }
};

/**
 * Handles clicking on the print button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Album.prototype.onPrintButtonClicked_ = function(event) {
  window.print();
};

/**
 * Handles clicking on the export button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Album.prototype.onExportButtonClicked_ = function(event) {
  this.exportSelection();
};

/**
 * Handles clicking on the delete button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Album.prototype.onDeleteButtonClicked_ = function(event) {
  this.deleteSelection();
};

/**
 * Updates visibility of the album buttons.
 * @private
 */
camera.views.Album.prototype.updateButtons_ = function() {
  if (this.pictures.length)
    document.querySelector('#album-print').removeAttribute('disabled');
  else
    document.querySelector('#album-print').setAttribute('disabled', '');

  if (this.selectedIndexes.length) {
    document.querySelector('#album-export').removeAttribute('disabled');
    document.querySelector('#album-delete').removeAttribute('disabled');
  } else {
    document.querySelector('#album-export').setAttribute('disabled', '');
    document.querySelector('#album-delete').setAttribute('disabled', '');
  }
};

/**
 * Opens the picture in the browser view.
 * @param {camera.models.Gallery.Picture} picture Picture to be opened.
 * @private
 */
camera.views.Album.prototype.openPictureInBrowser_ = function(picture) {
  this.router.navigate(
      camera.Router.ViewIdentifier.BROWSER, {
        picture: picture,
        selectionChanged: function(picture) {
          this.setSelectedIndex(picture ? this.pictureIndex(picture) : null);
      }.bind(this)}, function() {
        if (this.entered && !this.pictures.length)
          this.router.back();
      }.bind(this));
};

/**
 * Updates the view for the changed selection.
 * @private
 */
camera.views.Album.prototype.updateOnSelectionChanged_ = function(index) {
  var selectedPicture = this.lastSelectedPicture();
  if (selectedPicture) {
    camera.util.ensureVisible(selectedPicture.element, this.scroller_);
  }

  // Update visibility of the album buttons.
  this.updateButtons_();

  // Synchronize focus with the selected item immediately if not scrolling.
  // Otherwise, synchronization will be invoked from onScrollEnd().
  if (!this.scrollTracker_.scrolling && !this.scroller_.animating)
    this.synchronizeFocus();
};

/**
 * @override
 */
camera.views.Album.prototype.setSelectedIndex = function(index) {
  camera.views.GalleryBase.prototype.setSelectedIndex.apply(this, arguments);

  this.updateOnSelectionChanged_();
};

/**
 * @override
 */
camera.views.Album.prototype.onKeyPressed = function(event) {
  var selectedIndexes = this.selectedIndexes;

  var changeSelection = function(index) {
    // Expand selection if shift-key is pressed; otherwise, select one picture.
    if (index === null)
      return;
    if (event.shiftKey)
      this.expandSelectionTo_(index);
    else
      this.setSelectedIndex(index);
  }.bind(this);

  switch (camera.util.getShortcutIdentifier(event)) {
    case 'Shift-Down':
    case 'Down':
      if (this.pictures.length) {
        var leadIndex = this.lastSelectedIndex();
        if (leadIndex === null) {
          changeSelection(this.pictures.length - 1);
        } else {
          var newIndex = null;
          for (var i = leadIndex - 1; i >= 0; i--) {
            if (this.pictures[leadIndex].element.offsetLeft ==
                this.pictures[i].element.offsetLeft) {
              newIndex = i;
              break;
            }
          }
          // Find the possibly nearest one below the last selected picture.
          if (newIndex === null && this.pictures[0].element.offsetTop >
              this.pictures[leadIndex].element.offsetTop) {
            newIndex = 0;
          }
          changeSelection(newIndex);
        }
      }
      event.preventDefault();
      return;
    case 'Shift-Up':
    case 'Up':
      if (this.pictures.length) {
        var leadIndex = this.lastSelectedIndex();
        if (leadIndex === null) {
          changeSelection(0);
        } else {
          var newIndex = null;
          for (var i = leadIndex + 1; i < this.pictures.length; i++) {
            if (this.pictures[leadIndex].element.offsetLeft ==
                this.pictures[i].element.offsetLeft) {
              newIndex = i;
              break;
            }
          }
          changeSelection(newIndex);
        }
      }
      event.preventDefault();
      return;
    case 'Shift-Right':
    case 'Right':
    if (this.pictures.length) {
        var leadIndex = this.lastSelectedIndex();
        if (leadIndex === null) {
          changeSelection(this.pictures.length - 1);
        } else {
          var newIndex = (leadIndex > 0) ? leadIndex - 1 : null;
          changeSelection(newIndex);
        }
      }
      event.preventDefault();
      return;
    case 'Shift-Left':
    case 'Left':
      if (this.pictures.length) {
        var leadIndex = this.lastSelectedIndex();
        if (leadIndex === null) {
          changeSelection(0);
        } else {
          var newIndex = (leadIndex < this.pictures.length - 1) ?
              leadIndex + 1 : null;
          changeSelection(newIndex);
        }
      }
      event.preventDefault();
      return;
    case 'Shift-End':
    case 'End':
      if (this.pictures.length)
        changeSelection(0);
      event.preventDefault();
      return;
    case 'Shift-Home':
    case 'Home':
      if (this.pictures.length)
        changeSelection(this.pictures.length - 1);
      event.preventDefault();
      return;
    case 'Ctrl-A':  // Ctrl+A for selecting all.
      if (this.pictures.length) {
        this.setSelectedIndex(0);
        this.expandSelectionTo_(this.pictures.length - 1);
      }
      event.preventDefault();
      return;
    case 'Enter':
      // Do not handle enter, if other elements (such as buttons) are focused.
      if (document.activeElement != document.body &&
          !document.querySelector('#album .padder').contains(
              document.activeElement)) {
        return;
      }
      if (selectedIndexes.length == 1)
        this.openPictureInBrowser_(this.lastSelectedPicture().picture);
      event.preventDefault();
      return;
  }

  // Call the base view for unhandled keys.
  camera.views.GalleryBase.prototype.onKeyPressed.apply(this, arguments);
};

/**
 * Toggles the picture's selection.
 * @param {number} index Index of the picture to be toggled for its selection.
 * @private
 */
camera.views.Album.prototype.toggleSelection_ = function(index) {
  var removal = this.selectedIndexes.indexOf(index);
  if (removal != -1) {
    // Unselect the picture only if it's not the only selection.
    if (this.selectedIndexes.length > 1) {
      this.setPictureUnselected(index);
      this.selectedIndexes.splice(removal, 1);
      this.updateOnSelectionChanged_();
    }
  } else {
    // Select the picture if it's not selected yet.
    this.setPictureSelected(index);
    this.selectedIndexes.push(index);
    this.updateOnSelectionChanged_();
  }
};

/**
 * Expands the selection to a specified picture.
 * @param {number} index Index of the picture to be expanded to for selection.
 * @private
 */
camera.views.Album.prototype.expandSelectionTo_ = function(index) {
  var selectPicture = function(i) {
    // Remove the index from the selection if it was selected.
    var removal = this.selectedIndexes.indexOf(i);
    if (removal !== -1)
      this.selectedIndexes.splice(removal, 1);
    this.setPictureSelected(i);
    this.selectedIndexes.push(i);
  }.bind(this);

  var leadIndex = this.lastSelectedIndex();
  if (leadIndex !== null) {
    var min = Math.min.apply(null, this.selectedIndexes);
    var max = Math.max.apply(null, this.selectedIndexes);
    var continuous = this.selectedIndexes.length > 1;
    for (var i = min + 1; i < max; i++) {
      if (this.selectedIndexes.indexOf(i) == -1) {
        continuous = false;
        break;
      }
    }
    if (continuous) {
      // Expand the current selection from its min or max index till the
      // new index and make the new index the last selection.
      if (max < index) {
        for (var i = max + 1; i <= index; i++)
          selectPicture(i);
      } else if (min > index) {
        for (var i = min - 1; i >= index; i--)
          selectPicture(i);
      } else {
        // Deselect pictures and make the new selection if the new index is
        // between the current min and max index and the last selected index is
        // min or max index.
        if (leadIndex < index && leadIndex == min) {
          this.setSelectedIndex(max);
          for (var i = max - 1; i >= index; i--)
            selectPicture(i);
        } else if (leadIndex > index && leadIndex == max) {
          this.setSelectedIndex(min);
          for (var i = min + 1; i <= index; i++)
            selectPicture(i);
        } else if (leadIndex !== index) {
          // Simply update the last selected index.
          selectPicture(index);
        }
      }
    } else {
      // Expand the current selection from the last selected picture till the
      // new index and make the new index the last selection.
      if (leadIndex <= index) {
        for (var i = leadIndex + 1; i <= index; i++)
          selectPicture(i);
      } else {
        for (var i = leadIndex - 1; i >= index; i--)
          selectPicture(i);
      }
    }
  } else {
    selectPicture(index);
  }
  this.updateOnSelectionChanged_();
};

/**
 * Handles end of scrolling.
 * @private
 */
camera.views.Album.prototype.onScrollEnded_ = function() {
  this.synchronizeFocus();
};

/**
 * @override
 */
camera.views.Album.prototype.addPictureToDOM = function(picture) {
  var album = document.querySelector('#album .padder');
  var img = document.createElement('img');
  img.id = 'album-picture-' + (this.lastPictureIndex_++);
  img.tabIndex = -1;
  img.setAttribute('aria-role', 'option');
  img.setAttribute('aria-selected', 'false');
  album.insertBefore(img, album.firstChild);

  // Add to the collection.
  var domPicture = new camera.views.GalleryBase.DOMPicture(picture, img);
  this.pictures.push(domPicture);

  // Add handlers.
  img.addEventListener('mousedown', function(event) {
    event.preventDefault();  // Prevent focusing.
  });
  img.addEventListener('click', function(event) {
    var index = this.pictures.indexOf(domPicture);
    var key = camera.util.getShortcutIdentifier(event);
    if (key == 'Ctrl-') {
      this.toggleSelection_(index);
    } else if (key == 'Shift-') {
      this.expandSelectionTo_(index);
    } else {
      this.setSelectedIndex(index);
    }
  }.bind(this));
  img.addEventListener('contextmenu', function(event) {
    // Prevent the default context menu for ctrl-clicking.
    var index = this.pictures.indexOf(domPicture);
    if (camera.util.getShortcutIdentifier(event) == 'Ctrl-') {
      this.toggleSelection_(index);
      event.preventDefault();
    }
  }.bind(this));
  img.addEventListener('focus', function() {
    var index = this.pictures.indexOf(domPicture);
    if (this.selectedIndexes.indexOf(index) == -1)
      this.setSelectedIndex(this.pictures.indexOf(domPicture));
  }.bind(this));

  img.addEventListener('dblclick', function() {
    this.openPictureInBrowser_(picture);
  }.bind(this));
};

/**
 * @override
 */
camera.views.Album.prototype.ariaListNode = function() {
  return document.querySelector('#album');
};

