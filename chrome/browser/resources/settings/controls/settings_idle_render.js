// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-idle-render is a simple variant of dom-if designed for lazy
 * rendering of elements that are accessed imperatively.
 * If a use for idle time expansion is found outside of settings, this code
 * should be replaced by cr-lazy-render after that feature is merged into
 * ui/webui/resources/cr_elements/cr_lazy_render/cr_lazy_render.js
 */

Polymer({
  is: 'settings-idle-render',
  extends: 'template',

  behaviors: [Polymer.Templatizer],

  /** @private {TemplatizerNode} */
  child_: null,

  /** @private {number} */
  idleCallback_: 0,

  /** @override */
  attached: function() {
    this.idleCallback_ = requestIdleCallback(this.render_.bind(this));
  },

  /** @override */
  detached: function() {
    // No-op if callback already fired.
    cancelIdleCallback(this.idleCallback_);
  },

  /**
   * Stamp the template into the DOM tree synchronously
   * @return {Element} Child element which has been stamped into the DOM tree.
   */
  get: function() {
    if (!this.child_)
      this.render_();
    return this.child_;
  },

  /** @private */
  render_: function() {
    if (!this.ctor)
      this.templatize(this);
    var parentNode = this.parentNode;
    if (parentNode && !this.child_) {
      var instance = this.stamp({});
      this.child_ = instance.root.firstElementChild;
      parentNode.insertBefore(instance.root, this);
    }
  },

  /**
   * @param {string} prop
   * @param {Object} value
   */
  _forwardParentProp: function(prop, value) {
    if (this.child_)
      this.child_._templateInstance[prop] = value;
  },

  /**
   * @param {string} path
   * @param {Object} value
   */
  _forwardParentPath: function(path, value) {
    if (this.child_)
      this.child_._templateInstance.notifyPath(path, value, true);
  }
});
