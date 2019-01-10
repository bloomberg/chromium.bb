// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */
const State = {
  LOADING: 'loading',
  ACTIVE: 'active',
  IDLE: 'idle'
};

/**
 * Hosts the Ink component which is responsible for both PDF rendering and
 * annotation when in annotation mode.
 */
Polymer({
  is: 'viewer-ink-host',

  /** @private {InkAPI} */
  ink_: null,

  /** @private {?string} */
  fileName_: null,

  /** @private {ArrayBuffer} */
  buffer_: null,

  /** @private {State} */
  state_: State.IDLE,

  /** @private {PointerEvent} */
  activePointer_: null,

  /**
   * Whether we should suppress pointer events due to a gesture,
   * eg. pinch-zoom.
   *
   * @private {boolean}
   */
  pointerGesture_: false,

  listeners: {
    pointerdown: 'onPointerDown_',
    pointerup: 'onPointerUpOrCancel_',
    pointermove: 'onPointerMove_',
    pointercancel: 'onPointerUpOrCancel_',
    pointerleave: 'onPointerLeave_',
  },

  /** @param {AnnotationTool} tool */
  setAnnotationTool(tool) {
    this.tool_ = tool;
    if (this.state_ == State.ACTIVE) {
      this.ink_.setAnnotationTool(tool);
    }
  },

  /** @param {PointerEvent} e */
  isActivePointer_: function(e) {
    return this.activePointer_ && this.activePointer_.pointerId == e.pointerId;
  },

  /**
   * Dispatches a pointer event to Ink.
   *
   * @param {PointerEvent} e
   */
  dispatchPointerEvent_: function(e) {
    // TODO(dstockwell) come up with a solution to propagate e.timeStamp.
    this.ink_.dispatchPointerEvent(e.type, {
      pointerId: e.pointerId,
      pointerType: e.pointerType,
      clientX: e.clientX,
      clientY: e.clientY,
      pressure: e.pressure,
      buttons: e.buttons,
    });
  },

  onPointerDown_: function(e) {
    if (e.pointerType == 'mouse' && e.buttons != 1 || this.pointerGesture_) {
      return;
    }

    if (this.activePointer_) {
      if (this.activePointer_.pointerType == 'touch' &&
          e.pointerType == 'touch') {
        // A multi-touch gesture has started with the active pointer. Cancel
        // the active pointer and suppress further events until it is released.
        this.pointerGesture_ = true;
        this.ink_.dispatchPointerEvent('pointercancel', {
          pointerId: this.activePointer_.pointerId,
          pointerType: this.activePointer_.pointerType,
        });
      }
      return;
    }

    this.activePointer_ = e;
    this.dispatchPointerEvent_(e);
  },

  /** @param {PointerEvent} e */
  onPointerLeave_: function(e) {
    if (e.pointerType != 'mouse' || !this.isActivePointer_(e)) {
      return;
    }
    this.onPointerUpOrCancel_(new PointerEvent('pointerup', e));
  },

  /** @param {PointerEvent} e */
  onPointerUpOrCancel_: function(e) {
    if (!this.isActivePointer_(e)) {
      return;
    }
    this.activePointer_ = null;
    if (!this.pointerGesture_) {
      this.dispatchPointerEvent_(e);
    }
    this.pointerGesture_ = false;
  },

  /** @param {PointerEvent} e */
  onPointerMove_: function(e) {
    if (!this.isActivePointer_(e) || this.pointerGesture_) {
      return;
    }

    let events = e.getCoalescedEvents();
    if (events.length == 0) {
      events = [e];
    }
    for (const event of events) {
      this.dispatchPointerEvent_(event);
    }
  },

  /**
   * Begins annotation mode with the document represented by `data`.
   * When the return value resolves the Ink component will be ready
   * to render immediately.
   *
   * @param {string} fileName The name of the PDF file.
   * @param {!ArrayBuffer} data The contents of the PDF document.
   * @param {!Viewport} viewport
   * @return {!Promise} void value.
   */
  load: async function(fileName, data, viewport) {
    this.fileName_ = fileName;
    this.state_ = State.LOADING;
    this.$.frame.src = 'ink/index.html';
    await new Promise(resolve => this.$.frame.onload = resolve);
    this.ink_ = await this.$.frame.contentWindow.initInk();
    this.ink_.setPDF(data);
    this.state_ = State.ACTIVE;
    this.viewportChanged(viewport);
    this.style.visibility = 'visible';
  },

  viewportChanged: function(viewport) {
    if (this.state_ != State.ACTIVE) {
      return;
    }
    const pos = viewport.position;
    const size = viewport.size;
    const zoom = viewport.zoom;
    const documentWidth = viewport.getDocumentDimensions(zoom).width * zoom;
    // Adjust for page shadows.
    const y = pos.y - Viewport.PAGE_SHADOW.top * zoom;
    let x = pos.x - Viewport.PAGE_SHADOW.left * zoom;
    // Center the document if the width is smaller than the viewport.
    if (documentWidth < size.width) {
      x += (documentWidth - size.width) / 2;
    }
    // Invert the Y-axis and convert Pixels to Points.
    const pixelsToPoints = 72 / 96;
    const scale = pixelsToPoints / zoom;
    const camera = {
      top: (-y) * scale,
      left: (x) * scale,
      right: (x + size.width) * scale,
      bottom: (-y - size.height) * scale,
    };
    this.ink_.setCamera(camera);
  },

  /**
   * @return {!Promise<{fileName: string, dataToSave: ArrayBuffer}>}
   *     The serialized PDF document including any annotations that were made.
   */
  saveDocument: async function() {
    if (this.state_ == State.ACTIVE) {
      this.buffer_ = await this.ink_.getPDFDestructive().buffer;
      this.state_ = State.IDLE;
    }
    return {
      fileName: this.fileName_,
      dataToSave: this.buffer_,
    };
  },
});
