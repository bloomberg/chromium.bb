// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Namespace object for the utilities.
function ImageUtil() {}

/**
 * Performance trace.
 */
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
    if (!(key in this.lines_)) {
      if (this.container_) {
        var div = this.lines_[key] = document.createElement('div');
        this.container_.appendChild(div);
      } else {
        this.lines_[key] = {};
      }
    }
    this.lines_[key].textContent = key + ': ' + value;
    if (ImageUtil.trace.log) this.dumpLine(key);
  };

  PerformanceTrace.prototype.resetTimer = function(key) {
    this.timers_[key] = Date.now();
  };

  PerformanceTrace.prototype.reportTimer = function(key) {
    this.report(key, (Date.now() - this.timers_[key]) + 'ms');
  };

  PerformanceTrace.prototype.dump = function() {
    for (var key in this.lines_)
      this.dumpLine(key);
  };

  PerformanceTrace.prototype.dumpLine = function(key) {
    console.log('trace.' + this.lines_[key].textContent);
  };

  return new PerformanceTrace();
})();

/**
 * @param {number} min Minimum value.
 * @param {number} value Value to adjust.
 * @param {number} max Maximum value.
 * @return {number} The closest to the |value| number in span [min, max].
 */
ImageUtil.clamp = function(min, value, max) {
  return Math.max(min, Math.min(max, value));
};

