// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Command queue is the only way to modify images.
 * Supports undo/redo.
 * Command execution is asynchronous (callback-based).
 *
 * @param {Document} document Document to create canvases in.
 * @param {HTMLCanvasElement} canvas The canvas with the original image.
 * @param {function(callback)} saveFunction Function to save the image.
 * @constructor
 */
function CommandQueue(document, canvas, saveFunction) {
  this.document_ = document;
  this.undo_ = [];
  this.redo_ = [];
  this.subscribers_ = [];
  this.currentImage_ = canvas;

  // Current image may be null or not-null but with width = height = 0.
  // Copying an image with zero dimensions causes js errors.
  if (this.currentImage_) {
    this.baselineImage_ = document.createElement('canvas');
    this.baselineImage_.width = this.currentImage_.width;
    this.baselineImage_.height = this.currentImage_.height;
    if (this.currentImage_.width > 0 && this.currentImage_.height > 0) {
      var context = this.baselineImage_.getContext('2d');
      context.drawImage(this.currentImage_, 0, 0);
    }
  } else {
    this.baselineImage_ = null;
  }

  this.previousImage_ = document.createElement('canvas');
  this.previousImageAvailable_ = false;

  this.saveFunction_ = saveFunction;
  this.busy_ = false;
  this.UIContext_ = {};
}

/**
 * Attach the UI elements to the command queue.
 * Once the UI is attached the results of image manipulations are displayed.
 *
 * @param {ImageView} imageView The ImageView object to display the results.
 * @param {ImageEditor.Prompt} prompt Prompt to use with this CommandQueue.
 * @param {function(boolean)} lock Function to enable/disable buttons etc.
 */
CommandQueue.prototype.attachUI = function(imageView, prompt, lock) {
  this.UIContext_ = {
    imageView: imageView,
    prompt: prompt,
    lock: lock
  };
};

/**
 * Execute the action when the queue is not busy.
 * @param {function} callback Callback.
 */
CommandQueue.prototype.executeWhenReady = function(callback) {
  if (this.isBusy())
    this.subscribers_.push(callback);
  else
    setTimeout(callback, 0);
};

/**
 * @return {boolean} True if the command queue is busy.
 */
CommandQueue.prototype.isBusy = function() { return this.busy_ };

/**
 * Set the queue state to busy. Lock the UI.
 * @private
 */
CommandQueue.prototype.setBusy_ = function() {
  if (this.busy_)
    throw new Error('CommandQueue already busy');

  this.busy_ = true;

  if (this.UIContext_.lock)
    this.UIContext_.lock(true);

  ImageUtil.trace.resetTimer('command-busy');
};

/**
 * Set the queue state to not busy. Unlock the UI and execute pending actions.
 * @private
 */
CommandQueue.prototype.clearBusy_ = function() {
  if (!this.busy_)
    throw new Error('Inconsistent CommandQueue already not busy');

  this.busy_ = false;

  // Execute the actions requested while the queue was busy.
  while (this.subscribers_.length)
    this.subscribers_.shift()();

  if (this.UIContext_.lock)
    this.UIContext_.lock(false);

  ImageUtil.trace.reportTimer('command-busy');
};

/**
 * Commit the image change: save and unlock the UI.
 * @param {number=} opt_delay Delay in ms (to avoid disrupting the animation).
 * @private
 */
CommandQueue.prototype.commit_ = function(opt_delay) {
  setTimeout(this.saveFunction_.bind(null, this.clearBusy_.bind(this)),
      opt_delay || 0);
};

/**
 * Internal function to execute the command in a given context.
 *
 * @param {Command} command The command to execute.
 * @param {Object} uiContext The UI context.
 * @param {function} callback Completion callback.
 * @private
 */
CommandQueue.prototype.doExecute_ = function(command, uiContext, callback) {
  if (!this.currentImage_)
    throw new Error('Cannot operate on null image');

  // Remember one previous image so that the first undo is as fast as possible.
  this.previousImage_.width = this.currentImage_.width;
  this.previousImage_.height = this.currentImage_.height;
  this.previousImageAvailable_ = true;
  var context = this.previousImage_.getContext('2d');
  context.drawImage(this.currentImage_, 0, 0);

  command.execute(
      this.document_,
      this.currentImage_,
      function(result, opt_delay) {
        this.currentImage_ = result;
        callback(opt_delay);
      }.bind(this),
      uiContext);
};

