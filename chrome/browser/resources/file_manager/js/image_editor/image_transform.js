// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Resize mode.
 */

ImageEditor.Mode.Resize = function() {
  ImageEditor.Mode.call(this, 'Resize');
};

ImageEditor.Mode.Resize.prototype = {__proto__: ImageEditor.Mode.prototype};

// TODO(dgozman): register Mode.Resize in v2.

ImageEditor.Mode.Resize.prototype.createTools = function(toolbar) {
  var canvas = this.getContent().getCanvas();
  this.widthRange_ =
      toolbar.addRange('width', 0, canvas.width, canvas.width * 2);
  this.heightRange_ =
      toolbar.addRange('height', 0, canvas.height, canvas.height * 2);
};

ImageEditor.Mode.Resize.prototype.commit = function() {
  ImageUtil.trace.resetTimer('transform');
  var newCanvas = this.getContent().copyCanvas(
      this.widthRange_.getValue(), this.heightRange_.getValue());
  ImageUtil.trace.reportTimer('transform');
  this.getContent().setCanvas(newCanvas);
  this.getViewport().fitImage();
};

/**
 * Rotate mode.
 */

ImageEditor.Mode.Rotate = function() {
  ImageEditor.Mode.call(this, 'Rotate');
};

ImageEditor.Mode.Rotate.prototype = {__proto__: ImageEditor.Mode.prototype};

ImageEditor.Mode.register(ImageEditor.Mode.Rotate);

ImageEditor.Mode.Rotate.prototype.commit = function() {};

ImageEditor.Mode.Rotate.prototype.rollback = function() {
  if (this.backup_) {
    this.getContent().setCanvas(this.backup_);
    this.backup_ = null;
  }
  this.transform_ = null;
};

ImageEditor.Mode.Rotate.prototype.createTools = function(toolbar) {
  toolbar.addButton("Left", this.modifyTransform.bind(this, 1, 1, 3));
  toolbar.addButton("Right", this.modifyTransform.bind(this, 1, 1, 1));
  toolbar.addButton("Flip V", this.modifyTransform.bind(this, 1, -1, 0));
  toolbar.addButton("Flip H", this.modifyTransform.bind(this, -1, 1, 0));

  var srcCanvas = this.getContent().getCanvas();

  var width = srcCanvas.width;
  var height = srcCanvas.height;
  var maxTg = Math.min(width / height, height / width);
  var maxTilt = Math.floor(Math.atan(maxTg) * 180 / Math.PI);
  this.tiltRange_ =
      toolbar.addRange('angle', -maxTilt, 0, maxTilt, 10);

  this.tiltRange_.
      addEventListener('mousedown', this.onTiltStart.bind(this), false);
  this.tiltRange_.
      addEventListener('mouseup', this.onTiltStop.bind(this), false);
};

ImageEditor.Mode.Rotate.prototype.getOriginal = function() {
  if (!this.backup_) {
    this.backup_ = this.getContent().getCanvas();
  }
  return this.backup_;
};

ImageEditor.Mode.Rotate.prototype.getTransform = function() {
  if (!this.transform_) {
    this.transform_ = new ImageEditor.Mode.Rotate.Transform();
  }
  return this.transform_;
};

ImageEditor.Mode.Rotate.prototype.onTiltStart = function() {
  this.tiltDrag_ = true;

  var original = this.getOriginal();

  // Downscale the original image to the overview thumbnail size.
  var downScale = ImageBuffer.Overview.MAX_SIZE /
      Math.max(original.width, original.height);

  this.preScaledOriginal_ = this.getContent().createBlankCanvas(
        original.width * downScale, original.height * downScale);
  Rect.drawImage(this.preScaledOriginal_.getContext('2d'), original);

  // Translate the current offset into the original image coordinate space.
  var viewport = this.getViewport();
  var originalOffset = this.getTransform().transformOffsetToBaseline(
      viewport.getOffsetX(), viewport.getOffsetY());

  // Find the part of the original image that is sufficient to pre-render
  // the rotation results.
  var screenClipped = viewport.getScreenClipped();
  var diagonal = viewport.screenToImageSize(
      Math.sqrt(screenClipped.width * screenClipped.width +
                screenClipped.height * screenClipped.height));

  var originalBounds = new Rect(original);

  var originalPreclipped = new Rect(
      originalBounds.width / 2 - originalOffset.x - diagonal / 2,
      originalBounds.height / 2 - originalOffset.y - diagonal / 2,
      diagonal,
      diagonal).clamp(originalBounds);

  // We assume that the scale is not changing during the mouse drag.
  var scale = viewport.getScale();
  this.preClippedOriginal_ = this.getContent().createBlankCanvas(
        originalPreclipped.width * scale, originalPreclipped.height * scale);

  Rect.drawImage(this.preClippedOriginal_.getContext('2d'), original, null,
      originalPreclipped);

  this.repaint();
};

