// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Namespace object for the utilities.
function ImageUtil() {}

// Performance trace.
ImageUtil.trace = (function() {
  function PerformanceTrace() {
    this.lines_ = {};
    this.timers_ = {};
    this.container_ = null;
  }

  PerformanceTrace.prototype.bindToDOM = function(container) {
    this.container_ = container;
  };

  PerformanceTrace.prototype.report = function(key, value) {
    if (!this.container_) return;
    if (!(key in this.lines_)) {
      var div = this.lines_[key] = document.createElement('div');
      this.container_.appendChild(div);
    }
    this.lines_[key].textContent = key + ': ' + value;
  };

  PerformanceTrace.prototype.resetTimer = function(key) {
    this.timers_[key] = Date.now();
  };

  PerformanceTrace.prototype.reportTimer = function(key) {
    this.report(key, (Date.now() - this.timers_[key]) + 'ms');
  };

  return new PerformanceTrace();
})();


ImageUtil.clamp = function(min, value, max) {
  return Math.max(min, Math.min(max, value));
};

ImageUtil.between = function(min, value, max) {
  return (value - min) * (value - max) <= 0;
};

/**
 * Rectangle class.
 */

/**
 * Rectange constructor takes 0, 1, 2 or 4 arguments.
 * Supports following variants:
 *   new Rect(left, top, width, height)
 *   new Rect(width, height)
 *   new Rect(rect)         // anything with left, top, width, height properties
 *   new Rect(bounds)       // anything with left, top, right, bottom properties
 *   new Rect(canvas|image) // anything with width and height properties.
 *   new Rect()             // empty rectangle.
 * @constructor
 */
function Rect(args) {
  switch (arguments.length) {
    case 4:
      this.left = arguments[0];
      this.top = arguments[1];
      this.width = arguments[2];
      this.height = arguments[3];
      return;

    case 2:
      this.left = 0;
      this.top = 0;
      this.width = arguments[0];
      this.height = arguments[1];
      return;

    case 1: {
      var source = arguments[0];
      if ('left' in source && 'top' in source) {
        this.left = source.left;
        this.top = source.top;
        if ('right' in source && 'bottom' in source) {
          this.width = source.right - source.left;
          this.height = source.bottom - source.top;
          return;
        }
      } else {
        this.left = 0;
        this.top = 0;
      }
      if ('width' in source && 'height' in source) {
        this.width = source.width;
        this.height = source.height;
        return;
      }
      break; // Fall through to the error message.
    }

    case 0:
      this.left = 0;
      this.top = 0;
      this.width = 0;
      this.height = 0;
      return;
  }
  console.error('Invalid Rect constructor arguments:',
       Array.apply(null, arguments));
}

/**
 * @return {Rect} A rectangle with every dimension scaled.
 */
Rect.prototype.scale = function(factor) {
  return new Rect(
      this.left * factor,
      this.top * factor,
      this.width * factor,
      this.height * factor);
};

/**
 * @return {Rect} A rectangle shifted by (dx,dy), same size.
 */
Rect.prototype.shift = function(dx, dy) {
  return new Rect(this.left + dx, this.top + dy, this.width, this.height);
};

/**
 * @return {Rect} A rectangle with left==x and top==y, same size.
 */
Rect.prototype.moveTo = function(x, y) {
  return new Rect(x, y, this.width, this.height);
};

/**
 * @return {Rect} A rectangle inflated by (dx, dy), same center.
 */
Rect.prototype.inflate = function(dx, dy) {
  return new Rect(
      this.left - dx, this.top - dy, this.width + 2 * dx, this.height + 2 * dy);
};

/**
 * @return {Boolean} True if the point lies inside the rectange.
 */
Rect.prototype.inside = function(x, y) {
  return this.left <= x && x < this.left + this.width &&
         this.top <= y && y < this.top + this.height;
};

Rect.prototype.intersects = function(rect) {
  return (this.left + this.width) > rect.left &&
         (rect.left + rect.width) > this.left &&
         (this.top + this.height) > rect.top &&
         (rect.top + rect.height) > this.top;
};

Rect.prototype.contains = function(rect) {
  return (this.left <= rect.left) &&
         (rect.left + rect.width) <= (this.left + this.width) &&
         (this.top <= rect.top) &&
         (rect.top + rect.height) <= (this.top + this.height);
};

/**
 * Clamp the rectangle to the bounds by moving it.
 * Decrease the size only if necessary.
 */
