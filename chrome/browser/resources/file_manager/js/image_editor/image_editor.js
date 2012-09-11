// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ImageEditor is the top level object that holds together and connects
 * everything needed for image editing.
 * @param {Viewport} viewport The viewport.
 * @param {ImageView} imageView The ImageView containing the images to edit.
 * @param {ImageEditor.Prompt} prompt Prompt instance.
 * @param {Object} DOMContainers Various DOM containers required for the editor.
 * @param {Array.<ImageEditor.Mode>} modes Available editor modes.
 * @param {function} displayStringFunction String formatting function.
 * @constructor
 */
function ImageEditor(
    viewport, imageView, prompt, DOMContainers, modes, displayStringFunction) {
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

  this.panControl_.setDoubleTapCallback(this.onDoubleTap_.bind(this));

  this.mainToolbar_ = new ImageEditor.Toolbar(
      DOMContainers.toolbar, displayStringFunction);

  this.modeToolbar_ = new ImageEditor.Toolbar(
      DOMContainers.mode, displayStringFunction,
      this.onOptionsChange.bind(this));

  this.prompt_ = prompt;

  this.createToolButtons();

  this.commandQueue_ = null;
}

/**
 * @return {boolean} True if no user commands are to be accepted.
 */
ImageEditor.prototype.isLocked = function() {
  return !this.commandQueue_ || this.commandQueue_.isBusy();
};

/**
 * @return {boolean} True if the command queue is busy.
 */
ImageEditor.prototype.isBusy = function() {
  return this.commandQueue_ && this.commandQueue_.isBusy();
};

/**
 * Reflect the locked state of the editor in the UI.
 * @param {boolean} on True if locked.
 */
ImageEditor.prototype.lockUI = function(on) {
  ImageUtil.setAttribute(this.rootContainer_, 'locked', on);
};

/**
 * Report the tool use to the metrics subsystem.
 * @param {string} name Action name.
 */
ImageEditor.prototype.recordToolUse = function(name) {
  ImageUtil.metrics.recordEnum(
      ImageUtil.getMetricName('Tool'), name, this.actionNames_);
};

/**
 * Content update handler.
 * @private
 */
ImageEditor.prototype.onContentUpdate_ = function() {
  for (var i = 0; i != this.modes_.length; i++) {
    var mode = this.modes_[i];
    ImageUtil.setAttribute(mode.button_, 'disabled', !mode.isApplicable());
  }
};

/**
 * Request prefetch for an image.
 * @param {string} url Image url.
 */
ImageEditor.prototype.prefetchImage = function(url) {
  this.imageView_.prefetch(url);
};

/**
 * Open the editing session for a new image.
 *
 * @param {string} url Image url.
 * @param {object} metadata Metadata.
 * @param {object} effect Transition effect object.
 * @param {function(function)} saveFunction Image save function.
 * @param {function} displayCallback Display callback.
 * @param {function} loadCallback Load callback.
 */
