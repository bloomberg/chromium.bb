// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * UI component used for setting custom print margins.
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to read and
   *     write custom margin values.
   * @param {!print_preview.DocumentInfo} documentInfo Document data model.
   * @constructor
   * @extends {print_preview.Component}
   */
  function MarginControlContainer(printTicketStore, documentInfo) {
    print_preview.Component.call(this);

    /**
     * Used to read and write custom margin values.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Document data model.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;

    /**
     * Used to convert between the system's local units and points.
     * @type {!print_preview.MeasurementSystem}
     * @private
     */
    this.measurementSystem_ = printTicketStore.measurementSystem;

    /**
     * Convenience array that contains all of the margin controls.
     * @type {!Object.<
     *     !print_preview.ticket_items.CustomMargins.Orientation,
     *     !print_preview.MarginControl>}
     * @private
     */
    this.controls_ = {};
    for (var key in print_preview.ticket_items.CustomMargins.Orientation) {
      var orientation = print_preview.ticket_items.CustomMargins.Orientation[
          key];
      var control = new print_preview.MarginControl(orientation);
      this.controls_[orientation] = control;
      this.addChild(control);
    }

    /**
     * Margin control currently being dragged. Null if no control is being
     * dragged.
     * @type {print_preview.MarginControl}
     * @private
     */
    this.draggedControl_ = null;

    /**
     * Translation transformation in pixels to translate from the origin of the
     * custom margins component to the top-left corner of the most visible
     * preview page.
     * @type {!print_preview.Coordinate2d}
     * @private
     */
    this.translateTransform_ = new print_preview.Coordinate2d(0, 0);

    /**
     * Scaling transformation to scale from pixels to the units which the
     * print preview is in. The scaling factor is the same in both dimensions,
     * so this field is just a single number.
     * @type {number}
     * @private
     */
    this.scaleTransform_ = 1;

    /**
     * Clipping size for clipping the margin controls.
     * @type {print_preview.Size}
     * @private
     */
    this.clippingSize_ = null;
  };

  /**
   * CSS classes used by the custom margins component.
   * @enum {string}
   * @private
   */
  MarginControlContainer.Classes_ = {
    DRAGGING_HORIZONTAL: 'margin-control-container-dragging-horizontal',
    DRAGGING_VERTICAL: 'margin-control-container-dragging-vertical'
  };

  /**
   * @param {!print_preview.ticket_items.CustomMargins.Orientation} orientation
   *     Orientation value to test.
   * @return {boolean} Whether the given orientation is TOP or BOTTOM.
   * @private
   */
  MarginControlContainer.isTopOrBottom_ = function(orientation) {
    return orientation ==
        print_preview.ticket_items.CustomMargins.Orientation.TOP ||
        orientation ==
            print_preview.ticket_items.CustomMargins.Orientation.BOTTOM;
  };

  MarginControlContainer.prototype = {
    __proto__: print_preview.Component.prototype,

    /**
     * Updates the translation transformation that translates pixel values in
     * the space of the HTML DOM.
     * @param {print_preview.Coordinate2d} translateTransform Updated value of
     *     the translation transformation.
     */
    updateTranslationTransform: function(translateTransform) {
      if (!translateTransform.equals(this.translateTransform_)) {
        this.translateTransform_ = translateTransform;
        for (var orientation in this.controls_) {
          this.controls_[orientation].setTranslateTransform(translateTransform);
        }
      }
    },

    /**
     * Updates the scaling transform that scales pixels values to point values.
     * @param {number} scaleTransform Updated value of the scale transform.
     */
    updateScaleTransform: function(scaleTransform) {
      if (scaleTransform != this.scaleTransform_) {
        this.scaleTransform_ = scaleTransform;
        for (var orientation in this.controls_) {
          this.controls_[orientation].setScaleTransform(scaleTransform);
        }
      }
    },

    /**
     * Clips margin controls to the given clip size in pixels.
     * @param {print_preview.Size} Size to clip the margin controls to.
     */
    updateClippingMask: function(clipSize) {
      if (!clipSize) {
        return;
      }
      this.clippingSize_ = clipSize;
      for (var orientation in this.controls_) {
        var el = this.controls_[orientation].getElement();
        el.style.clip = 'rect(' +
            (-el.offsetTop) + 'px, ' +
            (clipSize.width - el.offsetLeft) + 'px, ' +
            (clipSize.height - el.offsetTop) + 'px, ' +
            (-el.offsetLeft) + 'px)';
      }
    },

    /** Shows the margin controls if the need to be shown. */
    showMarginControlsIfNeeded: function() {
      if (this.printTicketStore_.getMarginsType() ==
          print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        this.setIsMarginControlsVisible_(true);
      }
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      // We want to respond to mouse up events even beyond the component's
      // element.
      this.tracker.add(window, 'mouseup', this.onMouseUp_.bind(this));
      this.tracker.add(window, 'mousemove', this.onMouseMove_.bind(this));
      this.tracker.add(
          this.getElement(), 'mouseover', this.onMouseOver_.bind(this));
      this.tracker.add(
          this.getElement(), 'mouseout', this.onMouseOut_.bind(this));

      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.INITIALIZE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.TICKET_CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.documentInfo_,
          print_preview.DocumentInfo.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.CAPABILITIES_CHANGE,
          this.onTicketChange_.bind(this));

      for (var orientation in this.controls_) {
        this.tracker.add(
            this.controls_[orientation],
            print_preview.MarginControl.EventType.DRAG_START,
            this.onControlDragStart_.bind(this, this.controls_[orientation]));
        this.tracker.add(
            this.controls_[orientation],
            print_preview.MarginControl.EventType.TEXT_CHANGE,
            this.onControlTextChange_.bind(this, this.controls_[orientation]));
      }
    },

    /** @override */
    decorateInternal: function() {
      for (var orientation in this.controls_) {
        this.controls_[orientation].render(this.getElement());
      }
    },

    /**
     * @param {boolean} isVisible Whether the margin controls are visible.
     * @private
     */
    setIsMarginControlsVisible_: function(isVisible) {
      for (var orientation in this.controls_) {
        this.controls_[orientation].setIsVisible(isVisible);
      }
    },

    /**
     * Moves the position of the given control to the desired position in
     * pixels within some constraint minimum and maximum.
     * @param {!print_preview.MarginControl} control Control to move.
     * @param {!print_preview.Coordinate2d} posInPixels Desired position to move
     *     to in pixels.
     * @private
     */
    moveControlWithConstraints_: function(control, posInPixels) {
      var newPosInPts;
      if (MarginControlContainer.isTopOrBottom_(control.getOrientation())) {
        newPosInPts = control.convertPixelsToPts(posInPixels.y);
      } else {
        newPosInPts = control.convertPixelsToPts(posInPixels.x);
      }
      newPosInPts = Math.min(
          this.printTicketStore_.getCustomMarginMax(control.getOrientation()),
          newPosInPts);
      newPosInPts = Math.max(0, newPosInPts);
      newPosInPts = Math.round(newPosInPts);
      control.setPositionInPts(newPosInPts);
      control.setTextboxValue(this.serializeValueFromPts_(newPosInPts));
    },

    /**
     * @param {string} value Value to parse to points. E.g. '3.40"' or '200mm'.
     * @return {number} Value in points represented by the input value.
     * @private
     */
    parseValueToPts_: function(value) {
      // Removing whitespace anywhere in the string.
      value = value.replace(/\s*/g, '');
      if (value.length == 0) {
        return null;
      }
      var validationRegex = new RegExp('^(^-?)(\\d)+(\\' +
          this.measurementSystem_.thousandsDelimeter + '\\d{3})*(\\' +
          this.measurementSystem_.decimalDelimeter + '\\d*)?' +
          '(' + this.measurementSystem_.unitSymbol + ')?$');
      if (validationRegex.test(value)) {
        // Replacing decimal point with the dot symbol in order to use
        // parseFloat() properly.
        var replacementRegex =
            new RegExp('\\' + this.measurementSystem_.decimalDelimeter + '{1}');
        value = value.replace(replacementRegex, '.');
        return this.measurementSystem_.convertToPoints(parseFloat(value));
      }
      return null;
    },

    /**
     * @param {number} value Value in points to serialize.
     * @return {string} String representation of the value in the system's local
     *     units.
     * @private
     */
    serializeValueFromPts_: function(value) {
      value = this.measurementSystem_.convertFromPoints(value);
      value = this.measurementSystem_.roundValue(value);
      return value + this.measurementSystem_.unitSymbol;
    },

    /**
     * Called when a margin control starts to drag.
     * @param {print_preview.MarginControl} control The control which started to
     *     drag.
     * @private
     */
    onControlDragStart_: function(control) {
      this.draggedControl_ = control;
      this.getElement().classList.add(
          MarginControlContainer.isTopOrBottom_(control.getOrientation()) ?
              MarginControlContainer.Classes_.DRAGGING_VERTICAL :
              MarginControlContainer.Classes_.DRAGGING_HORIZONTAL);
    },

    /**
     * Called when the mouse moves in the custom margins component. Moves the
     * dragged margin control.
     * @param {MouseEvent} event Contains the position of the mouse.
     * @private
     */
    onMouseMove_: function(event) {
      if (this.draggedControl_) {
        this.moveControlWithConstraints_(
            this.draggedControl_,
            this.draggedControl_.translateMouseToPositionInPixels(
                new print_preview.Coordinate2d(event.x, event.y)));
        this.updateClippingMask(this.clippingSize_);
      }
    },

    /**
     * Called when the mouse is released in the custom margins component.
     * Releases the dragged margin control.
     * @param {MouseEvent} event Contains the position of the mouse.
     * @private
     */
    onMouseUp_: function(event) {
      if (this.draggedControl_) {
        this.getElement().classList.remove(
            MarginControlContainer.Classes_.DRAGGING_VERTICAL);
        this.getElement().classList.remove(
            MarginControlContainer.Classes_.DRAGGING_HORIZONTAL);
        if (event) {
          var posInPixels =
              this.draggedControl_.translateMouseToPositionInPixels(
                  new print_preview.Coordinate2d(event.x, event.y));
          this.moveControlWithConstraints_(this.draggedControl_, posInPixels);
        }
        this.updateClippingMask(this.clippingSize_);
        this.printTicketStore_.updateCustomMargin(
            this.draggedControl_.getOrientation(),
            this.draggedControl_.getPositionInPts());
        this.draggedControl_ = null;
      }
    },

    /**
     * Called when the mouse moves onto the component. Shows the margin
     * controls.
     * @private
     */
    onMouseOver_: function() {
      var fromElement = event.fromElement;
      while (fromElement != null) {
        if (fromElement == this.getElement()) {
          return;
        }
        fromElement = fromElement.parentElement;
      }
      if (this.printTicketStore_.hasMarginsCapability() &&
          this.printTicketStore_.getMarginsType() ==
              print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        this.setIsMarginControlsVisible_(true);
      }
    },

    /**
     * Called when the mouse moves off of the component. Hides the margin
     * controls.
     * @private
     */
    onMouseOut_: function(event) {
      var toElement = event.toElement;
      while (toElement != null) {
        if (toElement == this.getElement()) {
          return;
        }
        toElement = toElement.parentElement;
      }
      if (this.draggedControl_ != null) {
        return;
      }
      for (var orientation in this.controls_) {
        if (this.controls_[orientation].getIsFocused() ||
            this.controls_[orientation].getIsInError()) {
          return;
        }
      }
      this.setIsMarginControlsVisible_(false);
    },

    /**
     * Called when the print ticket changes. Updates the position of the margin
     * controls.
     * @private
     */
    onTicketChange_: function() {
      var margins = this.printTicketStore_.getCustomMargins();
      for (var orientation in this.controls_) {
        var control = this.controls_[orientation];
        control.setPageSize(this.documentInfo_.pageSize);
        control.setTextboxValue(
            this.serializeValueFromPts_(margins.get(orientation)));
        control.setPositionInPts(margins.get(orientation));
        control.setIsInError(false);
        control.setIsEnabled(true);
      }
      this.updateClippingMask(this.clippingSize_);
      if (this.printTicketStore_.getMarginsType() !=
          print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        this.setIsMarginControlsVisible_(false);
      }
    },

    /**
     * Called when the text in a textbox of a margin control changes or the
     * textbox loses focus.
     * Updates the print ticket store.
     * @param {!print_preview.MarginControl} control Updated control.
     * @private
     */
    onControlTextChange_: function(control) {
      var marginValue = this.parseValueToPts_(control.getTextboxValue());
      if (marginValue != null) {
        this.printTicketStore_.updateCustomMargin(
            control.getOrientation(), marginValue);
      } else {
        var enableOtherControls;
        if (!control.getIsFocused()) {
          // If control no longer in focus, revert to previous valid value.
          control.setTextboxValue(
              this.serializeValueFromPts_(control.getPositionInPts()));
          control.setIsInError(false);
          enableOtherControls = true;
        } else {
          control.setIsInError(true);
          enableOtherControls = false;
        }
        // Enable other controls.
        for (var o in this.controls_) {
          if (control.getOrientation() != o) {
            this.controls_[o].setIsEnabled(enableOtherControls);
          }
        }
      }
    }
  };

  // Export
  return {
    MarginControlContainer: MarginControlContainer
  };
});