Rect.prototype.clamp = function(bounds) {
  var rect = new Rect(this);

  if (rect.width > bounds.width) {
    rect.left = bounds.left;
    rect.width = bounds.width;
  } else if (rect.left < bounds.left){
    rect.left = bounds.left;
  } else if (rect.left + rect.width >
             bounds.left + bounds.width) {
    rect.left = bounds.left + bounds.width - rect.width;
  }

  if (rect.height > bounds.height) {
    rect.top = bounds.top;
    rect.height = bounds.height;
  } else if (rect.top < bounds.top){
    rect.top = bounds.top;
  } else if (rect.top + rect.height >
             bounds.top + bounds.height) {
    rect.top = bounds.top + bounds.height - rect.height;
  }

  return rect;
};

Rect.prototype.toString = function() {
  return '(' + this.left + ',' + this.top + '):' +
         '(' + (this.left + this.width) + ',' + (this.top + this.height) + ')';
};
/*
 * Useful shortcuts for drawing (static functions).
 */

/**
 * Draw the image in context with appropriate scaling.
 */
Rect.drawImage = function(context, image, opt_dstRect, opt_srcRect) {
  opt_dstRect = opt_dstRect || new Rect(context.canvas);
  opt_srcRect = opt_srcRect || new Rect(image);
  context.drawImage(image,
      opt_srcRect.left, opt_srcRect.top, opt_srcRect.width, opt_srcRect.height,
      opt_dstRect.left, opt_dstRect.top, opt_dstRect.width, opt_dstRect.height);
};

/**
 * Draw a box around the rectangle.
 */
Rect.outline = function(context, rect) {
  context.strokeRect(
      rect.left - 0.5, rect.top - 0.5, rect.width + 1, rect.height + 1);
};

/**
 * Fill the rectangle.
 */
Rect.fill = function(context, rect) {
  context.fillRect(rect.left, rect.top, rect.width, rect.height);
};

/**
 * Fills the space between the two rectangles.
 */
Rect.fillBetween= function(context, inner, outer) {
  var inner_right = inner.left + inner.width;
  var inner_bottom = inner.top + inner.height;
  var outer_right = outer.left + outer.width;
  var outer_bottom = outer.top + outer.height;
  if (inner.top > outer.top) {
    context.fillRect(
        outer.left, outer.top, outer.width, inner.top - outer.top);
  }
  if (inner.left > outer.left) {
    context.fillRect(
        outer.left, inner.top, inner.left - outer.left, inner.height);
  }
  if (inner.width < outer_right) {
    context.fillRect(
        inner_right, inner.top, outer_right - inner_right, inner.height);
  }
  if (inner.height < outer_bottom) {
    context.fillRect(
        outer.left, inner_bottom, outer.width, outer_bottom - inner_bottom);
  }
};

/**
 * Circle class.
 */

function Circle(x, y, R) {
  this.x = x;
  this.y = y;
  this.squaredR = R * R;
}

Circle.prototype.inside = function(x, y) {
  x -= this.x;
  y -= this.y;
  return x * x + y * y <= this.squaredR;
};

/**
 * Copy an image applying scaling and rotation.
 *
 * @param {HTMLCanvasElement} dst destination
 * @param {HTMLCanvasElement|HTMLImageElement} src source
 * @param {number} scaleX
 * @param {number} scaleY
 * @param {number} angle (in radians)
 */
ImageUtil.drawImageTransformed = function(dst, src, scaleX, scaleY, angle) {
  var context = dst.getContext('2d');
  context.save();
  context.translate(context.canvas.width / 2, context.canvas.height / 2);
  context.rotate(angle);
  context.scale(scaleX, scaleY);
  context.drawImage(src, -src.width / 2, -src.height / 2);
  context.restore();
};


ImageUtil.deepCopy = function(obj) {
  if (obj == null || typeof obj != 'object')
    return obj;  // Copy built-in types as is.

  var res;
  if (obj.constructor.name == 'Array') {
    // obj.constructor == Array would give a false negative if obj came
    // from a different context.
    res = [];
    for (var i = 0; i != obj.length; i++) {
      res[i] = ImageUtil.deepCopy(obj[i]);
    }
  } else {
    res = {};
    for (var p in obj) {
      res[p] = ImageUtil.deepCopy(obj[p]);
    }
  }
  return res;
};

ImageUtil.setAttribute = function(element, attribute, on) {
  if (on)
    element.setAttribute(attribute, attribute);
  else
    element.removeAttribute(attribute);
};

/*
 * ImageLoader loads an image from a given URL into a canvas in two steps:
 * 1. Loads the image into an HTMLImageElement.
 * 2. Copies pixels from HTMLImageElement to HTMLCanvasElement. This is done
 *    stripe-by-stripe to avoid freezing up the UI. The transform is taken into
 *    account.
 */

