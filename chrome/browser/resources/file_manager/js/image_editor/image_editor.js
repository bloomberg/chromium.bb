// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ImageEditor is the top level object that holds together and connects
 * everything needed for image editing.
 * @param {Viewport} viewport
 * @param {ImageView} imageView
 * @param {Object} DOMContainers
 * @param {Array.<ImageEditor.Mode>} modes
 * @param {Object} displayStringFunction
 */
function ImageEditor(
    viewport, imageView, DOMContainers, modes, displayStringFunction) {
  this.rootContainer_ = DOMContainers.root;
  this.container_ = DOMContainers.image;
  this.modes_ = modes;
  this.displayStringFunction_ = displayStringFunction;

  ImageUtil.removeChildren(this.container_);

  var document = this.container_.ownerDocument;

  this.viewport_ = viewport;
  this.viewport_.sizeByFrame(this.container_);

  this.buffer_ = new ImageBuffer();
  this.viewport_.addRepaintCallback(this.buffer_.draw.bind(this.buffer_));

  this.imageView_ = imageView;
  this.imageView_.addContentCallback(this.onContentUpdate_.bind(this));
  this.buffer_.addOverlay(this.imageView_);

  this.panControl_ = new ImageEditor.MouseControl(
      this.rootContainer_, this.container_, this.getBuffer());

  this.mainToolbar_ = new ImageEditor.Toolbar(
      DOMContainers.toolbar, displayStringFunction);

  this.modeToolbar_ = new ImageEditor.Toolbar(
      DOMContainers.mode, displayStringFunction,
      this.onOptionsChange.bind(this));

  this.prompt_ = new ImageEditor.Prompt(
      this.rootContainer_, displayStringFunction);

  this.createToolButtons();

  this.commandQueue_ = null;
}

ImageEditor.prototype.trackWindow = function(window) {
  if (window.resizeListener) {
    // Make sure we do not leak the previous instance.
    window.removeEventListener('resize', window.resizeListener, false);
  }
  window.resizeListener = this.resizeFrame.bind(this);
  window.addEventListener('resize', window.resizeListener, false);
};

ImageEditor.prototype.isLocked = function() {
  return !this.commandQueue_ || this.commandQueue_.isBusy();
};

ImageEditor.prototype.isBusy = function() {
  return this.commandQueue_ && this.commandQueue_.isBusy();
};

ImageEditor.prototype.lockUI = function(on) {
  ImageUtil.setAttribute(this.rootContainer_, 'locked', on);
};

ImageEditor.prototype.recordToolUse = function(name) {
  ImageUtil.metrics.recordEnum(
      ImageUtil.getMetricName('Tool'), name, this.actionNames_);
};

ImageEditor.prototype.onContentUpdate_ = function() {
  for (var i = 0; i != this.modes_.length; i++) {
    var mode = this.modes_[i];
    ImageUtil.setAttribute(mode.button_, 'disabled', !mode.isApplicable());
  }
};

ImageEditor.prototype.prefetchImage = function(id, source) {
  this.imageView_.prefetch(id, source);
};

ImageEditor.prototype.openSession = function(
    id, source, metadata, slide, saveFunction, callback) {
  if (this.commandQueue_)
    throw new Error('Session not closed');

  this.lockUI(true);

  var self = this;
  this.imageView_.load(id, source, metadata, slide, function(loadType) {
    self.lockUI(false);
    self.commandQueue_ = new CommandQueue(
        self.container_.ownerDocument,
        self.imageView_.getCanvas(),
        saveFunction);
    self.commandQueue_.attachUI(
        self.getImageView(), self.getPrompt(), self.lockUI.bind(self));
    self.updateUndoRedo();
    callback(loadType);
  });
};

/**
 * Close the current image editing session.
 * @param {function} callback Callback.
 */
