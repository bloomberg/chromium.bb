// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * @param {boolean} dragEnabled
   * @param {!EventTarget} target
   * @constructor
   * @implements cr.ui.DragWrapperDelegate
   */
  function DragAndDropHandler(dragEnabled, target) {
    this.dragEnabled = dragEnabled;
    /** @private {!EventTarget} */
    this.eventTarget_ = target;
  }

  // TODO(devlin): Un-chrome.send-ify this implementation.
  DragAndDropHandler.prototype = {
    /** @type {boolean} */
    dragEnabled: false,

    /** @override */
    shouldAcceptDrag: function(e) {
      // External Extension installation can be disabled globally, e.g. while a
      // different overlay is already showing.
      if (!this.dragEnabled)
        return false;

      // We can't access filenames during the 'dragenter' event, so we have to
      // wait until 'drop' to decide whether to do something with the file or
      // not.
      // See: http://www.w3.org/TR/2011/WD-html5-20110113/dnd.html#concept-dnd-p
      return !!e.dataTransfer.types &&
          e.dataTransfer.types.indexOf('Files') > -1;
    },

    /** @override */
    doDragEnter: function() {
      chrome.send('startDrag');
      this.eventTarget_.dispatchEvent(
          new CustomEvent('extension-drag-started'));
    },

    /** @override */
    doDragLeave: function() {
      this.fireDragEnded_();
      chrome.send('stopDrag');
    },

    /** @override */
    doDragOver: function(e) {
      e.preventDefault();
    },

    /** @override */
    doDrop: function(e) {
      this.fireDragEnded_();
      if (e.dataTransfer.files.length != 1)
        return;

      let toSend = '';
      // Files lack a check if they're a directory, but we can find out through
      // its item entry.
      for (let i = 0; i < e.dataTransfer.items.length; ++i) {
        if (e.dataTransfer.items[i].kind == 'file' &&
            e.dataTransfer.items[i].webkitGetAsEntry().isDirectory) {
          toSend = 'installDroppedDirectory';
          break;
        }
      }
      // Only process files that look like extensions. Other files should
      // navigate the browser normally.
      if (!toSend &&
          /\.(crx|user\.js|zip)$/i.test(e.dataTransfer.files[0].name)) {
        toSend = 'installDroppedFile';
      }

      if (toSend) {
        e.preventDefault();
        chrome.send(toSend);
      }
    },

    /** @private */
    fireDragEnded_: function() {
      this.eventTarget_.dispatchEvent(new CustomEvent('extension-drag-ended'));
    }
  };

  return {
    DragAndDropHandler: DragAndDropHandler,
  };
});
