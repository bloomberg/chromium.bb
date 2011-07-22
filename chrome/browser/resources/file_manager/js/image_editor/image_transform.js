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

ImageEditor.Mode.register(ImageEditor.Mode.Resize);

ImageEditor.Mode.Resize.prototype.createTools = function(toolbar) {
  var canvas = this.getBuffer().getImageCanvas();
  this.widthRange_ =
      toolbar.addRange('width', 0, canvas.width, canvas.width * 2);
  this.heightRange_ =
      toolbar.addRange('height', 0, canvas.height, canvas.height * 2);
};

ImageEditor.Mode.Resize.prototype.commit = function() {
  var newCanvas = this.getBuffer().createBlankCanvas(
      this.widthRange_.getValue(), this.heightRange_.getValue());

  var srcCanvas = this.getBuffer().getImageCanvas();

  var context = newCanvas.getContext("2d");
  ImageUtil.trace.resetTimer('transform');
  Rect.drawImage(
      context, srcCanvas, new Rect(newCanvas), new Rect(srcCanvas));
  ImageUtil.trace.reportTimer('transform');

  this.getBuffer().setImageCanvas(newCanvas);
  this.getBuffer().fitImage();
};

/**
 * Rotate mode.
 */

ImageEditor.Mode.Rotate = function() {
  ImageEditor.Mode.call(this, 'Rotate');
};

ImageEditor.Mode.Rotate.prototype = {__proto__: ImageEditor.Mode.prototype};

ImageEditor.Mode.register(ImageEditor.Mode.Rotate);

ImageEditor.Mode.Rotate.prototype.commit = function() {
  this.getBuffer().fitImage();
};

ImageEditor.Mode.Rotate.prototype.rollback = function() {
  if (this.backup_) {
    this.getBuffer().setImageCanvas(this.backup_);
    this.backup_ = null;
    this.transform_ = null;
  }
};

ImageEditor.Mode.Rotate.prototype.createTools = function(toolbar) {
  toolbar.addButton("Left", this.doTransform.bind(this, 1, 1, 3));
  toolbar.addButton("Right", this.doTransform.bind(this, 1, 1, 1));
  toolbar.addButton("Flip V", this.doTransform.bind(this, 1, -1, 0));
  toolbar.addButton("Flip H", this.doTransform.bind(this, -1, 1, 0));

  var srcCanvas = this.getBuffer().getImageCanvas();

  var width = srcCanvas.width;
  var height = srcCanvas.height;
  var maxTg = Math.min(width / height, height / width);
  var maxTilt = Math.floor(Math.atan(maxTg) * 180 / Math.PI);
  this.tiltRange_ =
      toolbar.addRange('angle', -maxTilt, 0, maxTilt, 10);

  this.tiltRange_.addEventListener('mousedown', this.onTiltStart.bind(this));
  this.tiltRange_.addEventListener('mouseup', this.onTiltStop.bind(this));
};

ImageEditor.Mode.Rotate.prototype.onTiltStart = function() {
  this.tiltDrag_ = true;
  this.repaint();
};

ImageEditor.Mode.Rotate.prototype.onTiltStop = function() {
  this.tiltDrag_ = false;
  this.repaint();
};

ImageEditor.Mode.Rotate.prototype.draw = function(context) {
  if (!this.tiltDrag_) return;

  var rect = this.getBuffer().getClippedScreen();
  const STEP = 50;
  context.save();
  context.globalAlpha = 0.4;
  context.strokeStyle = "#C0C0C0";
  for(var x = 0; x <= rect.width - STEP; x += STEP) {
    context.strokeRect(rect.left + x, rect.top, STEP, rect.height);
  }
  for(var y = 0; y <= rect.height - STEP; y += STEP) {
    context.strokeRect(rect.left, rect.top + y, rect.width, STEP);
  }
  context.restore();
};

ImageEditor.Mode.Rotate.prototype.doTransform =
    function(scaleX, scaleY, turn90) {
  if (!this.backup_) {
    this.backup_ = this.getBuffer().getImageCanvas();
    this.transform_ = new ImageEditor.Mode.Rotate.Transform();
  }

  var baselineOffset = this.transform_.transformOffsetToBaseline(
      this.getBuffer().getOffsetX(), this.getBuffer().getOffsetY());

  this.transform_.modify(scaleX, scaleY, turn90, this.tiltRange_.getValue());

  if (scaleX * scaleY < 0) {
    this.tiltRange_.setValue(this.transform_.tilt);
  }

  var srcCanvas = this.backup_;

  var newSize = this.transform_.getTiltedRectSize(
      srcCanvas.width, srcCanvas.height);

  var dstCanvas =
      this.getBuffer().createBlankCanvas(newSize.width, newSize.height);

  var context = dstCanvas.getContext("2d");
  context.save();
  context.translate(dstCanvas.width / 2, dstCanvas.height / 2);
  context.rotate(this.transform_.getAngle());
  context.scale(this.transform_.scaleX, this.transform_.scaleY);
  ImageUtil.trace.resetTimer('transform');
  context.drawImage(srcCanvas, -srcCanvas.width / 2, -srcCanvas.height / 2);
  ImageUtil.trace.reportTimer('transform');
  context.restore();

  var newOffset = this.transform_.transformOffsetFromBaseline(
      baselineOffset.x, baselineOffset.y);

  // Ignoring offset clipping make rotation behave more natural.
  this.getBuffer().setOffset(
      newOffset.x, newOffset.y, true /*ignore clipping*/);
  this.getBuffer().setImageCanvas(dstCanvas);
  this.getBuffer().repaint();
};