ImageEditor.prototype.closeSession = function(callback) {
  this.getPrompt().hide();
  if (this.imageView_.isLoading()) {
    if (this.commandQueue_)
      console.warn('Inconsistent image editor state');
    this.imageView_.cancelLoad();
    this.lockUI(false);
    callback();
    return;
  }
  if (!this.commandQueue_)
    return;  // Session is already closing, ignore the callback.

  this.executeWhenReady(callback);
  this.commandQueue_ = null;
};

/**
 * Commit the current operation and execute the action.
 *
 * @param {function} callback Callback.
 */
ImageEditor.prototype.executeWhenReady = function(callback) {
  if (this.commandQueue_) {
    this.leaveModeGently();
    this.commandQueue_.executeWhenReady(callback);
  } else {
    if (!this.imageView_.isLoading())
      console.warn('Inconsistent image editor state');
    callback();
  }
};

ImageEditor.prototype.undo = function() {
  if (this.isLocked()) return;
  this.recordToolUse('undo');

  // First undo click should dismiss the uncommitted modifications.
  if (this.currentMode_ && this.currentMode_.isUpdated()) {
    this.currentMode_.reset();
    return;
  }

  this.getPrompt().hide();
  this.leaveMode(false);
  this.commandQueue_.undo();
  this.updateUndoRedo();
};

ImageEditor.prototype.redo = function() {
  if (this.isLocked()) return;
  this.recordToolUse('redo');
  this.getPrompt().hide();
  this.leaveMode(false);
  this.commandQueue_.redo();
  this.updateUndoRedo();
};

ImageEditor.prototype.updateUndoRedo = function() {
  var canUndo = this.commandQueue_ && this.commandQueue_.canUndo();
  var canRedo = this.commandQueue_ && this.commandQueue_.canRedo();
  ImageUtil.setAttribute(this.undoButton_, 'disabled', !canUndo);
  ImageUtil.setAttribute(this.redoButton_, 'hidden', !canRedo);
};

ImageEditor.prototype.getCanvas = function() {
  return this.getImageView().getCanvas();
};

/**
 * Window resize handler.
 */
ImageEditor.prototype.resizeFrame = function() {
  this.getViewport().sizeByFrameAndFit(this.container_);
  this.getViewport().repaint();
};

/**
 * @return {ImageBuffer}
 */
ImageEditor.prototype.getBuffer = function () { return this.buffer_ };

/**
 * @return {ImageView}
 */
ImageEditor.prototype.getImageView = function () { return this.imageView_ };

/**
 * @return {Viewport}
 */
ImageEditor.prototype.getViewport = function () { return this.viewport_ };

/**
 * @return {ImageEditor.Prompt}
 */
ImageEditor.prototype.getPrompt = function () { return this.prompt_ };

ImageEditor.prototype.onOptionsChange = function(options) {
  ImageUtil.trace.resetTimer('update');
  if (this.currentMode_) {
    this.currentMode_.update(options);
  }
  ImageUtil.trace.reportTimer('update');
};

/**
 * ImageEditor.Mode represents a modal state dedicated to a specific operation.
 * Inherits from ImageBuffer.Overlay to simplify the drawing of
 * mode-specific tools.
 */

ImageEditor.Mode = function(name) {
  this.name = name;
  this.message_ = 'enter_when_done';
};

ImageEditor.Mode.prototype = {__proto__: ImageBuffer.Overlay.prototype };

ImageEditor.Mode.prototype.getViewport = function() { return this.viewport_ };

ImageEditor.Mode.prototype.getImageView = function() { return this.imageView_ };

ImageEditor.Mode.prototype.getMessage = function() { return this.message_ };

ImageEditor.Mode.prototype.isApplicable = function() { return true };

/**
 * Called once after creating the mode button.
 */
ImageEditor.Mode.prototype.bind = function(editor, button) {
  this.editor_ = editor;
  this.editor_.registerAction_(this.name);
  this.button_ = button;
  this.viewport_ = editor.getViewport();
  this.imageView_ = editor.getImageView();
};

/**
 * Called before entering the mode.
 */
