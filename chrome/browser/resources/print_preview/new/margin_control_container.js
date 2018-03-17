// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-margin-control-container',

  behaviors: [SettingsBehavior],

  properties: {
    /** @type {!print_preview.Size} */
    pageSize: {
      type: Object,
      notify: true,
    },

    previewLoaded: Boolean,

    /** @type {?print_preview.MeasurementSystem} */
    measurementSystem: Object,

    /** @private {number} */
    scaleTransform_: {
      type: Number,
      notify: true,
      value: 0,
    },

    /** @private {!print_preview.Coordinate2d} */
    translateTransform_: {
      type: Object,
      notify: true,
      value: new print_preview.Coordinate2d(0, 0),
    },

    /** @private {?print_preview.Size} */
    clipSize_: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {boolean} */
    available_: {
      type: Boolean,
      notify: true,
      computed: 'computeAvailable_(previewLoaded, settings.margins.value)',
      observer: 'onAvailableChange_',
    },

    /**
     * @private {!Array<!print_preview.ticket_items.CustomMarginsOrientation>}
     */
    marginSides_: {
      type: Array,
      notify: true,
      value: [
        print_preview.ticket_items.CustomMarginsOrientation.TOP,
        print_preview.ticket_items.CustomMarginsOrientation.RIGHT,
        print_preview.ticket_items.CustomMarginsOrientation.BOTTOM,
        print_preview.ticket_items.CustomMarginsOrientation.LEFT,
      ],
    },

    /**
     * String attribute used to set cursor appearance. Possible values:
     * empty (''): No margin control is currently being dragged.
     * 'dragging-horizontal': The left or right control is being dragged.
     * 'dragging-vertical': The top or bottom control is being dragged.
     * @private {string}
     */
    dragging_: {
      type: String,
      reflectToAttribute: true,
      value: '',
    },

    /** @private {boolean} */
    visible: {
      type: Boolean,
      notify: true,
      reflectToAttribute: true,
      value: true,
    },
  },

  /** @private {!print_preview.Coordinate2d} */
  pointerStartPositionInPixels_: new print_preview.Coordinate2d(0, 0),

  /** @private {?print_preview.Coordinate2d} */
  marginStartPositionInPixels_: null,

  /**
   * @return {boolean}
   * @private
   */
  computeAvailable_: function() {
    return this.previewLoaded && !!this.clipSize_ &&
        this.getSetting('margins').value ==
        print_preview.ticket_items.MarginsTypeValue.CUSTOM &&
        !!this.pageSize;
  },

  /** @private */
  onAvailableChange_: function() {
    this.visible = this.available_;
  },

  /**
   * @return {boolean} Whether the margin controls should be invisible.
   * @private
   */
  isInvisible_: function() {
    return !this.available_ || !this.visible;
  },

  /**
   * @param {!print_preview.ticket_items.CustomMarginsOrientation} orientation
   *     Orientation value to test.
   * @return {boolean} Whether the given orientation is TOP or BOTTOM.
   * @private
   */
  isTopOrBottom_: function(orientation) {
    return orientation ==
        print_preview.ticket_items.CustomMarginsOrientation.TOP ||
        orientation ==
        print_preview.ticket_items.CustomMarginsOrientation.BOTTOM;
  },

  /**
   * Moves the position of the given control to the desired position in
   * pixels within some constraint minimum and maximum.
   * @param {!HTMLElement} control Control to move.
   * @param {!print_preview.Coordinate2d} posInPixels Desired position to move
   *     to in pixels.
   * @private
   */
  moveControlWithConstraints_: function(control, posInPixels) {
    let newPosInPts;
    if (this.isTopOrBottom_(
            /** @type {print_preview.ticket_items.CustomMarginsOrientation} */ (
                control.side))) {
      newPosInPts = control.convertPixelsToPts(posInPixels.y);
    } else {
      newPosInPts = control.convertPixelsToPts(posInPixels.x);
    }
    newPosInPts = Math.max(0, newPosInPts);
    newPosInPts = Math.round(newPosInPts);
    control.setPositionInPts(newPosInPts);
    control.setTextboxValue(this.serializeValueFromPts_(newPosInPts));
  },

  /**
   * @param {number} value Value in points to serialize.
   * @return {string} String representation of the value in the system's local
   *     units.
   * @private
   */
  serializeValueFromPts_: function(value) {
    assert(this.measurementSystem);
    value = this.measurementSystem.convertFromPoints(value);
    value = this.measurementSystem.roundValue(value);
    return value + this.measurementSystem.unitSymbol;
  },

  /**
   * Translates the position of the margin control relative to the pointer
   * position in pixels.
   * @param {!print_preview.Coordinate2d} pointerPosition New position of
   *     the pointer.
   * @return {!print_preview.Coordinate2d} New position of the margin control.
   */
  translatePointerToPositionInPixels: function(pointerPosition) {
    return new print_preview.Coordinate2d(
        pointerPosition.x - this.pointerStartPositionInPixels_.x +
            this.marginStartPositionInPixels_.x,
        pointerPosition.y - this.pointerStartPositionInPixels_.y +
            this.marginStartPositionInPixels_.y);
  },

  /**
   * Called when the pointer moves in the custom margins component. Moves the
   * dragged margin control.
   * @param {!PointerEvent} event Contains the position of the pointer.
   * @private
   */
  onPointerMove_: function(event) {
    const control =
        /** @type {!PrintPreviewMarginControlElement} */ (event.target);
    this.moveControlWithConstraints_(
        control,
        this.translatePointerToPositionInPixels(
            new print_preview.Coordinate2d(event.x, event.y)));
    this.updateClippingMask(this.clipSize_);
  },

  /**
   * Called when the pointer is released in the custom margins component.
   * Releases the dragged margin control.
   * @param {!PointerEvent} event Contains the position of the pointer.
   * @private
   */
  onPointerUp_: function(event) {
    const control =
        /** @type {!PrintPreviewMarginControlElement} */ (event.target);
    this.dragging_ = '';
    const posInPixels = this.translatePointerToPositionInPixels(
        new print_preview.Coordinate2d(event.x, event.y));
    this.moveControlWithConstraints_(control, posInPixels);
    this.updateClippingMask(this.clipSize_);
    this.unlisten(control, 'pointercancel', 'onPointerUp_');
    this.unlisten(control, 'pointerup', 'onPointerUp_');
    this.unlisten(control, 'pointermove', 'onPointerMove_');

    this.fire('margin-drag-changed', false);
  },

  /** @return {boolean} Whether the user is dragging a margin control. */
  isDragging: function() {
    return this.dragging_ != '';
  },

  /**
   * @param {!CustomEvent} e Event containing the new textbox value.
   * @private
   */
  onTextChange_: function(e) {
    const marginValue = this.parseValueToPts_(/** @type {string} */ (e.detail));
    e.target.setPositionInPts(marginValue);
  },

  /**
   * @param {!PointerEvent} e Fired when pointerdown occurs on a margin control.
   * @private
   */
  onPointerDown_: function(e) {
    const control =
        /** @type {!PrintPreviewMarginControlElement} */ (e.target);
    if (!control.shouldDrag(e))
      return;

    this.pointerStartPositionInPixels_ =
        new print_preview.Coordinate2d(e.x, e.y);
    this.marginStartPositionInPixels_ =
        new print_preview.Coordinate2d(control.offsetLeft, control.offsetTop);
    this.dragging_ =
        this.isTopOrBottom_(
            /** @type {print_preview.ticket_items.CustomMarginsOrientation} */ (
                control.side)) ?
        'dragging-vertical' :
        'dragging-horizontal';
    this.listen(control, 'pointercancel', 'onPointerUp_');
    this.listen(control, 'pointerup', 'onPointerUp_');
    this.listen(control, 'pointermove', 'onPointerMove_');
    control.setPointerCapture(e.pointerId);

    this.fire('margin-drag-changed', true);
  },

  /**
   * @param {string} value Value to parse to points. E.g. '3.40"' or '200mm'.
   * @return {?number} Value in points represented by the input value.
   * @private
   */
  parseValueToPts_: function(value) {
    // Removing whitespace anywhere in the string.
    value = value.replace(/\s*/g, '');
    if (value.length == 0) {
      return null;
    }
    assert(this.measurementSystem);
    const validationRegex = new RegExp(
        '^(^-?)(\\d)+(\\' + this.measurementSystem.thousandsDelimeter +
        '\\d{3})*(\\' + this.measurementSystem.decimalDelimeter + '\\d*)?' +
        '(' + this.measurementSystem.unitSymbol + ')?$');
    if (validationRegex.test(value)) {
      // Replacing decimal point with the dot symbol in order to use
      // parseFloat() properly.
      const replacementRegex =
          new RegExp('\\' + this.measurementSystem.decimalDelimeter + '{1}');
      value = value.replace(replacementRegex, '.');
      return this.measurementSystem.convertToPoints(parseFloat(value));
    }
    return null;
  },

  /**
   * Updates the translation transformation that translates pixel values in
   * the space of the HTML DOM.
   * @param {print_preview.Coordinate2d} translateTransform Updated value of
   *     the translation transformation.
   */
  updateTranslationTransform: function(translateTransform) {
    if (!translateTransform.equals(this.translateTransform_)) {
      this.translateTransform_ = translateTransform;
    }
  },

  /**
   * Updates the scaling transform that scales pixels values to point values.
   * @param {number} scaleTransform Updated value of the scale transform.
   */
  updateScaleTransform: function(scaleTransform) {
    if (scaleTransform != this.scaleTransform_)
      this.scaleTransform_ = scaleTransform;
  },

  /**
   * Clips margin controls to the given clip size in pixels.
   * @param {print_preview.Size} clipSize Size to clip the margin controls to.
   */
  updateClippingMask: function(clipSize) {
    if (!clipSize) {
      return;
    }
    this.clipSize_ = clipSize;
    this.notifyPath('clipSize_');
  },
});
