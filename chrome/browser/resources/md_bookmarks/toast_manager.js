// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which shows toasts.
 */
cr.define('bookmarks', function() {

  var ToastManager = Polymer({
    is: 'bookmarks-toast-manager',

    properties: {
      duration: {
        type: Number,
        value: 0,
      },

      /** @private */
      open_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** @private */
      showUndo_: Boolean,
    },

    /** @private {bookmarks.TimerProxy} */
    timerProxy_: new bookmarks.TimerProxy(),

    /** @private {number|null} */
    hideTimeoutId_: null,

    /** @override */
    attached: function() {
      assert(ToastManager.instance_ == null);
      ToastManager.instance_ = this;
    },

    /** @override */
    detached: function() {
      ToastManager.instance_ = null;
    },

    /**
     * @param {string} label The label to display inside the toast.
     * @param {boolean} showUndo Whether the undo button should be shown.
     */
    show: function(label, showUndo) {
      this.$.content.textContent = label;
      this.showInternal_(showUndo);
    },

    /**
     * Shows the toast, making certain text fragments collapsible.
     * @param {!Array<!{value: string, collapsible: boolean}>} pieces
     * @param {boolean} showUndo Whether the undo button should be shown.
     */
    showForStringPieces: function(pieces, showUndo) {
      var content = this.$.content;
      content.textContent = '';
      pieces.forEach(function(p) {
        if (p.value.length == 0)
          return;

        var span = document.createElement('span');
        span.textContent = p.value;
        if (p.collapsible)
          span.classList.add('collapsible');

        content.appendChild(span);
      });

      this.showInternal_(showUndo);
    },

    /**
     * @param {boolean} showUndo Whether the undo button should be shown.
     * @private
     */
    showInternal_: function(showUndo) {
      this.open_ = true;
      this.showUndo_ = showUndo;

      if (!this.duration)
        return;

      if (this.hideTimeoutId_ != null)
        this.timerProxy_.clearTimeout(this.hideTimeoutId_);

      this.hideTimeoutId_ = this.timerProxy_.setTimeout(function() {
        this.open_ = false;
        this.hideTimeoutId_ = null;
      }.bind(this), this.duration);
    },

    hide: function() {
      this.open_ = false;
    },

    /** @private */
    onUndoTap_: function() {
      // Will hide the toast.
      this.fire('command-undo');
    },
  });

  /** @private {?bookmarks.ToastManager} */
  ToastManager.instance_ = null;

  /** @return {!bookmarks.ToastManager} */
  ToastManager.getInstance = function() {
    return assert(ToastManager.instance_);
  };

  return {
    ToastManager: ToastManager,
  };
});