ImageEditor.Mode.prototype.setUp = function() {
  this.editor_.getBuffer().addOverlay(this);
  this.updated_ = false;
};

/**
 * Create mode-specific controls here.
 */
ImageEditor.Mode.prototype.createTools = function(toolbar) {};

/**
 * Called before exiting the mode.
 */
ImageEditor.Mode.prototype.cleanUpUI = function() {
  this.editor_.getBuffer().removeOverlay(this);
};

/**
 * Called after exiting the mode.
 */
ImageEditor.Mode.prototype.cleanUpCaches = function() {};

/**
 * Called when any of the controls changed its value.
 */
ImageEditor.Mode.prototype.update = function(options) {
  this.markUpdated();
};

ImageEditor.Mode.prototype.markUpdated = function() {
  this.updated_ = true;
};

ImageEditor.Mode.prototype.isUpdated= function() { return this.updated_ };

/**
 * Resets the mode to a clean state.
 */
ImageEditor.Mode.prototype.reset = function() {
  this.editor_.modeToolbar_.reset();
  this.updated_ = false;
};

/**
 * One-click editor tool, requires no interaction, just executes the command.
 * @param {string} name
 * @param {Command} command
 * @constructor
 */
ImageEditor.Mode.OneClick = function(name, command) {
  ImageEditor.Mode.call(this, name);
  this.instant = true;
  this.command_ = command;
};

ImageEditor.Mode.OneClick.prototype = {__proto__: ImageEditor.Mode.prototype};

ImageEditor.Mode.OneClick.prototype.getCommand = function() {
  return this.command_;
};

ImageEditor.prototype.registerAction_ = function(name) {
  this.actionNames_.push(name);
};

ImageEditor.prototype.createToolButtons = function() {
  this.mainToolbar_.clear();
  this.actionNames_ = [];

  var self = this;
  function createButton(name, handler) {
    return self.mainToolbar_.addButton(name, handler, name);
  }

  for (var i = 0; i != this.modes_.length; i++) {
    var mode = this.modes_[i];
    mode.bind(this, createButton(mode.name, this.enterMode.bind(this, mode)));
  }
  this.undoButton_ = createButton('undo', this.undo.bind(this));
  this.registerAction_('undo');

  this.redoButton_ = createButton('redo', this.redo.bind(this));
  this.registerAction_('redo');
};

ImageEditor.prototype.getMode = function() { return this.currentMode_ };

/**
 * The user clicked on the mode button.
 */
ImageEditor.prototype.enterMode = function(mode, event) {
  if (this.isLocked()) return;

  if (this.currentMode_ == mode) {
    // Currently active editor tool clicked, commit if modified.
    this.leaveMode(this.currentMode_.updated_);
    return;
  }

  this.recordToolUse(mode.name);

  this.leaveModeGently();
  // The above call could have caused a commit which might have initiated
  // an asynchronous command execution. Wait for it to complete, then proceed
  // with the mode set up.
  this.commandQueue_.executeWhenReady(
      this.setUpMode_.bind(this, mode, event));
};

ImageEditor.prototype.setUpMode_ = function(mode, event) {
  this.currentTool_ = event.target;

  ImageUtil.setAttribute(this.currentTool_, 'pressed', true);

  this.currentMode_ = mode;
  this.currentMode_.setUp();

  if (this.currentMode_.instant) {  // Instant tool.
    this.leaveMode(true);
    return;
  }

  this.getPrompt().show(this.currentMode_.getMessage());

  this.modeToolbar_.clear();
  this.currentMode_.createTools(this.modeToolbar_);
  this.modeToolbar_.show(true);
};

/**
 * The user clicked on 'OK' or 'Cancel' or on a different mode button.
 */