ImageEditor.Mode.Rotate.prototype.update = function(values) {
  this.doTransform(1, 1, 0);
};

ImageEditor.Mode.Rotate.Transform = function() {
  this.scaleX = 1;
  this.scaleY = 1;
  this.turn90 = 0;
  this.tilt = 0;
}

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

  var newCanvas = this.getBuffer().
      createBlankCanvas(cropImageRect.width, cropImageRect.height);

  var newContext = newCanvas.getContext("2d");
  ImageUtil.trace.resetTimer('transform');
  Rect.drawImage(newContext, this.getBuffer().getImageCanvas(),
      new Rect(newCanvas), cropImageRect);
  ImageUtil.trace.reportTimer('transform');

  this.getBuffer().setImageCanvas(newCanvas);
  this.getBuffer().fitImage();
};

ImageEditor.Mode.Crop.prototype.rollback = function() {
  this.createDefaultCrop();
};

ImageEditor.Mode.Crop.prototype.createDefaultCrop = function() {
  var rect = new Rect(this.getBuffer().getClippedImage());
  rect = rect.inflate (-rect.width / 6, -rect.height / 6);
  this.cropRect_ = new DraggableRect(
      rect, this.getBuffer(), ImageEditor.Mode.Crop.GRAB_RADIUS);
};

ImageEditor.Mode.Crop.prototype.draw = function(context) {
  var R = ImageEditor.Mode.Crop.GRAB_RADIUS;

  var inner = this.getBuffer().imageToScreenRect(this.cropRect_.getRect());
  var outer = this.getBuffer().getClippedScreen();

  var inner_bottom = inner.top + inner.height;
  var inner_right = inner.left + inner.width;

  context.globalAlpha = 0.25;
  context.fillStyle = '#000000';
  Rect.fillBetween(context, inner, outer);
  Rect.stroke(context, inner);

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
  context.fill();

  context.globalAlpha = 1;
  context.strokeStyle = '#808080';
  context.strokeRect(inner.left, inner.top, inner.width, inner.height);
  context.strokeRect(
      inner.left + inner.width / 3, inner.top, inner.width / 3, inner.height);
  context.strokeRect(
      inner.left, inner.top + inner.height / 3, inner.width, inner.height / 3);
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

function DraggableRect(rect, buffer, sensitivity) {

  // The bounds are not held in a regular rectangle (with width/height).
  // left/top/right/bottom held instead for convenience.
  this.bounds_ = {};
  this.bounds_[DraggableRect.LEFT] = rect.left;
  this.bounds_[DraggableRect.RIGHT] = rect.left + rect.width;
  this.bounds_[DraggableRect.TOP] = rect.top;
  this.bounds_[DraggableRect.BOTTOM] = rect.top + rect.height;

  this.buffer_ = buffer;
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
};

// Static members to simplify reflective access to the bounds.
DraggableRect.LEFT = 'left'
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
  var R = this.buffer_.screenToImageSize(this.sensitivity_);

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
        this.buffer_.screenToImageX(x), this.buffer_.screenToImageY(y));
  }
  if (mode.whole) return 'hand';
  if (mode.outside) return 'crosshair';
  return this.cssSide_[mode.ySide] + this.cssSide_[mode.xSide] + '-resize';
};

DraggableRect.prototype.getDragHandler = function(x, y) {
  x = this.buffer_.screenToImageX(x);
  y = this.buffer_.screenToImageY(y);

  var clipRect = this.buffer_.getClippedImage();
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
    return ImageUtil.clip(
        clipRect.left,
        self.buffer_.screenToImageX(x) + mouseBiasX,
        clipRect.left + clipRect.width - fixedWidth);
  }

  function convertY(y) {
    return ImageUtil.clip(
        clipRect.top,
        self.buffer_.screenToImageY(y) + mouseBiasY,
        clipRect.top + clipRect.height - fixedHeight);
  }

  return function(x, y) {
    if (resizeFuncX) resizeFuncX(convertX(x));
    if (resizeFuncY) resizeFuncY(convertY(y));
  };
};