ImageEditor.prototype.openSession = function(
    url, metadata, effect, saveFunction, displayCallback, loadCallback) {
  if (this.commandQueue_)
    throw new Error('Session not closed');

  this.lockUI(true);

  var self = this;
  this.imageView_.load(
      url, metadata, effect, displayCallback, function(loadType) {
    self.lockUI(false);
    self.commandQueue_ = new CommandQueue(
        self.container_.ownerDocument,
        self.imageView_.getCanvas(),
        saveFunction);
    self.commandQueue_.attachUI(
        self.getImageView(), self.getPrompt(), self.lockUI.bind(self));
    self.updateUndoRedo();
    loadCallback(loadType);
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

/**
 * Undo the recently executed command.
 */
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

/**
 * Redo the recently un-done command.
 */
ImageEditor.prototype.redo = function() {
  if (this.isLocked()) return;
  this.recordToolUse('redo');
  this.getPrompt().hide();
  this.leaveMode(false);
  this.commandQueue_.redo();
  this.updateUndoRedo();
};

/**
 * Update Undo/Redo buttons state.
 */
ImageEditor.prototype.updateUndoRedo = function() {
  var canUndo = this.commandQueue_ && this.commandQueue_.canUndo();
  var canRedo = this.commandQueue_ && this.commandQueue_.canRedo();
  ImageUtil.setAttribute(this.undoButton_, 'disabled', !canUndo);
  this.redoButton_.hidden = !canRedo;
};

/**
 * @return {HTMLCanvasElement} The current image canvas.
 */
ImageEditor.prototype.getCanvas = function() {
  return this.getImageView().getCanvas();
};

/**
 * @return {ImageBuffer} ImageBuffer instance.
 */
ImageEditor.prototype.getBuffer = function() { return this.buffer_ };

/**
 * @return {ImageView} ImageView instance.
 */
ImageEditor.prototype.getImageView = function() { return this.imageView_ };

/**
 * @return {Viewport} Viewport instance.
 */
ImageEditor.prototype.getViewport = function() { return this.viewport_ };

/**
 * @return {ImageEditor.Prompt} Prompt instance.
 */
ImageEditor.prototype.getPrompt = function() { return this.prompt_ };

/**
 * Handle the toolbar controls update.
 * @param {object} options A map of options.
 */
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
 * @param {string} name The mode name.
 * @constructor
 */

ImageEditor.Mode = function(name) {
  this.name = name;
  this.message_ = 'enter_when_done';
};

ImageEditor.Mode.prototype = {__proto__: ImageBuffer.Overlay.prototype };

/**
 * @return {Viewport} Viewport instance.
 */
ImageEditor.Mode.prototype.getViewport = function() { return this.viewport_ };

/**
 * @return {ImageView} ImageView instance.
 */
ImageEditor.Mode.prototype.getImageView = function() { return this.imageView_ };

/**
 * @return {string} The mode-specific message to be displayed when entering.
 */
ImageEditor.Mode.prototype.getMessage = function() { return this.message_ };

/**
 * @return {boolean} True if the mode is applicable in the current context.
 */
ImageEditor.Mode.prototype.isApplicable = function() { return true };

/**
 * Called once after creating the mode button.
 *
 * @param {ImageEditor} editor The editor instance.
 * @param {HTMLElement} button The mode button.
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
 * @param {ImageEditor.Toolbar} toolbar The toolbar to populate.
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
 * @param {object} options A map of options.
 */
ImageEditor.Mode.prototype.update = function(options) {
  this.markUpdated();
};

/**
 * Mark the editor mode as updated.
 */
ImageEditor.Mode.prototype.markUpdated = function() {
  this.updated_ = true;
};

/**
 * @return {boolean} True if the mode controls changed.
 */
ImageEditor.Mode.prototype.isUpdated = function() { return this.updated_ };

/**
 * Resets the mode to a clean state.
 */
ImageEditor.Mode.prototype.reset = function() {
  this.editor_.modeToolbar_.reset();
  this.updated_ = false;
};

/**
 * One-click editor tool, requires no interaction, just executes the command.
 * @param {string} name The mode name.
 * @param {Command} command The command to execute on click.
 * @constructor
 */
ImageEditor.Mode.OneClick = function(name, command) {
  ImageEditor.Mode.call(this, name);
  this.instant = true;
  this.command_ = command;
};

ImageEditor.Mode.OneClick.prototype = {__proto__: ImageEditor.Mode.prototype};

/**
 * @return {Command} command
 */
ImageEditor.Mode.OneClick.prototype.getCommand = function() {
  return this.command_;
};

/**
 * Register the action name. Required for metrics reporting.
 * @param {string} name Button name.
 * @private
 */
ImageEditor.prototype.registerAction_ = function(name) {
  this.actionNames_.push(name);
};

/**
 * Populate the toolbar.
 */
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

/**
 * @return {ImageEditor.Mode} The current mode.
 */
ImageEditor.prototype.getMode = function() { return this.currentMode_ };

/**
 * The user clicked on the mode button.
 *
 * @param {ImageEditor.Mode} mode The new mode.
 */
ImageEditor.prototype.enterMode = function(mode) {
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
  this.commandQueue_.executeWhenReady(this.setUpMode_.bind(this, mode));
};

/**
 * Set up the new editing mode.
 *
 * @param {ImageEditor.Mode} mode The mode.
 * @private
 */
ImageEditor.prototype.setUpMode_ = function(mode) {
  this.currentTool_ = mode.button_;

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
 * @param {boolean} commit True if commit is required.
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

/**
 * Leave the mode, commit only if required by the current mode.
 */
ImageEditor.prototype.leaveModeGently = function() {
  this.leaveMode(this.currentMode_ &&
                 this.currentMode_.updated_ &&
                 this.currentMode_.implicitCommit);
};

/**
 * Enter the editor mode with the given name.
 *
 * @param {string} name Mode name.
 * @private
 */
ImageEditor.prototype.enterModeByName_ = function(name) {
  for (var i = 0; i != this.modes_.length; i++) {
    var mode = this.modes_[i];
    if (mode.name == name) {
      if (!mode.button_.hasAttribute('disabled'))
        this.enterMode(mode);
      return;
    }
  }
  console.error('Mode "' + name + '" not found.');
};

/**
 * Key down handler.
 * @param {Event} event The keydown event.
 * @return {boolean} True if handled.
 */
ImageEditor.prototype.onKeyDown = function(event) {
  switch (util.getKeyModifiers(event) + event.keyIdentifier) {
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

    case 'Ctrl-U+0059':  // Ctrl+Y
      if (this.commandQueue_.canRedo()) {
        this.redo();
        return true;
      }
      break;

    case 'U+0041':  // 'a'
      this.enterModeByName_('autofix');
      return true;

    case 'U+0042':  // 'b'
      this.enterModeByName_('exposure');
      return true;

    case 'U+0043':  // 'c'
      this.enterModeByName_('crop');
      return true;

    case 'U+004C':  // 'l'
      this.enterModeByName_('rotate_left');
      return true;

    case 'U+0052':  // 'r'
      this.enterModeByName_('rotate_right');
      return true;

  }
  return false;
};

/**
 * Double tap handler.
 * @param {number} x X coordinate of the event.
 * @param {number} y Y coordinate of the event.
 * @private
 */
ImageEditor.prototype.onDoubleTap_ = function(x, y) {
  if (this.getMode()) {
    var action = this.buffer_.getDoubleTapAction(x, y);
    if (action == ImageBuffer.DoubleTapAction.COMMIT)
      this.leaveMode(true);
    else if (action == ImageBuffer.DoubleTapAction.CANCEL)
      this.leaveMode(false);
  }
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
 * A helper object for panning the ImageBuffer.
 *
 * @param {HTMLElement} rootContainer The top-level container.
 * @param {HTMLElement} container The container for mouse events.
 * @param {ImageBuffer} buffer Image buffer.
 * @constructor
 */
ImageEditor.MouseControl = function(rootContainer, container, buffer) {
  this.rootContainer_ = rootContainer;
  this.container_ = container;
  this.buffer_ = buffer;

  var handlers = {
    'touchstart': this.onTouchStart,
    'touchend': this.onTouchEnd,
    'touchcancel': this.onTouchCancel,
    'touchmove': this.onTouchMove,

     'mousedown': this.onMouseDown,
     'mouseup': this.onMouseUp,
     'mousemove': this.onMouseMove
  };

  for (var eventName in handlers) {
    container.addEventListener(
        eventName, handlers[eventName].bind(this), false);
  }
};

/**
 * Maximum movement for touch to be detected as a tap (in pixels).
 * @private
 */
ImageEditor.MouseControl.MAX_MOVEMENT_FOR_TAP_ = 8;

/**
 * Maximum time for touch to be detected as a tap (in milliseconds).
 * @private
 */
ImageEditor.MouseControl.MAX_TAP_DURATION_ = 500;

/**
 * Maximum distance from the first tap to the second tap to be considered
 * as a double tap.
 * @private
 */
ImageEditor.MouseControl.MAX_DISTANCE_FOR_DOUBLE_TAP_ = 32;

/**
 * Maximum time for touch to be detected as a double tap (in milliseconds).
 * @private
 */
ImageEditor.MouseControl.MAX_DOUBLE_TAP_DURATION_ = 1000;

/**
 * Convert the event position from the event object to client coordinates.
 *
 * @param {MouseEvent|Touch} e Pointer position.
 * @return {object} A pair of x,y in client coordinates.
 * @private
 */
ImageEditor.MouseControl.getPosition_ = function(e) {
  var clientRect = e.target.getBoundingClientRect();
  return {
    x: e.clientX - clientRect.left,
    y: e.clientY - clientRect.top
  };
};

/**
 * Returns touch position or null if there is more than one touch position.
 *
 * @param {TouchEvent} e Event.
 * @return {object?} A pair of x,y in client coordinates.
 * @private
 */
ImageEditor.MouseControl.prototype.getTouchPosition_ = function(e) {
  if (e.targetTouches.length == 1)
    return ImageEditor.MouseControl.getPosition_(e.targetTouches[0]);
  else
    return null;
};

/**
 * Touch start handler.
 * @param {TouchEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onTouchStart = function(e) {
  var position = this.getTouchPosition_(e);
  if (position) {
    this.touchStartInfo_ = {
      x: position.x,
      y: position.y,
      time: Date.now()
    };
    this.dragHandler_ = this.buffer_.getDragHandler(position.x, position.y,
                                                    true /* touch */);
    this.dragHappened_ = false;
  }
  e.preventDefault();
};

/**
 * Touch end handler.
 * @param {TouchEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onTouchEnd = function(e) {
  if (!this.dragHappened_ && Date.now() - this.touchStartInfo_.time <=
                             ImageEditor.MouseControl.MAX_TAP_DURATION_) {
    this.buffer_.onClick(this.touchStartInfo_.x, this.touchStartInfo_.y);
    if (this.previousTouchStartInfo_ &&
        Date.now() - this.previousTouchStartInfo_.time <
            ImageEditor.MouseControl.MAX_DOUBLE_TAP_DURATION_) {
      var prevTouchCircle = new Circle(
          this.previousTouchStartInfo_.x,
          this.previousTouchStartInfo_.y,
          ImageEditor.MouseControl.MAX_DISTANCE_FOR_DOUBLE_TAP_);
      if (prevTouchCircle.inside(this.touchStartInfo_.x,
                                 this.touchStartInfo_.y)) {
        this.doubleTapCallback_(this.touchStartInfo_.x, this.touchStartInfo_.y);
      }
    }
    this.previousTouchStartInfo_ = this.touchStartInfo_;
  } else {
    this.previousTouchStartInfo_ = null;
  }
  this.onTouchCancel(e);
  e.preventDefault();
};

/**
 * Default double tap handler.
 * @param {number} x X coordinate of the event.
 * @param {number} y Y coordinate of the event.
 * @private
 */
ImageEditor.MouseControl.prototype.doubleTapCallback_ = function(x, y) {};

/**
 * Sets callback to be called when double tap detected.
 * @param {function(number, number)} callback New double tap callback.
 */
ImageEditor.MouseControl.prototype.setDoubleTapCallback = function(callback) {
  this.doubleTapCallback_ = callback;
};

/**
 * Touch chancel handler.
 */
ImageEditor.MouseControl.prototype.onTouchCancel = function() {
  this.dragHandler_ = null;
  this.dragHappened_ = false;
  this.touchStartInfo_ = null;
  this.lockMouse_(false);
};

/**
 * Touch move handler.
 * @param {TouchEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onTouchMove = function(e) {
  var position = this.getTouchPosition_(e);
  if (!position)
    return;

  if (this.touchStartInfo_ && !this.dragHappened_) {
    var tapCircle = new Circle(this.touchStartInfo_.x, this.touchStartInfo_.y,
                    ImageEditor.MouseControl.MAX_MOVEMENT_FOR_TAP_);
    this.dragHappened_ = !tapCircle.inside(position.x, position.y);
  }
  if (this.dragHandler_ && this.dragHappened_) {
    this.dragHandler_(position.x, position.y);
    this.lockMouse_(true);
  }
  e.preventDefault();
};

/**
 * Mouse down handler.
 * @param {MouseEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onMouseDown = function(e) {
  var position = ImageEditor.MouseControl.getPosition_(e);

  this.dragHandler_ = this.buffer_.getDragHandler(position.x, position.y,
                                                  false /* mouse */);
  this.dragHappened_ = false;
  this.updateCursor_(position);
  e.preventDefault();
};