ImageEditor.prototype.leaveMode = function(commit) {
  if (!this.currentMode_) return;

  if (!this.currentMode_.instant) {
    this.getPrompt().hide();
  }

  this.modeToolbar_.show(false);

  this.currentMode_.cleanUpUI();
  if (commit) {
    var self = this;
    var command = this.currentMode_.getCommand();
    if (command) {  // Could be null if the user did not do anything.
      this.commandQueue_.execute(command);
      this.updateUndoRedo();
    }
  }
  this.currentMode_.cleanUpCaches();
  this.currentMode_ = null;

  ImageUtil.setAttribute(this.currentTool_, 'pressed', false);
  this.currentTool_ = null;
};

ImageEditor.prototype.leaveModeGently = function() {
  this.leaveMode(this.currentMode_ &&
                 this.currentMode_.updated_ &&
                 this.currentMode_.implicitCommit);
};

ImageEditor.prototype.onKeyDown = function(event) {
  switch(util.getKeyModifiers(event) + event.keyIdentifier) {
    case 'U+001B': // Escape
    case 'Enter':
      if (this.getMode()) {
        this.leaveMode(event.keyIdentifier == 'Enter');
        return true;
      }
      break;

    case 'Ctrl-U+005A':  // Ctrl+Z
      if (this.commandQueue_.canUndo()) {
        this.undo();
        return true;
      }
      break;

    case 'Ctrl-Shift-U+005A':  // Ctrl+Shift-Z
      if (this.commandQueue_.canRedo()) {
        this.redo();
        return true;
      }
      break;
  }
  return false;
};

/**
 * Hide the tools that overlap the given rectangular frame.
 *
 * @param {Rect} frame Hide the tool that overlaps this rect.
 * @param {Rect} transparent But do not hide the tool that is completely inside
 *                           this rect.
 */
ImageEditor.prototype.hideOverlappingTools = function(frame, transparent) {
  var tools = this.rootContainer_.ownerDocument.querySelectorAll('.dimmable');
  for (var i = 0; i != tools.length; i++) {
    var tool = tools[i];
    var toolRect = tool.getBoundingClientRect();
    ImageUtil.setAttribute(tool, 'dimmed',
        (frame && frame.intersects(toolRect)) &&
        !(transparent && transparent.contains(toolRect)));
  }
};

/**
 * Scale control for an ImageBuffer.
 */
ImageEditor.ScaleControl = function(parent, viewport) {
  this.viewport_ = viewport;
  this.viewport_.setScaleControl(this);

  var div = parent.ownerDocument.createElement('div');
  div.className = 'scale-tool';
  parent.appendChild(div);

  this.sizeDiv_ = parent.ownerDocument.createElement('div');
  this.sizeDiv_.className = 'size-div';
  div.appendChild(this.sizeDiv_);

  var scaleDiv = parent.ownerDocument.createElement('div');
  scaleDiv.className = 'scale-div';
  div.appendChild(scaleDiv);

  var scaleDown = parent.ownerDocument.createElement('button');
  scaleDown.className = 'scale-down';
  scaleDiv.appendChild(scaleDown);
  scaleDown.addEventListener('click', this.onDownButton.bind(this), false);
  scaleDown.textContent = '-';

  this.scaleRange_ = parent.ownerDocument.createElement('input');
  this.scaleRange_.type = 'range';
  this.scaleRange_.max = ImageEditor.ScaleControl.MAX_SCALE;
  this.scaleRange_.addEventListener(
      'change', this.onSliderChange.bind(this), false);
  scaleDiv.appendChild(this.scaleRange_);

  this.scaleLabel_ = parent.ownerDocument.createElement('span');
  scaleDiv.appendChild(this.scaleLabel_);

  var scaleUp = parent.ownerDocument.createElement('button');
  scaleUp.className = 'scale-up';
  scaleUp.textContent = '+';
  scaleUp.addEventListener('click', this.onUpButton.bind(this), false);
  scaleDiv.appendChild(scaleUp);

  var scale1to1 = parent.ownerDocument.createElement('button');
  scale1to1.className = 'scale-1to1';
  scale1to1.textContent = '1:1';
  scale1to1.addEventListener('click', this.on1to1Button.bind(this), false);
  scaleDiv.appendChild(scale1to1);

  var scaleFit = parent.ownerDocument.createElement('button');
  scaleFit.className = 'scale-fit';
  scaleFit.textContent = '\u2610';
  scaleFit.addEventListener('click', this.onFitButton.bind(this), false);
  scaleDiv.appendChild(scaleFit);
};