/**
 * Executes the command.
 *
 * @param {Command} command Command to execute.
 * @param {boolean=} opt_keep_redo True if redo stack should not be cleared.
 */
CommandQueue.prototype.execute = function(command, opt_keep_redo) {
  this.setBusy_();

  if (!opt_keep_redo)
    this.redo_ = [];

  this.undo_.push(command);

  this.doExecute_(command, this.UIContext_, this.commit_.bind(this));
};

/**
 * @return {boolean} True if Undo is applicable.
 */
CommandQueue.prototype.canUndo = function() {
  return this.undo_.length != 0;
};

/**
 * Undo the most recent command.
 */
CommandQueue.prototype.undo = function() {
  if (!this.canUndo())
    throw new Error('Cannot undo');

  this.setBusy_();

  var command = this.undo_.pop();
  this.redo_.push(command);

  var self = this;

  function complete() {
    var delay = command.revertView(
        self.currentImage_, self.UIContext_.imageView);
    self.commit_(delay);
  }

  if (this.previousImageAvailable_) {
    // First undo after an execute call.
    this.currentImage_.width = this.previousImage_.width;
    this.currentImage_.height = this.previousImage_.height;
    var context = this.currentImage_.getContext('2d');
    context.drawImage(this.previousImage_, 0, 0);

    // Free memory.
    this.previousImage_.width = 0;
    this.previousImage_.height = 0;
    this.previousImageAvailable_ = false;

    complete();
    // TODO(kaznacheev) Consider recalculating previousImage_ right here
    // by replaying the commands in the background.
  } else {
    this.currentImage_.width = this.baselineImage_.width;
    this.currentImage_.height = this.baselineImage_.height;
    var context = this.currentImage_.getContext('2d');
    context.drawImage(this.baselineImage_, 0, 0);

    var replay = function(index) {
      if (index < self.undo_.length)
        self.doExecute_(self.undo_[index], {}, replay.bind(null, index + 1));
      else {
        complete();
      }
    };

    replay(0);
  }
};

/**
 * @return {boolean} True if Redo is applicable.
 */
CommandQueue.prototype.canRedo = function() {
  return this.redo_.length != 0;
};

/**
 * Repeat the command that was recently un-done.
 */
CommandQueue.prototype.redo = function() {
  if (!this.canRedo())
    throw new Error('Cannot redo');

  this.execute(this.redo_.pop(), true);
};

/**
 * Closes internal buffers. Call to ensure, that internal buffers are freed
 * as soon as possible.
 */
CommandQueue.prototype.close = function() {
  // Free memory used by the undo buffer.
  this.previousImage_.width = 0;
  this.previousImage_.height = 0;
  this.previousImageAvailable_ = false;

  if (this.baselineImage_) {
    this.baselineImage_.width = 0;
    this.baselineImage_.height = 0;
  }
};

/**
 * Command object encapsulates an operation on an image and a way to visualize
 * its result.
 *
 * @param {string} name Command name.
 * @constructor
 */
function Command(name) {
  this.name_ = name;
}

/**
 * @return {string} String representation of the command.
 */
Command.prototype.toString = function() {
  return 'Command ' + this.name_;
};

/**
 * Execute the command and visualize its results.
 *
 * The two actions are combined into one method because sometimes it is nice
 * to be able to show partial results for slower operations.
 *
 * @param {Document} document Document on which to execute command.
 * @param {HTMLCanvasElement} srcCanvas Canvas to execute on.
 * @param {function(HTMLCanvasElement, number)} callback Callback to call on
 *   completion.
 * @param {Object} uiContext Context to work in.
 */
Command.prototype.execute = function(document, srcCanvas, callback, uiContext) {
  console.error('Command.prototype.execute not implemented');
};

/**
 * Visualize reversion of the operation.
 *
 * @param {HTMLCanvasElement} canvas Image data to use.
 * @param {ImageView} imageView ImageView to revert.
 * @return {number} Animation duration in ms.
 */
Command.prototype.revertView = function(canvas, imageView) {
  imageView.replace(canvas);
  return 0;
};

/**
 * Creates canvas to render on.
 *
 * @param {Document} document Document to create canvas in.
 * @param {HTMLCanvasElement} srcCanvas to copy optional dimensions from.
 * @param {number=} opt_width new canvas width.
 * @param {number=} opt_height new canvas height.
 * @return {HTMLCanvasElement} Newly created canvas.
 * @private
 */