ImageEditor.Mode.Rotate.prototype.onTiltStop = function() {
  this.tiltDrag_ = false;
  if (this.preScaledOriginal_) {
    this.preScaledOriginal_ = false;
    this.preClippedOriginal_ = false;
    this.applyTransform();
  } else {
    this.repaint();
  }
};

ImageEditor.Mode.Rotate.prototype.draw = function(context) {
  if (!this.tiltDrag_) return;

  var screenClipped = this.getViewport().getScreenClipped();

  if (this.preClippedOriginal_) {
    ImageUtil.trace.resetTimer('preview');
    var transformed = this.getContent().createBlankCanvas(
        screenClipped.width, screenClipped.height);
    this.getTransform().apply(
        transformed.getContext('2d'), this.preClippedOriginal_);
    Rect.drawImage(context, transformed, screenClipped);
    ImageUtil.trace.reportTimer('preview');
  }

  const STEP = 50;
  context.save();
  context.globalAlpha = 0.4;
  context.strokeStyle = "#C0C0C0";

  context.beginPath();
  var top = screenClipped.top + 0.5;
  var left = screenClipped.left + 0.5;
  for(var x = Math.ceil(screenClipped.left / STEP) * STEP;
          x < screenClipped.left + screenClipped.width;
          x += STEP) {
    context.moveTo(x + 0.5, top);
    context.lineTo(x + 0.5, top + screenClipped.height);
  }
  for(var y = Math.ceil(screenClipped.top / STEP) * STEP;
          y < screenClipped.top + screenClipped.height;
          y += STEP) {
    context.moveTo(left, y + 0.5);
    context.lineTo(left + screenClipped.width, y + 0.5);
  }
  context.closePath();
  context.stroke();

  context.restore();
};

ImageEditor.Mode.Rotate.prototype.modifyTransform =
    function(scaleX, scaleY, turn90) {

  var transform = this.getTransform();
  var viewport = this.getViewport();

  var baselineOffset = transform.transformOffsetToBaseline(
      viewport.getOffsetX(), viewport.getOffsetY());

  transform.modify(scaleX, scaleY, turn90, this.tiltRange_.getValue());

  var newOffset = transform.transformOffsetFromBaseline(
      baselineOffset.x, baselineOffset.y);

  // Ignoring offset clipping makes rotation behave more naturally.
  viewport.setOffset(newOffset.x, newOffset.y, true /*ignore clipping*/);

  if (scaleX * scaleY < 0) {
    this.tiltRange_.setValue(transform.tilt);
  }

  this.applyTransform();
};

ImageEditor.Mode.Rotate.prototype.applyTransform = function() {
  var srcCanvas = this.getOriginal();

  var newSize = this.transform_.getTiltedRectSize(
      srcCanvas.width, srcCanvas.height);

  var scale = 1;

  if (this.preScaledOriginal_) {
    scale = this.preScaledOriginal_.width / srcCanvas.width;
    srcCanvas = this.preScaledOriginal_;
  }

  var dstCanvas = this.getContent().createBlankCanvas(
      newSize.width * scale, newSize.height * scale);
  ImageUtil.trace.resetTimer('transform');
  this.transform_.apply(dstCanvas.getContext('2d'), srcCanvas);
  ImageUtil.trace.reportTimer('transform');
  this.getContent().setCanvas(dstCanvas, newSize.width, newSize.height);

  this.repaint();
};

ImageEditor.Mode.Rotate.prototype.update = function(values) {
  this.modifyTransform(1, 1, 0);
};

ImageEditor.Mode.Rotate.Transform = function() {
  this.scaleX = 1;
  this.scaleY = 1;
  this.turn90 = 0;
  this.tilt = 0;
};

ImageEditor.Mode.Rotate.Transform.prototype.modify =
    function(scaleX, scaleY, turn90, tilt) {
  this.scaleX *= scaleX;
  this.scaleY *= scaleY;
  this.turn90 += turn90;
  this.tilt = (scaleX * scaleY > 0) ? tilt : -tilt;
};