ImageEditor.ScaleControl.STANDARD_SCALES =
    [25, 33, 50, 67, 100, 150, 200, 300, 400, 500, 600, 800];

ImageEditor.ScaleControl.NUM_SCALES =
    ImageEditor.ScaleControl.STANDARD_SCALES.length;

ImageEditor.ScaleControl.MAX_SCALE = ImageEditor.ScaleControl.STANDARD_SCALES
    [ImageEditor.ScaleControl.NUM_SCALES - 1];

ImageEditor.ScaleControl.FACTOR = 100;

/**
 * Called when the buffer changes the content and decides that it should
 * have different min scale.
 */
ImageEditor.ScaleControl.prototype.setMinScale = function(scale) {
  this.scaleRange_.min = Math.min(
      Math.round(Math.min(1, scale) * ImageEditor.ScaleControl.FACTOR),
      ImageEditor.ScaleControl.MAX_SCALE);
};

/**
 * Called when the buffer changes the content.
 */
ImageEditor.ScaleControl.prototype.displayImageSize = function(width, height) {
  this.sizeDiv_.textContent = width + ' x ' +  height;
};

/**
 * Called when the buffer changes the scale independently from the controls.
 */
ImageEditor.ScaleControl.prototype.displayScale = function(scale) {
  this.updateSlider(Math.round(scale * ImageEditor.ScaleControl.FACTOR));
};

/**
 * Called when the user changes the scale via the controls.
 */
ImageEditor.ScaleControl.prototype.setScale = function (scale) {
  scale = ImageUtil.clamp(this.scaleRange_.min, scale, this.scaleRange_.max);
  this.updateSlider(scale);
  this.viewport_.setScale(scale / ImageEditor.ScaleControl.FACTOR, false);
  this.viewport_.repaint();
};

ImageEditor.ScaleControl.prototype.updateSlider = function(scale) {
  this.scaleLabel_.textContent = scale + '%';
  if (this.scaleRange_.value != scale)
      this.scaleRange_.value = scale;
};

ImageEditor.ScaleControl.prototype.onSliderChange = function (e) {
  this.setScale(e.target.value);
};

ImageEditor.ScaleControl.prototype.getSliderScale = function () {
  return this.scaleRange_.value;
};

ImageEditor.ScaleControl.prototype.onDownButton = function () {
  var percent = this.getSliderScale();
  var scales = ImageEditor.ScaleControl.STANDARD_SCALES;
  for(var i = scales.length - 1; i >= 0; i--) {
    var scale = scales[i];
    if (scale < percent) {
      this.setScale(scale);
      return;
    }
  }
  this.setScale(this.scaleRange_.min);
};

ImageEditor.ScaleControl.prototype.onUpButton = function () {
  var percent = this.getSliderScale();
  var scales = ImageEditor.ScaleControl.STANDARD_SCALES;
  for(var i = 0; i < scales.length; i++) {
    var scale = scales[i];
    if (scale > percent) {
      this.setScale(scale);
      return;
    }
  }
};

ImageEditor.ScaleControl.prototype.onFitButton = function () {
  this.viewport_.fitImage();
  this.viewport_.repaint();
};

ImageEditor.ScaleControl.prototype.on1to1Button = function () {
  this.viewport_.setScale(1);
  this.viewport_.repaint();
};

/**
 * A helper object for panning the ImageBuffer.
 * @constructor
 */
ImageEditor.MouseControl = function(rootContainer, container, buffer) {
  this.rootContainer_ = rootContainer;
  this.container_ = container;
  this.buffer_ = buffer;
  container.addEventListener('mousedown', this.onMouseDown.bind(this), false);
  container.addEventListener('mouseup', this.onMouseUp.bind(this), false);
  container.addEventListener('mousemove', this.onMouseMove.bind(this), false);
};

