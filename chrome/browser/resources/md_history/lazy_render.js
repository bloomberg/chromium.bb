// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * history-lazy-render is a simple variant of dom-if designed for lazy rendering
 * of elements that are accessed imperatively.
 * Usage:
 *   <template is="history-lazy-render" id="menu">
 *     <heavy-menu></heavy-menu>
 *   </template>
 *
 *   this.$.menu.get().then(function(menu) {
 *     menu.show();
 *   });
 */

Polymer({
  is: 'history-lazy-render',
  extends: 'template',

  behaviors: [
    Polymer.Templatizer
  ],

  /** @private {Promise<Element>} */
  _renderPromise: null,

  /** @private {TemplateInstance} */
  _instance: null,

  /**
   * Stamp the template into the DOM tree asynchronously
   * @return {Promise<Element>} Promise which resolves when the template has
   *   been stamped.
   */
  get: function() {
    if (!this._renderPromise) {
      this._renderPromise = new Promise(function(resolve) {
        this._debounceTemplate(function() {
          this._render();
          this._renderPromise = null;
          resolve(this.getIfExists());
        }.bind(this));
      }.bind(this));
    }
    return this._renderPromise;
  },

  /**
   * @return {?Element} The element contained in the template, if it has
   *   already been stamped.
   */
  getIfExists: function() {
    if (this._instance) {
      var children = this._instance._children;

      for (var i = 0; i < children.length; i++) {
        if (children[i].nodeType == Node.ELEMENT_NODE)
          return children[i];
      }
    }
    return null;
  },

  _render: function() {
    if (!this.ctor)
      this.templatize(this);
    var parentNode = this.parentNode;
    if (parentNode && !this._instance) {
      this._instance = /** @type {TemplateInstance} */(this.stamp({}));
      var root = this._instance.root;
      parentNode.insertBefore(root, this);
    }
  },

  /**
   * @param {string} prop
   * @param {Object} value
   */
  _forwardParentProp: function(prop, value) {
    if (this._instance)
      this._instance.__setProperty(prop, value, true);
  },

  /**
   * @param {string} path
   * @param {Object} value
   */
  _forwardParentPath: function(path, value) {
    if (this._instance)
      this._instance._notifyPath(path, value, true);
  }
});