const DEG_IN_RADIAN = 180 / Math.PI;

ImageEditor.Mode.Rotate.Transform.prototype.getAngle = function() {
  return (this.turn90 * 90 + this.tilt) / DEG_IN_RADIAN;
};

ImageEditor.Mode.Rotate.Transform.prototype.transformOffsetFromBaseline =
    function(x, y) {
  var angle = this.getAngle();
  var sin = Math.sin(angle);
  var cos = Math.cos(angle);

  x *= this.scaleX;
  y *= this.scaleY;

  return {
    x: (x * cos - y * sin),
    y: (x * sin + y * cos)
  };
};

ImageEditor.Mode.Rotate.Transform.prototype.transformOffsetToBaseline =
    function(x, y) {
  var angle = -this.getAngle();
  var sin = Math.sin(angle);
  var cos = Math.cos(angle);

  return {
    x: (x * cos - y * sin) / this.scaleX,
    y: (x * sin + y * cos) / this.scaleY
  };
};

ImageEditor.Mode.Rotate.Transform.prototype.getTiltedRectSize =
    function(width, height) {
  if (this.turn90 & 1) {
    var temp = width;
    width = height;
    height = temp;
  }

  var angle = Math.abs(this.tilt) / DEG_IN_RADIAN;

  var sin = Math.sin(angle);
  var cos = Math.cos(angle);
  var denom = cos * cos - sin * sin;

  return {
    width: Math.floor((width * cos - height * sin) / denom),
    height: Math.floor((height * cos - width * sin) / denom)
  }
};

ImageEditor.Mode.Rotate.Transform.prototype.apply = function(
    context, srcCanvas) {
  context.save();
  context.translate(context.canvas.width / 2, context.canvas.height / 2);
  context.rotate(this.getAngle());
  context.scale(this.scaleX, this.scaleY);
  context.drawImage(srcCanvas, -srcCanvas.width / 2, -srcCanvas.height / 2);
  context.restore();
};

/**
 * Crop mode.
 */

ImageEditor.Mode.Crop = function() {
  ImageEditor.Mode.call(this, "Crop");
};

ImageEditor.Mode.Crop.prototype = {__proto__: ImageEditor.Mode.prototype};

ImageEditor.Mode.register(ImageEditor.Mode.Crop);

ImageEditor.Mode.Crop.prototype.createTools = function() {
  this.createDefaultCrop();
};

ImageEditor.Mode.Crop.GRAB_RADIUS = 5;

ImageEditor.Mode.Crop.prototype.commit = function() {
  var cropImageRect = this.cropRect_.getRect();

  var newCanvas = this.getContent().
      createBlankCanvas(cropImageRect.width, cropImageRect.height);

  var newContext = newCanvas.getContext("2d");
  ImageUtil.trace.resetTimer('transform');
  Rect.drawImage(newContext, this.getContent().getCanvas(),
      new Rect(newCanvas), cropImageRect);
  ImageUtil.trace.reportTimer('transform');

  this.getContent().setCanvas(newCanvas);
  this.getViewport().fitImage();
};

ImageEditor.Mode.Crop.prototype.rollback = function() {
  this.createDefaultCrop();
};

ImageEditor.Mode.Crop.prototype.createDefaultCrop = function() {
  var rect = new Rect(this.getViewport().getImageClipped());
  rect = rect.inflate (
      -Math.round(rect.width / 6), -Math.round(rect.height / 6));
  this.cropRect_ = new DraggableRect(
      rect, this.getViewport(), ImageEditor.Mode.Crop.GRAB_RADIUS);
};