Command.prototype.createCanvas_ = function(
    document, srcCanvas, opt_width, opt_height) {
  var result = document.createElement('canvas');
  result.width = opt_width || srcCanvas.width;
  result.height = opt_height || srcCanvas.height;
  return result;
};


/**
 * Rotate command
 * @param {number} rotate90 Rotation angle in 90 degree increments (signed).
 * @constructor
 * @extends {Command}
 */
Command.Rotate = function(rotate90) {
  Command.call(this, 'rotate(' + rotate90 * 90 + 'deg)');
  this.rotate90_ = rotate90;
};

Command.Rotate.prototype = { __proto__: Command.prototype };

/** @override */
Command.Rotate.prototype.execute = function(
    document, srcCanvas, callback, uiContext) {
  var result = this.createCanvas_(
      document,
      srcCanvas,
      (this.rotate90_ & 1) ? srcCanvas.height : srcCanvas.width,
      (this.rotate90_ & 1) ? srcCanvas.width : srcCanvas.height);
  ImageUtil.drawImageTransformed(
      result, srcCanvas, 1, 1, this.rotate90_ * Math.PI / 2);
  var delay;
  if (uiContext.imageView) {
    delay = uiContext.imageView.replaceAndAnimate(result, null, this.rotate90_);
  }
  setTimeout(callback, 0, result, delay);
};

/** @override */
Command.Rotate.prototype.revertView = function(canvas, imageView) {
  return imageView.replaceAndAnimate(canvas, null, -this.rotate90_);
};


/**
 * Crop command.
 *
 * @param {Rect} imageRect Crop rectange in image coordinates.
 * @constructor
 * @extends {Command}
 */
Command.Crop = function(imageRect) {
  Command.call(this, 'crop' + imageRect.toString());
  this.imageRect_ = imageRect;
};

Command.Crop.prototype = { __proto__: Command.prototype };

/** @override */
Command.Crop.prototype.execute = function(
    document, srcCanvas, callback, uiContext) {
  var result = this.createCanvas_(
      document, srcCanvas, this.imageRect_.width, this.imageRect_.height);
  Rect.drawImage(result.getContext('2d'), srcCanvas, null, this.imageRect_);
  var delay;
  if (uiContext.imageView) {
    delay = uiContext.imageView.replaceAndAnimate(result, this.imageRect_, 0);
  }
  setTimeout(callback, 0, result, delay);
};

/** @override */
Command.Crop.prototype.revertView = function(canvas, imageView) {
  return imageView.animateAndReplace(canvas, this.imageRect_);
};


/**
 * Filter command.
 *
 * @param {string} name Command name.
 * @param {function(ImageData,ImageData,number,number)} filter Filter function.
 * @param {string} message Message to display when done.
 * @constructor
 * @extends {Command}
 */
Command.Filter = function(name, filter, message) {
  Command.call(this, name);
  this.filter_ = filter;
  this.message_ = message;
};

Command.Filter.prototype = { __proto__: Command.prototype };

/** @override */
Command.Filter.prototype.execute = function(
    document, srcCanvas, callback, uiContext) {
  var result = this.createCanvas_(document, srcCanvas);

  var self = this;

  var previousRow = 0;

  function onProgressVisible(updatedRow, rowCount) {
    if (updatedRow == rowCount) {
      uiContext.imageView.replace(result);
      if (self.message_)
        uiContext.prompt.show(self.message_, 2000);
      callback(result);
    } else {
      var viewport = uiContext.imageView.viewport_;

      var imageStrip = new Rect(viewport.getImageBounds());
      imageStrip.top = previousRow;
      imageStrip.height = updatedRow - previousRow;

      var screenStrip = new Rect(viewport.getImageBoundsOnScreen());
      screenStrip.top = Math.round(viewport.imageToScreenY(previousRow));
      screenStrip.height =
          Math.round(viewport.imageToScreenY(updatedRow)) - screenStrip.top;

      uiContext.imageView.paintDeviceRect(
          viewport.screenToDeviceRect(screenStrip), result, imageStrip);
      previousRow = updatedRow;
    }
  }

  function onProgressInvisible(updatedRow, rowCount) {
    if (updatedRow == rowCount) {
      callback(result);
    }
  }

  filter.applyByStrips(result, srcCanvas, this.filter_,
      uiContext.imageView ? onProgressVisible : onProgressInvisible);
};