/**
 * @param {number} min Minimum value.
 * @param {number} value Value to check.
 * @param {number} max Maximum value.
 * @return {boolean} True if value is between.
 */
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
function Rect() {
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
 * @param {number} factor Factor to scale.
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
 * @param {number} dx Difference in X.
 * @param {number} dy Difference in Y.
 * @return {Rect} A rectangle shifted by (dx,dy), same size.
 */
Rect.prototype.shift = function(dx, dy) {
  return new Rect(this.left + dx, this.top + dy, this.width, this.height);
};

/**
 * @param {number} x Coordinate of the left top corner.
 * @param {number} y Coordinate of the left top corner.
 * @return {Rect} A rectangle with left==x and top==y, same size.
 */
Rect.prototype.moveTo = function(x, y) {
  return new Rect(x, y, this.width, this.height);
};

/**
 * @param {number} dx Difference in X.
 * @param {number} dy Difference in Y.
 * @return {Rect} A rectangle inflated by (dx, dy), same center.
 */
Rect.prototype.inflate = function(dx, dy) {
  return new Rect(
      this.left - dx, this.top - dy, this.width + 2 * dx, this.height + 2 * dy);
};

/**
 * @param {number} x Coordinate of the point.
 * @param {number} y Coordinate of the point.
 * @return {boolean} True if the point lies inside the rectange.
 */
Rect.prototype.inside = function(x, y) {
  return this.left <= x && x < this.left + this.width &&
         this.top <= y && y < this.top + this.height;
};

/**
 * @param {Rect} rect Rectangle to check.
 * @return {boolean} True if this recangle intersects with the |rect|.
 */
Rect.prototype.intersects = function(rect) {
  return (this.left + this.width) > rect.left &&
         (rect.left + rect.width) > this.left &&
         (this.top + this.height) > rect.top &&
         (rect.top + rect.height) > this.top;
};

/**
 * @param {Rect} rect Rectangle to check.
 * @return {boolean} True if this recangle containg the |rect|.
 */
Rect.prototype.contains = function(rect) {
  return (this.left <= rect.left) &&
         (rect.left + rect.width) <= (this.left + this.width) &&
         (this.top <= rect.top) &&
         (rect.top + rect.height) <= (this.top + this.height);
};

/**
 * @return {boolean} True if rectangle is empty.
 */
Rect.prototype.isEmpty = function() {
  return this.width == 0 || this.height == 0;
};

/**
 * Clamp the rectangle to the bounds by moving it.
 * Decrease the size only if necessary.
 * @param {Rect} bounds Bounds.
 * @return {Rect} Caclulated rectangle.
 */
Rect.prototype.clamp = function(bounds) {
  var rect = new Rect(this);

  if (rect.width > bounds.width) {
    rect.left = bounds.left;
    rect.width = bounds.width;
  } else if (rect.left < bounds.left) {
    rect.left = bounds.left;
  } else if (rect.left + rect.width >
             bounds.left + bounds.width) {
    rect.left = bounds.left + bounds.width - rect.width;
  }

  if (rect.height > bounds.height) {
    rect.top = bounds.top;
    rect.height = bounds.height;
  } else if (rect.top < bounds.top) {
    rect.top = bounds.top;
  } else if (rect.top + rect.height >
             bounds.top + bounds.height) {
    rect.top = bounds.top + bounds.height - rect.height;
  }

  return rect;
};

/**
 * @return {string} String representation.
 */
Rect.prototype.toString = function() {
  return '(' + this.left + ',' + this.top + '):' +
         '(' + (this.left + this.width) + ',' + (this.top + this.height) + ')';
};
/*
 * Useful shortcuts for drawing (static functions).
 */

/**
 * Draw the image in context with appropriate scaling.
 * @param {CanvasRenderingContext2D} context Context to draw.
 * @param {Image} image Image to draw.
 * @param {Rect=} opt_dstRect Rectangle in the canvas
 *                            (whole canvas by default).
 * @param {Rect=} opt_srcRect Rectangle in the imsge
 *                            (whole image by default).
 */
Rect.drawImage = function(context, image, opt_dstRect, opt_srcRect) {
  opt_dstRect = opt_dstRect || new Rect(context.canvas);
  opt_srcRect = opt_srcRect || new Rect(image);
  if (opt_dstRect.isEmpty() || opt_srcRect.isEmpty())
    return;
  context.drawImage(image,
      opt_srcRect.left, opt_srcRect.top, opt_srcRect.width, opt_srcRect.height,
      opt_dstRect.left, opt_dstRect.top, opt_dstRect.width, opt_dstRect.height);
};

/**
 * Draw a box around the rectangle.
 * @param {CanvasRenderingContext2D} context Context to draw.
 * @param {Rect} rect Rectangle.
 */
Rect.outline = function(context, rect) {
  context.strokeRect(
      rect.left - 0.5, rect.top - 0.5, rect.width + 1, rect.height + 1);
};

/**
 * Fill the rectangle.
 * @param {CanvasRenderingContext2D} context Context to draw.
 * @param {Rect} rect Rectangle.
 */
Rect.fill = function(context, rect) {
  context.fillRect(rect.left, rect.top, rect.width, rect.height);
};

/**
 * Fills the space between the two rectangles.
 * @param {CanvasRenderingContext2D} context Context to draw.
 * @param {Rect} inner Inner rectangle.
 * @param {Rect} outer Outer rectangle.
 */
Rect.fillBetween = function(context, inner, outer) {
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
 * @param {number} x X coordinate of circle center.
 * @param {number} y Y coordinate of circle center.
 * @param {number} r Radius.
 * @constructor
 */
function Circle(x, y, r) {
  this.x = x;
  this.y = y;
  this.squaredR = r * r;
}

/**
 * Check if the point is inside the circle.
 * @param {number} x X coordinate of the point.
 * @param {number} y Y coordinate of the point.
 * @return {boolean} True if the point is inside.
 */
Circle.prototype.inside = function(x, y) {
  x -= this.x;
  y -= this.y;
  return x * x + y * y <= this.squaredR;
};

/**
 * Copy an image applying scaling and rotation.
 *
 * @param {HTMLCanvasElement} dst Destination.
 * @param {HTMLCanvasElement|HTMLImageElement} src Source.
 * @param {number} scaleX Y scale transformation.
 * @param {number} scaleY X scale transformation.
 * @param {number} angle (in radians).
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

/**
 * @param {*} value Structure.
 * @return {*} Clone of the value.
 */
ImageUtil.deepCopy = function(value) {
  return JSON.parse(JSON.stringify(value));
};

/**
 * Adds or removes an attribute to/from an HTML element.
 * @param {HTMLElement} element To be applied to.
 * @param {string} attribute Name of attribute.
 * @param {boolean} on True if add, false if remove.
 */
ImageUtil.setAttribute = function(element, attribute, on) {
  if (on)
    element.setAttribute(attribute, '');
  else
    element.removeAttribute(attribute);
};

/**
 * Adds or removes CSS class to/from an HTML element.
 * @param {HTMLElement} element To be applied to.
 * @param {string} className Name of CSS class.
 * @param {boolean} on True if add, false if remove.
 */
ImageUtil.setClass = function(element, className, on) {
  var cl = element.classList;
  if (on)
    cl.add(className);
  else
    cl.remove(className);
};

/**
 * ImageLoader loads an image from a given URL into a canvas in two steps:
 * 1. Loads the image into an HTMLImageElement.
 * 2. Copies pixels from HTMLImageElement to HTMLCanvasElement. This is done
 *    stripe-by-stripe to avoid freezing up the UI. The transform is taken into
 *    account.
 * @param {HTMLDocument} document Owner document.
 * @constructor
 */
ImageUtil.ImageLoader = function(document) {
  this.document_ = document;
  this.image_ = new Image();
  this.generation_ = 0;
};

/**
 * Max size of image to be displayed (in pixels)
 */
ImageUtil.ImageLoader.IMAGE_SIZE_LIMIT = 25 * 1000 * 1000;

/**
 * @param {HTMLImageElement|HTMLCanvasElement|Object} image Image or image
 *   metadata, should have |width| and |height| properties.
 * @return {boolean} True if the image is too large to be loaded.
 */
ImageUtil.ImageLoader.isTooLarge = function(image) {
  return image.width * image.height > ImageUtil.ImageLoader.IMAGE_SIZE_LIMIT;
};

/**
 * @param {string} url Image URL.
 * @param {function(function(object))} transformFetcher function to get
 *    the image transform (which we need for the image orientation).
 * @param {function(HTMLCanvasElement)} callback To be called when loaded.
 * @param {number} opt_delay Load delay in milliseconds, useful to let the
 *        animations play out before the computation heavy image loading starts.
 */
ImageUtil.ImageLoader.prototype.load = function(
    url, transformFetcher, callback, opt_delay) {
  this.cancel();

  this.url_ = url;
  this.callback_ = callback;

  // The transform fetcher is not cancellable so we need a generation counter.
  var generation = ++this.generation_;
  var onTransform = function(image, transform) {
    if (generation == this.generation_) {
      this.convertImage_(
          image, transform || { scaleX: 1, scaleY: 1, rotate90: 0});
    }
  };

  var startLoad = function() {
    ImageUtil.metrics.startInterval(ImageUtil.getMetricName('LoadTime'));
    this.timeout_ = null;
    // The clients of this class sometimes request the same url repeatedly.
    // The onload fires only if the src is different from the previous value.
    // To work around that we reset the src temporarily.
    this.image_.src = '';
    var errorCallback = function(error) {
      this.image_.onerror = null;
      this.image_.onload = null;
      var tmpCallback = this.callback_;
      this.callback_ = null;
      var emptyCanvas = this.document_.createElement('canvas');
      emptyCanvas.width = 0;
      emptyCanvas.height = 0;
      tmpCallback(emptyCanvas, error);
    }.bind(this);
    this.image_.onload = function(e) {
      this.image_.onerror = null;
      this.image_.onload = null;
      if (ImageUtil.ImageLoader.isTooLarge(this.image_)) {
        errorCallback('IMAGE_TOO_BIG_ERROR');
        return;
      }
      transformFetcher(url, onTransform.bind(this, e.target));
    }.bind(this);
    // errorCallback has an optional error argument, which in case of general
    // error should not be specified
    this.image_.onerror = errorCallback.bind(this, 'IMAGE_ERROR');
    this.taskId_ = util.loadImage(this.image_, url);
  }.bind(this);
  if (opt_delay) {
    this.timeout_ = setTimeout(startLoad, opt_delay);
  } else {
    startLoad();
  }
};

/**
 * @return {boolean} True if an image is loading.
 */
ImageUtil.ImageLoader.prototype.isBusy = function() {
  return !!this.callback_;
};

/**
 * @param {string} url Image url.
 * @return {boolean} True if loader loads this image.
 */
ImageUtil.ImageLoader.prototype.isLoading = function(url) {
  return this.isBusy() && (this.url_ == url);
};

/**
 * @param {Function} callback To be called when the image loaded.
 */
ImageUtil.ImageLoader.prototype.setCallback = function(callback) {
  this.callback_ = callback;
};

/**
 * Stops loading image.
 */
ImageUtil.ImageLoader.prototype.cancel = function() {
  if (!this.callback_) return;
  this.callback_ = null;
  if (this.timeout_) {
    clearTimeout(this.timeout_);
    this.timeout_ = null;
  }
  if (this.image_) {
    this.image_.onload = function() {};
    this.image_.onerror = function() {};
    this.image_.src = '';
  }
  if (this.taskId_)
    util.cancelLoadImage(this.taskId_);
  this.generation_++;  // Silence the transform fetcher if it is in progress.
};

/**
 * @param {HTMLImageElement} image Image to be transformed.
 * @param {object} transform rransformation description to applay to the image.
 * @private
 */
ImageUtil.ImageLoader.prototype.convertImage_ = function(image, transform) {
  var canvas = this.document_.createElement('canvas');

  if (transform.rotate90 & 1) {  // Rotated +/-90deg, swap the dimensions.
    canvas.width = image.height;
    canvas.height = image.width;
  } else {
    canvas.width = image.width;
    canvas.height = image.height;
  }

  var context = canvas.getContext('2d');
  context.save();
  context.translate(canvas.width / 2, canvas.height / 2);
  context.rotate(transform.rotate90 * Math.PI / 2);
  context.scale(transform.scaleX, transform.scaleY);

  var stripCount = Math.ceil(image.width * image.height / (1 << 21));
  var step = Math.max(16, Math.ceil(image.height / stripCount)) & 0xFFFFF0;

  this.copyStrip_(context, image, 0, step);
};

/**
 * @param {CanvasRenderingContext2D} context Context to draw.
 * @param {HTMLImageElement} image Image to draw.
 * @param {number} firstRow Number of the first pixel row to draw.
 * @param {number} rowCount Count of pixel rows to draw.
 * @private
 */
ImageUtil.ImageLoader.prototype.copyStrip_ = function(
    context, image, firstRow, rowCount) {
  var lastRow = Math.min(firstRow + rowCount, image.height);

  context.drawImage(
      image, 0, firstRow, image.width, lastRow - firstRow,
      -image.width / 2, firstRow - image.height / 2,
      image.width, lastRow - firstRow);

  if (lastRow == image.height) {
    context.restore();
    if (this.url_.substr(0, 5) != 'data:') {  // Ignore data urls.
      ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('LoadTime'));
    }
    try {
      setTimeout(this.callback_, 0, context.canvas);
    } catch (e) {
      console.log(e);
    }
    this.callback_ = null;
  } else {
    var self = this;
    this.timeout_ = setTimeout(
        function() {
          self.timeout_ = null;
          self.copyStrip_(context, image, lastRow, rowCount);
        }, 0);
  }
};

/**
 * @param {HTMLElement} element To remove children from.
 */
ImageUtil.removeChildren = function(element) {
  element.textContent = '';
};

/**
 * @param {string} url "filesystem:" URL.
 * @return {string} File name.
 */
ImageUtil.getFullNameFromUrl = function(url) {
  url = decodeURIComponent(url);
  if (url.indexOf('/') != -1)
    return url.substr(url.lastIndexOf('/') + 1);
  else
    return url;
};

/**
 * @param {string} name File name (with extension).
 * @return {string} File name without extension.
 */
ImageUtil.getFileNameFromFullName = function(name) {
  var index = name.lastIndexOf('.');
  if (index != -1)
    return name.substr(0, index);
  else
    return name;
};

/**
 * @param {string} url "filesystem:" URL.
 * @return {string} File name.
 */
ImageUtil.getFileNameFromUrl = function(url) {
  return ImageUtil.getFileNameFromFullName(ImageUtil.getFullNameFromUrl(url));
};

/**
 * @param {string} fullName Original file name.
 * @param {string} name New file name without extension.
 * @return {string} New file name with base of |name| and extension of
 *                  |fullName|.
 */
ImageUtil.replaceFileNameInFullName = function(fullName, name) {
  var index = fullName.lastIndexOf('.');
  if (index != -1)
    return name + fullName.substr(index);
  else
    return name;
};

/**
 * @param {string} name File name.
 * @return {string} File extension.
 */
ImageUtil.getExtensionFromFullName = function(name) {
  var index = name.lastIndexOf('.');
  if (index != -1)
    return name.substring(index);
  else
    return '';
};

/**
 * Metrics (from metrics.js) itnitialized by the File Manager from owner frame.
 * @type {Object?}
 */
ImageUtil.metrics = null;

/**
 * @param {string} name Local name.
 * @return {string} Full name.
 */
ImageUtil.getMetricName = function(name) {
  return 'PhotoEditor.' + name;
};

/**
 * Used for metrics reporting, keep in sync with the histogram description.
 */
ImageUtil.FILE_TYPES = ['jpg', 'png', 'gif', 'bmp', 'webp'];