ImageEditor.Mode.Crop.prototype.draw = function(context) {
  var R = ImageEditor.Mode.Crop.GRAB_RADIUS;

  var inner = this.getViewport().imageToScreenRect(this.cropRect_.getRect());
  var outer = this.getViewport().getScreenClipped();

  var inner_bottom = inner.top + inner.height;
  var inner_right = inner.left + inner.width;

  context.globalAlpha = 0.25;
  context.fillStyle = '#000000';
  Rect.fillBetween(context, inner, outer);

  context.fillStyle = '#FFFFFF';
  context.beginPath();
  context.moveTo(inner.left, inner.top);
  context.arc(inner.left, inner.top, R, 0, Math.PI * 2);
  context.moveTo(inner.left, inner_bottom);
  context.arc(inner.left, inner_bottom, R, 0, Math.PI * 2);
  context.moveTo(inner_right, inner.top);
  context.arc(inner_right, inner.top, R, 0, Math.PI * 2);
  context.moveTo(inner_right, inner_bottom);
  context.arc(inner_right, inner_bottom, R, 0, Math.PI * 2);
  context.closePath();
  context.fill();

  context.globalAlpha = 0.5;
  context.strokeStyle = '#FFFFFF';

  context.beginPath();
  context.closePath();
  for (var i = 0; i <= 3; i++) {
    var y = inner.top - 0.5 + Math.round((inner.height + 1) * i / 3);
    context.moveTo(inner.left, y);
    context.lineTo(inner.left + inner.width, y);

    var x = inner.left - 0.5 + Math.round((inner.width + 1) * i / 3);
    context.moveTo(x, inner.top);
    context.lineTo(x, inner.top + inner.height);
  }
  context.stroke();
};

ImageEditor.Mode.Crop.prototype.getCursorStyle = function(x, y, mouseDown) {
  return this.cropRect_.getCursorStyle(x, y, mouseDown);
};

ImageEditor.Mode.Crop.prototype.getDragHandler = function(x, y) {
  var cropDragHandler = this.cropRect_.getDragHandler(x, y);
  if (!cropDragHandler) return null;

  var self = this;
  return function(x, y) {
    cropDragHandler(x, y);
    self.repaint();
  };
};

/**
 * A draggable rectangle over the image.
 */

function DraggableRect(rect, viewport, sensitivity) {

  // The bounds are not held in a regular rectangle (with width/height).
  // left/top/right/bottom held instead for convenience.
  this.bounds_ = {};
  this.bounds_[DraggableRect.LEFT] = rect.left;
  this.bounds_[DraggableRect.RIGHT] = rect.left + rect.width;
  this.bounds_[DraggableRect.TOP] = rect.top;
  this.bounds_[DraggableRect.BOTTOM] = rect.top + rect.height;

  this.viewport_ = viewport;
  this.sensitivity_ = sensitivity;

  this.oppositeSide_ = {};
  this.oppositeSide_[DraggableRect.LEFT] = DraggableRect.RIGHT;
  this.oppositeSide_[DraggableRect.RIGHT] = DraggableRect.LEFT;
  this.oppositeSide_[DraggableRect.TOP] = DraggableRect.BOTTOM;
  this.oppositeSide_[DraggableRect.BOTTOM] = DraggableRect.TOP;

  // Translation table to form CSS-compatible cursor style.
  this.cssSide_ = {};
  this.cssSide_[DraggableRect.LEFT] = 'w';
  this.cssSide_[DraggableRect.TOP] = 'n';
  this.cssSide_[DraggableRect.RIGHT] = 'e';
  this.cssSide_[DraggableRect.BOTTOM] = 's';
  this.cssSide_[DraggableRect.NONE] = '';
}

// Static members to simplify reflective access to the bounds.
DraggableRect.LEFT = 'left';
DraggableRect.RIGHT = 'right';
DraggableRect.TOP = 'top';
DraggableRect.BOTTOM = 'bottom';
DraggableRect.NONE = 'none';

DraggableRect.prototype.getRect = function() { return new Rect(this.bounds_) };

DraggableRect.prototype.getDragMode = function(x, y) {
  var result = {
    xSide: DraggableRect.NONE,
    ySide: DraggableRect.NONE
  };

  var bounds = this.bounds_;
  var R = this.viewport_.screenToImageSize(this.sensitivity_);

  var circle = new Circle(x, y, R);

  var xBetween = ImageUtil.between(bounds.left, x, bounds.right);
  var yBetween = ImageUtil.between(bounds.top, y, bounds.bottom);

  if (circle.inside(bounds.left, bounds.top)) {
    result.xSide = DraggableRect.LEFT;
    result.ySide = DraggableRect.TOP;
  } else if (circle.inside(bounds.left, bounds.bottom)) {
    result.xSide = DraggableRect.LEFT;
    result.ySide = DraggableRect.BOTTOM;
  } else if (circle.inside(bounds.right, bounds.top)) {
    result.xSide = DraggableRect.RIGHT;
    result.ySide = DraggableRect.TOP;
  } else if (circle.inside(bounds.right, bounds.bottom)) {
    result.xSide = DraggableRect.RIGHT;
    result.ySide = DraggableRect.BOTTOM;
  } else if (yBetween && Math.abs(x - bounds.left) <= R) {
    result.xSide = DraggableRect.LEFT;
  } else if (yBetween && Math.abs(x - bounds.right) <= R) {
    result.xSide = DraggableRect.RIGHT;
  } else if (xBetween && Math.abs(y - bounds.top) <= R) {
    result.ySide = DraggableRect.TOP;
  } else if (xBetween && Math.abs(y - bounds.bottom) <= R) {
    result.ySide = DraggableRect.BOTTOM;
  } else if (xBetween && yBetween) {
    result.whole = true;
  } else {
    result.outside = true;
  }

  return result;
};