ImageEditor.MouseControl.getPosition = function(e) {
  var clientRect = e.target.getBoundingClientRect();
  return {
    x: e.clientX - clientRect.left,
    y: e.clientY - clientRect.top
  };
};

ImageEditor.MouseControl.prototype.onMouseDown = function(e) {
  var position = ImageEditor.MouseControl.getPosition(e);

  this.dragHandler_ = this.buffer_.getDragHandler(position.x, position.y);
  this.dragHappened_ = false;
  this.updateCursor_(position);
  e.preventDefault();
};

ImageEditor.MouseControl.prototype.onMouseUp = function(e) {
  var position = ImageEditor.MouseControl.getPosition(e);

  if (!this.dragHappened_) {
    this.buffer_.onClick(position.x, position.y);
  }
  this.dragHandler_ = null;
  this.dragHappened_ = false;
  this.lockMouse_(false);
  e.preventDefault();
};

ImageEditor.MouseControl.prototype.onMouseMove = function(e) {
  var position = ImageEditor.MouseControl.getPosition(e);

  if (this.dragHandler_ && !e.which) {
    // mouseup must have happened while the mouse was outside our window.
    this.dragHandler_ = null;
    this.lockMouse_(false);
  }

  this.updateCursor_(position);
  if (this.dragHandler_) {
    this.dragHandler_(position.x, position.y);
    this.dragHappened_ = true;
    this.lockMouse_(true);
  }
  e.preventDefault();
};

ImageEditor.MouseControl.prototype.lockMouse_ = function(on) {
  ImageUtil.setAttribute(this.rootContainer_, 'mousedrag', on);
};

ImageEditor.MouseControl.prototype.updateCursor_ = function(position) {
  this.container_.setAttribute('cursor',
      this.buffer_.getCursorStyle(position.x, position.y, !!this.dragHandler_));
};

/**
 * A toolbar for the ImageEditor.
 * @constructor
 */
ImageEditor.Toolbar = function(parent, displayStringFunction, updateCallback) {
  this.wrapper_ = parent;
  this.displayStringFunction_ = displayStringFunction;
  this.updateCallback_ = updateCallback;
};

ImageEditor.Toolbar.prototype.clear = function() {
  ImageUtil.removeChildren(this.wrapper_);
};

ImageEditor.Toolbar.prototype.create_ = function(tagName) {
  return this.wrapper_.ownerDocument.createElement(tagName);
};

ImageEditor.Toolbar.prototype.add = function(element) {
  this.wrapper_.appendChild(element);
  return element;
};

ImageEditor.Toolbar.prototype.addLabel = function(text) {
  var label = this.create_('span');
  label.textContent = this.displayStringFunction_(text);
  return this.add(label);
};

ImageEditor.Toolbar.prototype.addButton = function(
    text, handler, opt_class1) {
  var button = this.create_('div');
  button.classList.add('button');
  if (opt_class1) button.classList.add(opt_class1);
  button.textContent = this.displayStringFunction_(text);
  button.addEventListener('click', handler, false);
  return this.add(button);
};

/**
 * @param {string} name An option name.
 * @param {number} min Min value of the option.
 * @param {number} value Default value of the option.
 * @param {number} max Max value of the options.
 * @param {number} scale A number to multiply by when setting
 *                       min/value/max in DOM.
 * @param {Boolean} opt_showNumeric True if numeric value should be displayed.
 */