ImageUtil.ImageLoader = function(document) {
  this.document_ = document;
  this.image_ = null;
};

/**
 * @param {string} url
 * @param {{scaleX: number, scaleY: number, rotate90: number}} transform
 * @param {function(HTMLCanvasElement} callback
 * @param {number} opt_delay Load delay in milliseconds, useful to let the
 *        animations play out before the computation heavy image loading starts.
 */
ImageUtil.ImageLoader.prototype.load = function(
    url, transform, callback, opt_delay) {
  this.cancel();

  this.url_ = url;
  this.transform_ = transform || { scaleX: 1, scaleY: 1, rotate90: 0};
  this.callback_ = callback;

  var self = this;
  function startLoad() {
    self.timeout_ = null;
    // The clients of this class sometimes request the same url repeatedly.
    // The onload fires only if the src is different from the previous value.
    // To work around that we create a new Image every time.
    self.image_ = new Image();
    self.image_.onload = function(e) {
      self.image_ = null;
      self.convertImage_(e.target);
    };
    self.image_.src = url;
  }
  if (opt_delay) {
    this.timeout_ = setTimeout(startLoad, opt_delay);
  } else {
    startLoad();
  }
};

ImageUtil.ImageLoader.prototype.isBusy = function() {
  return !!this.callback_;
};

ImageUtil.ImageLoader.prototype.isLoading = function(url) {
  return this.isBusy() && (this.url_ == url);
};

ImageUtil.ImageLoader.prototype.setCallback = function(callback) {
  this.callback_ = callback;
};

ImageUtil.ImageLoader.prototype.cancel = function() {
  if (!this.callback_) return;
  this.callback_ = null;
  if (this.timeout_) {
    clearTimeout(this.timeout_);
    this.timeout_ = null;
  }
  if (this.image_) {
    this.image_.onload = function(){};
    this.image_ = null;
  }
};

ImageUtil.ImageLoader.prototype.convertImage_ = function(image) {
  var canvas = this.document_.createElement('canvas');

  if (this.transform_.rotate90 & 1) {  // Rotated +/-90deg, swap the dimensions.
    canvas.width = image.height;
    canvas.height = image.width;
  } else  {
    canvas.width = image.width;
    canvas.height = image.height;
  }

  ImageUtil.trace.resetTimer('load-convert');

  var context = canvas.getContext('2d');
  context.save();
  context.translate(canvas.width / 2, canvas.height / 2);
  context.rotate(this.transform_.rotate90 * Math.PI/2);
  context.scale(this.transform_.scaleX, this.transform_.scaleY);

  var stripCount = Math.ceil(image.width * image.height / ( 1 << 21));
  var step = Math.max(16, Math.ceil(image.height / stripCount)) & 0xFFFFF0;

  this.copyStrip_(context, image, 0, step);
};

ImageUtil.ImageLoader.prototype.copyStrip_ = function(
    context, image, firstRow, rowCount) {
  var lastRow = Math.min (firstRow + rowCount, image.height);

  context.drawImage(
      image, 0, firstRow, image.width, lastRow - firstRow,
      -image.width / 2, firstRow - image.height / 2,
      image.width, lastRow - firstRow);

  if (lastRow == image.height) {
    context.restore();
    ImageUtil.trace.reportTimer('load-convert');
    var callback = this.callback_;
    this.callback_ = null;
    callback(context.canvas);
  } else {
    var self = this;
    this.timeout_ = setTimeout(
        function() {
          self.timeout_ = null;
          self.copyStrip_(context, image, lastRow, rowCount);
        }, 0);
  }
};

ImageUtil.removeChildren = function(element) {
  element.textContent = '';
};

ImageUtil.getFullNameFromUrl = function(url) {
  if (url.indexOf('/') != -1)
    return url.substr(url.lastIndexOf('/') + 1);
  else
    return url;
};

ImageUtil.getFileNameFromFullName = function(name) {
  var index = name.lastIndexOf('.');
  if (index != -1)
    return name.substr(0, index);
  else
    return name;
};

ImageUtil.getFileNameFromUrl = function(url) {
  return ImageUtil.getFileNameFromFullName(ImageUtil.getFullNameFromUrl(url));
};

ImageUtil.replaceFileNameInFullName = function(fullName, name) {
  var index = fullName.lastIndexOf('.');
  if (index != -1)
    return name + fullName.substr(index);
  else
    return name;
};