DraggableRect.prototype.getCursorStyle = function(x, y, mouseDown) {
  var mode;
  if (mouseDown) {
    mode = this.dragMode_;
  } else {
    mode = this.getDragMode(
        this.viewport_.screenToImageX(x), this.viewport_.screenToImageY(y));
  }
  if (mode.whole) return 'hand';
  if (mode.outside) return 'crosshair';
  return this.cssSide_[mode.ySide] + this.cssSide_[mode.xSide] + '-resize';
};

DraggableRect.prototype.getDragHandler = function(x, y) {
  x = this.viewport_.screenToImageX(x);
  y = this.viewport_.screenToImageY(y);

  var clipRect = this.viewport_.getImageClipped();
  if (!clipRect.inside(x, y)) return null;

  this.dragMode_ = this.getDragMode(x, y);

  var self = this;

  var mouseBiasX;
  var mouseBiasY;

  var fixedWidth = 0;
  var fixedHeight = 0;

  var resizeFuncX;
  var resizeFuncY;

  if (this.dragMode_.whole) {
    mouseBiasX = this.bounds_.left - x;
    fixedWidth = this.bounds_.right - this.bounds_.left;
    resizeFuncX = function(x) {
      self.bounds_.left = x;
      self.bounds_.right = self.bounds_.left + fixedWidth;
    };
    mouseBiasY = this.bounds_.top - y;
    fixedHeight = this.bounds_.bottom - this.bounds_.top;
    resizeFuncY = function(y) {
      self.bounds_.top = y;
      self.bounds_.bottom = self.bounds_.top + fixedHeight;
    };
  } else {
    if (this.dragMode_.outside) {
      this.dragMode_.xSide = DraggableRect.RIGHT;
      this.dragMode_.ySide = DraggableRect.BOTTOM;
      this.bounds_.left = this.bounds_.right = x;
      this.bounds_.top = this.bounds_.bottom = y;
    }

    function flipSide(side) {
      var opposite = self.oppositeSide_[side];
      var temp = self.bounds_[side];
      self.bounds_[side] = self.bounds_[opposite];
      self.bounds_[opposite] = temp;
      return opposite;
    }

    if (this.dragMode_.xSide != DraggableRect.NONE) {
      mouseBiasX = self.bounds_[this.dragMode_.xSide] - x;
      resizeFuncX = function(x) {
        self.bounds_[self.dragMode_.xSide] = x;
        if (self.bounds_.left > self.bounds_.right) {
          self.dragMode_.xSide = flipSide(self.dragMode_.xSide);
        }
      }
    }
    if (this.dragMode_.ySide != DraggableRect.NONE) {
      mouseBiasY = self.bounds_[this.dragMode_.ySide] - y;
      resizeFuncY = function(y) {
        self.bounds_[self.dragMode_.ySide] = y;
        if (self.bounds_.top > self.bounds_.bottom) {
          self.dragMode_.ySide = flipSide(self.dragMode_.ySide);
        }
      }
    }
  }

  function convertX(x) {
    return ImageUtil.clamp(
        clipRect.left,
        self.viewport_.screenToImageX(x) + mouseBiasX,
        clipRect.left + clipRect.width - fixedWidth);
  }

  function convertY(y) {
    return ImageUtil.clamp(
        clipRect.top,
        self.viewport_.screenToImageY(y) + mouseBiasY,
        clipRect.top + clipRect.height - fixedHeight);
  }

  return function(x, y) {
    if (resizeFuncX) resizeFuncX(convertX(x));
    if (resizeFuncY) resizeFuncY(convertY(y));
  };
};
