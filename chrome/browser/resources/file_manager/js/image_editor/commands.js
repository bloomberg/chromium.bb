// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Command queue is the only way to modify images.
 * Supports undo/redo.
 * Command execution is asynchronous (callback-based).
 *
 * @param {Document} document Document to create canvases in.
 * @param {HTMLCanvasElement} canvas The canvas with the original image.
 * @param {function(callback)} saveFunction Function to save the image.
 */
function CommandQueue(document, canvas, saveFunction) {
  this.document_ = document;
  this.undo_ = [];
  this.redo_ = [];
  this.subscribers_ = [];

  this.baselineImage_ = canvas;
  this.currentImage_ = canvas;
  this.previousImage_ = null;

  this.saveFunction_ = saveFunction;

  this.busy_ = false;

  this.UIContext_ = {};
}

/**
 * Attach the UI elements to the command queue.
 * Once the UI is attached the results of image manipulations are displayed.
 *
 * @param {ImageView} imageView The ImageView object to display the results
 * @param {ImageEditor.Prompt} prompt
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
 */
CommandQueue.prototype.commit_ = function() {
  setTimeout(function() {
        this.saveFunction_(this.clearBusy_.bind(this));
      }.bind(this),
      ImageView.ANIMATION_WAIT_INTERVAL);
};

/**
 * Internal function to execute the command in a given context.
 *
 * @param {Command} command The command to execute.
 * @param {object} uiContext The UI context.
 * @param {function} callback Completion callback.
 */
CommandQueue.prototype.doExecute_ = function(command, uiContext, callback) {
  if (!this.currentImage_)
    throw new Error('Cannot operate on null image');

  // Remember one previous image so that the first undo is as fast as possible.
  this.previousImage_ = this.currentImage_;
  var self = this;
  command.execute(
      this.document_,
      this.currentImage_,
      function(result) {
        self.currentImage_ = result;
        callback();
      },
      uiContext);
};

/**
 * Executes the command.
 *
 * @param {Command} command
 * @param {boolean} opt_keep_redo true if redo stack should not be cleared.
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
    if (self.UIContext_.imageView) {
      command.revertView(self.currentImage_, self.UIContext_.imageView);
    }
    self.commit_();
  }

  if (this.previousImage_) {
    // First undo after an execute call.
    this.currentImage_ = this.previousImage_;
    this.previousImage_ = null;
    complete();
    // TODO(kaznacheev) Consider recalculating previousImage_ right here
    // by replaying the commands in the background.
  } else {
    this.currentImage_ = this.baselineImage_;

    function replay(index) {
      if (index < self.undo_.length)
        self.doExecute_(self.undo_[index], {}, replay.bind(null, index + 1));
      else {
        complete();
      }
    }

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
 * Command object encapsulates an operation on an image and a way to visualize
 * its result.
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
 * @param {Document} document
 * @param {HTMLCanvasElement} srcCanvas
 * @param {Object} uiContext
 * @param {function(HTMLCanvasElement)}
 */
Command.prototype.execute = function(document, srcCanvas, callback, uiContext) {
  setTimeout(callback.bind(null, null), 0);
};

/**
 * Visualize reversion of the operation.
 *
 * @param {HTMLCanvasElement} canvas
 * @param {ImageView} imageView
 */
Command.prototype.revertView = function(canvas, imageView) {
  imageView.replace(canvas);
};

Command.prototype.createCanvas_ = function(
    document, srcCanvas, opt_width, opt_height) {
  var result = document.createElement('canvas');
  result.width = opt_width || srcCanvas.width;
  result.height = opt_height || srcCanvas.height;
  return result;
};


/**
 * Rotate command
 * @param {number} rotate90 Rotation angle in 90 degree increments (signed)
 */
Command.Rotate = function(rotate90) {
  Command.call(this, 'rotate(' + rotate90 * 90 + 'deg)');
  this.rotate90_ = rotate90;
};

Command.Rotate.prototype = { __proto__: Command.prototype };

Command.Rotate.prototype.execute = function(
    document, srcCanvas, callback, uiContext) {
  var result = this.createCanvas_(
      document,
      srcCanvas,
      (this.rotate90_ & 1) ? srcCanvas.height : srcCanvas.width,
      (this.rotate90_ & 1) ? srcCanvas.width : srcCanvas.height);
  ImageUtil.drawImageTransformed(
      result, srcCanvas, 1, 1, this.rotate90_ * Math.PI / 2);
  if (uiContext.imageView) {
    uiContext.imageView.replaceAndAnimate(result, null, this.rotate90_);
  }
  setTimeout(callback.bind(null, result), 0);
};

Command.Rotate.prototype.revertView = function(canvas, imageView) {
  imageView.replaceAndAnimate(canvas, null, -this.rotate90_);
};


/**
 * Crop command.
 *
 * @param {Rect} imageRect Crop rectange in image coordinates.
 */
Command.Crop = function(imageRect) {
  Command.call(this, 'crop' + imageRect.toString());
  this.imageRect_ = imageRect;
};

Command.Crop.prototype = { __proto__: Command.prototype };

Command.Crop.prototype.execute = function(
    document, srcCanvas, callback, uiContext) {
  var result = this.createCanvas_(
      document, srcCanvas, this.imageRect_.width, this.imageRect_.height);
  Rect.drawImage(result.getContext("2d"), srcCanvas, null, this.imageRect_);
  if (uiContext.imageView) {
    uiContext.imageView.replaceAndAnimate(result, this.imageRect_, 0);
  }
  setTimeout(callback.bind(null, result), 0);
};

Command.Crop.prototype.revertView = function(canvas, imageView) {
  imageView.animateAndReplace(canvas, this.imageRect_);
};


/**
 * Filter command.
 *
 * @param {string} name Command name
 * @param {function(ImageData,ImageData,number,number)} filter Filter function
 * @param {string} message Message to display when done
 */
Command.Filter = function(name, filter, message) {
  Command.call(this, name);
  this.filter_ = filter;
  this.message_ = message;
};

Command.Filter.prototype = { __proto__: Command.prototype };

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