/**
 * Mouse up handler.
 * @param {MouseEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onMouseUp = function(e) {
  var position = ImageEditor.MouseControl.getPosition_(e);

  if (!this.dragHappened_) {
    this.buffer_.onClick(position.x, position.y);
  }
  this.dragHandler_ = null;
  this.dragHappened_ = false;
  this.lockMouse_(false);
  e.preventDefault();
};

/**
 * Mouse move handler.
 * @param {MouseEvent} e Event.
 */
ImageEditor.MouseControl.prototype.onMouseMove = function(e) {
  var position = ImageEditor.MouseControl.getPosition_(e);

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

/**
 * Update the UI to reflect mouse drag state.
 * @param {boolean} on True if dragging.
 * @private
 */
ImageEditor.MouseControl.prototype.lockMouse_ = function(on) {
  ImageUtil.setAttribute(this.rootContainer_, 'mousedrag', on);
};

/**
 * Update the cursor.
 *
 * @param {object} position An object holding x and y properties.
 * @private
 */
ImageEditor.MouseControl.prototype.updateCursor_ = function(position) {
  var oldCursor = this.container_.getAttribute('cursor');
  var newCursor = this.buffer_.getCursorStyle(
      position.x, position.y, !!this.dragHandler_);
  if (newCursor != oldCursor)  // Avoid flicker.
    this.container_.setAttribute('cursor', newCursor);
};

/**
 * A toolbar for the ImageEditor.
 * @param {HTMLElement} parent The parent element.
 * @param {function} displayStringFunction A string formatting function.
 * @param {function} updateCallback The callback called when controls change.
 * @constructor
 */
ImageEditor.Toolbar = function(parent, displayStringFunction, updateCallback) {
  this.wrapper_ = parent;
  this.displayStringFunction_ = displayStringFunction;
  this.updateCallback_ = updateCallback;
};

/**
 * Clear the toolbar.
 */
ImageEditor.Toolbar.prototype.clear = function() {
  ImageUtil.removeChildren(this.wrapper_);
};

/**
 * Create a control.
 * @param {string} tagName The element tag name.
 * @return {HTMLElement} The created control element.
 * @private
 */
ImageEditor.Toolbar.prototype.create_ = function(tagName) {
  return this.wrapper_.ownerDocument.createElement(tagName);
};

/**
 * Add a control.
 * @param {HTMLElement} element The control to add.
 * @return {HTMLElement} The added element.
 */
ImageEditor.Toolbar.prototype.add = function(element) {
  this.wrapper_.appendChild(element);
  return element;
};

/**
 * Add a text label.
 * @param {string} name Label name.
 * @return {HTMLElement} The added label.
 */
ImageEditor.Toolbar.prototype.addLabel = function(name) {
  var label = this.create_('span');
  label.textContent = this.displayStringFunction_(name);
  return this.add(label);
};

/**
 * Add a button.
 * @param {string} name Button name.
 * @param {function} handler onClick handler.
 * @param {string} opt_class Extra class name.
 * @return {HTMLElement} The added button.
 */
ImageEditor.Toolbar.prototype.addButton = function(
    name, handler, opt_class) {
  var button = this.create_('div');
  button.classList.add('button');
  if (opt_class) button.classList.add(opt_class);
  button.textContent = this.displayStringFunction_(name);
  button.addEventListener('click', handler, false);
  return this.add(button);
};

/**
 * Add a range control (scalar value picker).
 *
 * @param {string} name An option name.
 * @param {number} min Min value of the option.
 * @param {number} value Default value of the option.
 * @param {number} max Max value of the options.
 * @param {number} scale A number to multiply by when setting
 *                       min/value/max in DOM.
 * @param {boolean} opt_showNumeric True if numeric value should be displayed.
 * @return {HTMLElement} Range element.
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

/**
 * @return {object} options A map of options.
 */
ImageEditor.Toolbar.prototype.getOptions = function() {
  var values = {};
  for (var child = this.wrapper_.firstChild; child; child = child.nextSibling) {
    if (child.name)
      values[child.name] = child.getValue();
  }
  return values;
};

/**
 * Reset the toolbar.
 */
ImageEditor.Toolbar.prototype.reset = function() {
  for (var child = this.wrapper_.firstChild; child; child = child.nextSibling) {
    if (child.reset) child.reset();
  }
};

/**
 * Show/hide the toolbar.
 * @param {boolean} on True if show.
 */
ImageEditor.Toolbar.prototype.show = function(on) {
  if (!this.wrapper_.firstChild)
    return;  // Do not show empty toolbar;

  this.wrapper_.hidden = !on;
};

/** A prompt panel for the editor.
 *
 * @param {HTMLElement} container Container element.
 * @param {function} displayStringFunction A formatting function.
 */
ImageEditor.Prompt = function(container, displayStringFunction) {
  this.container_ = container;
  this.displayStringFunction_ = displayStringFunction;
};

/**
 * Reset the prompt.
 */
ImageEditor.Prompt.prototype.reset = function() {
  this.cancelTimer();
  if (this.wrapper_) {
    this.container_.removeChild(this.wrapper_);
    this.wrapper_ = null;
    this.prompt_ = null;
  }
};

/**
 * Cancel the delayed action.
 */
ImageEditor.Prompt.prototype.cancelTimer = function() {
  if (this.timer_) {
    clearTimeout(this.timer_);
    this.timer_ = null;
  }
};

/**
 * Schedule the delayed action.
 * @param {function} callback Callback.
 * @param {number} timeout Timeout.
 */
ImageEditor.Prompt.prototype.setTimer = function(callback, timeout) {
  this.cancelTimer();
  var self = this;
  this.timer_ = setTimeout(function() {
    self.timer_ = null;
    callback();
  }, timeout);
};

/**
 * Show the prompt.
 *
 * @param {string} text The prompt text.
 * @param {number} timeout Timeout in ms.
 * @param {object} formatArgs varArgs for the formatting fuction.
 */
ImageEditor.Prompt.prototype.show = function(text, timeout, formatArgs) {
  this.showAt.apply(this,
      ['center'].concat(Array.prototype.slice.call(arguments)));
};

/**
 *
 * @param {string} pos The 'pos' attribute value.
 * @param {string} text The prompt text.
 * @param {number} timeout Timeout in ms.
 * @param {object} formatArgs varArgs for the formatting fuction.
 */
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

/**
 * Hide the prompt.
 */
ImageEditor.Prompt.prototype.hide = function() {
  if (!this.prompt_) return;
  this.prompt_.setAttribute('state', 'fadeout');
  // Allow some time for the animation to play out.
  this.setTimer(this.reset.bind(this), 500);
};