ImageEditor.Toolbar.prototype.addRange = function(
    name, min, value, max, scale, opt_showNumeric) {
  var self = this;

  scale = scale || 1;

  var range = this.create_('input');

  range.className = 'range';
  range.type = 'range';
  range.name = name;
  range.min = Math.ceil(min * scale);
  range.max = Math.floor(max * scale);

  var numeric = this.create_('div');
  numeric.className = 'numeric';
  function mirror() {
    numeric.textContent = Math.round(range.getValue() * scale) / scale;
  }

  range.setValue = function(newValue) {
    range.value = Math.round(newValue * scale);
    mirror();
  };

  range.getValue = function() {
    return Number(range.value) / scale;
  };

  range.reset = function() {
    range.setValue(value);
  };

  range.addEventListener('change',
      function() {
        mirror();
        self.updateCallback_(self.getOptions());
      },
      false);

  range.setValue(value);

  var label = this.create_('div');
  label.textContent = this.displayStringFunction_(name);
  label.className = 'label ' + name;
  this.add(label);
  this.add(range);
  if (opt_showNumeric) this.add(numeric);

  return range;
};

ImageEditor.Toolbar.prototype.getOptions = function() {
  var values = {};
  for (var child = this.wrapper_.firstChild; child; child = child.nextSibling) {
    if (child.name)
      values[child.name] = child.getValue();
  }
  return values;
};

ImageEditor.Toolbar.prototype.reset = function() {
  for (var child = this.wrapper_.firstChild; child; child = child.nextSibling) {
    if (child.reset) child.reset();
  }
};

ImageEditor.Toolbar.prototype.show = function(on) {
  if (!this.wrapper_.firstChild)
    return;  // Do not show empty toolbar;

  ImageUtil.setAttribute(this.wrapper_, 'hidden', !on);
};

/** A prompt panel for the editor.
 *
 * @param {HTMLElement} container
 */
ImageEditor.Prompt = function(container, displayStringFunction_) {
  this.container_ = container;
  this.displayStringFunction_ = displayStringFunction_;
};

ImageEditor.Prompt.prototype.reset = function() {
  this.cancelTimer();
  if (this.wrapper_) {
    this.container_.removeChild(this.wrapper_);
    this.wrapper_ = null;
    this.prompt_ = null;
  }
};

ImageEditor.Prompt.prototype.cancelTimer = function() {
  if (this.timer_) {
    clearTimeout(this.timer_);
    this.timer_ = null;
  }
};

ImageEditor.Prompt.prototype.setTimer = function(callback, timeout) {
  this.cancelTimer();
  var self = this;
  this.timer_ = setTimeout(function() {
    self.timer_ = null;
    callback();
  }, timeout);
};

ImageEditor.Prompt.prototype.show = function(text, timeout, formatArgs) {
  this.showAt.apply(this,
      ['center'].concat(Array.prototype.slice.call(arguments)));
};

ImageEditor.Prompt.prototype.showAt = function(pos, text, timeout, formatArgs) {
  this.reset();
  if (!text) return;

  var document = this.container_.ownerDocument;
  this.wrapper_ = document.createElement('div');
  this.wrapper_.className = 'prompt-wrapper';
  this.wrapper_.setAttribute('pos', pos);
  this.container_.appendChild(this.wrapper_);

  this.prompt_ = document.createElement('div');
  this.prompt_.className = 'prompt';

  // Create an extra wrapper which opacity can be manipulated separately.
  var tool = document.createElement('div');
  tool.className = 'dimmable';
  this.wrapper_.appendChild(tool);
  tool.appendChild(this.prompt_);

  var args = [text].concat(Array.prototype.slice.call(arguments, 3));
  this.prompt_.textContent = this.displayStringFunction_.apply(null, args);

  var close = document.createElement('div');
  close.className = 'close';
  close.addEventListener('click', this.hide.bind(this));
  this.prompt_.appendChild(close);

  setTimeout(
      this.prompt_.setAttribute.bind(this.prompt_, 'state', 'fadein'), 0);

  if (timeout)
    this.setTimer(this.hide.bind(this), timeout);
};

ImageEditor.Prompt.prototype.hide = function() {
  if (!this.prompt_) return;
  this.prompt_.setAttribute('state', 'fadeout');
  // Allow some time for the animation to play out.
  this.setTimer(this.reset.bind(this), 500);
};
