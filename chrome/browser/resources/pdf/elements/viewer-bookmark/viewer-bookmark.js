// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The |title| is the text label displayed for the bookmark.
 *
 * The bookmark may point at a location in the PDF or a URI.
 * If it points at a location, |page| indicates which 0-based page it leads to.
 * Optionally, |y| is the y position in that page, in pixel coordinates.
 * If it points at an URI, |uri| is the target for that bookmark.
 *
 * |children| is an array of the |Bookmark|s that are below this in a table of
 * contents tree
 * structure.
 * @typedef {{
 *   title: string,
 *   page: number,
 *   y: number,
 *   uri: string,
 *   children: !Array<!Bookmark>
 * }}
 */
var Bookmark;

(function() {
/** Amount that each level of bookmarks is indented by (px). */
var BOOKMARK_INDENT = 20;

Polymer({
  is: 'viewer-bookmark',

  properties: {
    /** @type {Bookmark} */
    bookmark: {type: Object, observer: 'bookmarkChanged_'},

    depth: {type: Number, observer: 'depthChanged'},

    childDepth: Number,

    childrenShown: {type: Boolean, reflectToAttribute: true, value: false},

    keyEventTarget: {
      type: Object,
      value: function() {
        return this.$.item;
      }
    }
  },

  behaviors: [Polymer.IronA11yKeysBehavior],

  keyBindings: {'enter': 'onEnter_', 'space': 'onSpace_'},

  bookmarkChanged_: function() {
    this.$.expand.style.visibility =
        this.bookmark.children.length > 0 ? 'visible' : 'hidden';
  },

  depthChanged: function() {
    this.childDepth = this.depth + 1;
    this.$.item.style.webkitPaddingStart =
        (this.depth * BOOKMARK_INDENT) + 'px';
  },

  onClick: function() {
    if (this.bookmark.hasOwnProperty('page')) {
      if (this.bookmark.hasOwnProperty('y')) {
        this.fire('change-page-and-xy', {
          page: this.bookmark.page,
          x: 0,
          y: this.bookmark.y,
          origin: 'bookmark'
        });
      } else {
        this.fire(
            'change-page', {page: this.bookmark.page, origin: 'bookmark'});
      }
    } else if (this.bookmark.hasOwnProperty('uri')) {
      this.fire('navigate', {uri: this.bookmark.uri, newtab: true});
    }
  },

  onEnter_: function(e) {
    // Don't allow events which have propagated up from the expand button to
    // trigger a click.
    if (e.detail.keyboardEvent.target != this.$.expand)
      this.onClick();
  },

  onSpace_: function(e) {
    // paper-icon-button stops propagation of space events, so there's no need
    // to check the event source here.
    this.onClick();
    // Prevent default space scroll behavior.
    e.detail.keyboardEvent.preventDefault();
  },

  toggleChildren: function(e) {
    this.childrenShown = !this.childrenShown;
    e.stopPropagation();  // Prevent the above onClick handler from firing.
  }
});
})();
