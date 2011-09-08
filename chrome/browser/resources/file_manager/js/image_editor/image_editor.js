// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ImageEditor is the top level object that holds together and connects
 * everything needed for image editing.
 * @param {HTMLElement} container
 * @param {function(Blob)} saveCallback
 * @param {function()} closeCallback
 * @param {HTMLElement} toolbarContainer
 */
function ImageEditor(container, toolbarContainer, saveCallback, closeCallback) {
  this.container_ = container;
  this.saveCallback_ = saveCallback;
  this.closeCallback_ = closeCallback;

  this.container_.innerHTML = '';

  var document = this.container_.ownerDocument;

  this.canvasWrapper_ = document.createElement('div');
  this.canvasWrapper_.className = 'canvas-wrapper';
  container.appendChild(this.canvasWrapper_);

  var canvas = document.createElement('canvas');
  this.canvasWrapper_.appendChild(canvas);
  canvas.width = this.canvasWrapper_.clientWidth;
  canvas.height = this.canvasWrapper_.clientHeight;

  this.buffer_ = new ImageBuffer(canvas);

  // TODO(dgozman): consider adding a ScaleControl in v2.

  this.panControl_ = new ImageEditor.MouseControl(canvas, this.getBuffer());

  this.toolbar_ = new ImageEditor.Toolbar(toolbarContainer,
      this.onOptionsChange.bind(this));
  this.initToolbar();
}

/**
 * Create an ImageEditor instance bound to a current web page, load the content.
 *
 * Use this method when image_editor.html is loaded into an iframe.
 *
 * @param {function(Blob)} saveCallback
 * @param {function()} closeCallback
 * @param {HTMLCanvasElement|HTMLImageElement|String} source
 * @param {Object} opt_metadata
 * @return {ImageEditor}
 */
ImageEditor.open = function(saveCallback, closeCallback, source, opt_metadata) {
  var container = document.getElementsByClassName('image-editor')[0];
  var toolbar = document.getElementsByClassName('toolbar-container')[0];
  var editor = new ImageEditor(container, toolbar, saveCallback, closeCallback);
  if (ImageEditor.resizeListener) {
    // Make sure we do not leak the previous instance.
    window.removeEventListener('resize', ImageEditor.resizeListener, false);
  }
  ImageEditor.resizeListener = editor.resizeFrame.bind(editor);
  window.addEventListener('resize', ImageEditor.resizeListener, false);
  editor.load(source, opt_metadata);
  return editor;
};

/**
 * Loads a new image and its metadata.
 * @param {HTMLCanvasElement|HTMLImageElement|String} source
 * @param {Object} opt_metadata
 */
ImageEditor.prototype.load = function(source, opt_metadata) {
  this.onModeCancel();
  this.getBuffer().load(source);
  this.metadata_ = opt_metadata;
};

/**
 * Window resize handler.
 */
ImageEditor.prototype.resizeFrame = function() {
  this.getBuffer().resizeScreen(
      this.canvasWrapper_.clientWidth, this.canvasWrapper_.clientHeight, true);
};

/**
 * @return {ImageBuffer}
 */
ImageEditor.prototype.getBuffer = function () {
  return this.buffer_;
};

/**
 * Destroys the UI and calls the close callback.
 */
ImageEditor.prototype.close = function() {
  this.container_.innerHTML = '';
  this.closeCallback_();
};

/**
 * Encode the current image into a blob and pass it to the save callback.
 */
ImageEditor.prototype.save = function() {
  this.saveCallback_(ImageEncoder.getBlob(
      this.getBuffer().getContent().getCanvas(), 'image/jpeg', this.metadata_));
};

ImageEditor.prototype.onOptionsChange = function(options) {
  ImageUtil.trace.resetTimer('update');
  if (this.currentMode_)
    this.currentMode_.update(options);
  ImageUtil.trace.reportTimer('update');
};

ImageEditor.prototype.initToolbar = function() {
  this.toolbar_.clear();

  this.createModeButtons();
  this.toolbar_.addButton('Save', this.save.bind(this, true));
};

/**
 * ImageEditor.Mode represents a modal state dedicated to a specific operation.
 * Inherits from ImageBuffer.Overlay to simplify the drawing of
 * mode-specific tools.
 */

ImageEditor.Mode = function(displayName) {
  this.displayName = displayName;
};

ImageEditor.Mode.prototype = {__proto__: ImageBuffer.Overlay.prototype };

ImageEditor.Mode.prototype.getBuffer = function() {
  return this.buffer_;
};

ImageEditor.Mode.prototype.repaint = function(opt_fromOverlay) {
  return this.buffer_.repaint(opt_fromOverlay);
};

ImageEditor.Mode.prototype.getViewport = function() {
  return this.viewport_;
};

ImageEditor.Mode.prototype.getContent = function() {
  return this.content_;
};

/**
 * Called after the instantiation.
 */
ImageEditor.Mode.prototype.setUp = function(buffer) {
  this.buffer_ = buffer;
  this.viewport_ = buffer.getViewport();
  this.content_ = buffer.getContent();
  this.buffer_.addOverlay(this);
};

/**
 * Create mode-specific controls here.
 */
ImageEditor.Mode.prototype.createTools = function(toolbar) {};

/**
 * Called before exiting the mode. Do the cleanup here.
 */
ImageEditor.Mode.prototype.cleanUp = function() {
  this.buffer_.removeOverlay(this);
};

/**
 * Called when any of the controls changed its value.
 */
ImageEditor.Mode.prototype.update = function(options) {};

/**
 * The user clicked 'OK'. Finalize the change.
 */
ImageEditor.Mode.prototype.commit = function() {};

/**
 * The user clicker 'Reset' or 'Cancel'. Undo the change.
 */
ImageEditor.Mode.prototype.rollback = function() {};


ImageEditor.Mode.constructors = [];

ImageEditor.Mode.register = function(constructor) {
  ImageEditor.Mode.constructors.push(constructor);
};

ImageEditor.prototype.createModeButtons = function() {
  for (var i = 0; i != ImageEditor.Mode.constructors.length; i++) {
    var mode = new ImageEditor.Mode.constructors[i];
    this.toolbar_.
        addButton(mode.displayName, this.onModeEnter.bind(this, mode));
  }
};

/**
 * The user clicked on the mode button.
 */
ImageEditor.prototype.onModeEnter = function(mode) {
  this.toolbar_.clear();

  this.toolbar_.addLabel(mode.displayName);

  this.currentMode_ = mode;
  this.currentMode_.setUp(this.getBuffer());
  this.currentMode_.createTools(this.toolbar_);

  this.toolbar_.addButton('Reset', this.onModeReset.bind(this)),
  this.toolbar_.addButton('OK', this.onModeCancel.bind(this, true)),
  this.toolbar_.addButton('Cancel', this.onModeCancel.bind(this, false));

  this.getBuffer().repaint();
};

/**
 * The user clicked on 'Cancel'.
 */
ImageEditor.prototype.onModeCancel = function(save) {
  if (!this.currentMode_) return;

  this.currentMode_.cleanUp();
  if (save) {
    this.currentMode_.commit();
  } else {
    this.currentMode_.rollback();
  }
  this.currentMode_ = null;

  this.getBuffer().repaint();

  this.initToolbar();
};

/**
 * The user clicked on 'Reset'.
 */
ImageEditor.prototype.onModeReset = function() {
  this.toolbar_.reset();
  this.currentMode_.rollback();
  this.getBuffer().repaint();
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
ImageEditor.MouseControl = function(canvas, buffer) {
  this.canvas_ = canvas;
  this.buffer_ = buffer;
  canvas.addEventListener('mousedown', this.onMouseDown.bind(this), false);
  canvas.addEventListener('mouseup', this.onMouseUp.bind(this), false);
  canvas.addEventListener('mousemove', this.onMouseMove.bind(this), false);
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
  this.canvas_.style.cursor =
      this.buffer_.getCursorStyle(position.x, position.y, !!this.dragHandler_);
  e.preventDefault();
};

ImageEditor.MouseControl.prototype.onMouseUp = function(e) {
  var position = ImageEditor.MouseControl.getPosition(e);

  if (!this.dragHappened_) {
    this.buffer_.onClick(position.x, position.y);
  }
  this.dragHandler_ = null;
  this.dragHappened_ = false;
  e.preventDefault();
};

ImageEditor.MouseControl.prototype.onMouseMove = function(e) {
  var position = ImageEditor.MouseControl.getPosition(e);

  this.canvas_.style.cursor =
      this.buffer_.getCursorStyle(position.x, position.y, !!this.dragHandler_);
  if (this.dragHandler_) {
    this.dragHandler_(position.x, position.y);
    this.dragHappened_ = true;
  }
  e.preventDefault();
};

/**
 * A toolbar for the ImageEditor.
 * @constructor
 */
ImageEditor.Toolbar = function (parent, updateCallback) {
  this.wrapper_ = parent.ownerDocument.createElement('div');
  this.wrapper_.className = 'toolbar';
  parent.appendChild(this.wrapper_);
  this.updateCallback_ = updateCallback;
};

ImageEditor.Toolbar.prototype.clear = function() {
  this.wrapper_.innerHTML = '';
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
  label.textContent = text;
  return this.add(label);
};

ImageEditor.Toolbar.prototype.addButton = function(text, handler) {
  var button = this.create_('button');
  button.textContent = text;
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
 */
ImageEditor.Toolbar.prototype.addRange = function(
    name, min, value, max, scale) {
  var self = this;

  scale = scale || 1;

  var range = this.create_('input');

  range.type = 'range';
  range.name = name;
  range.min = Math.ceil(min * scale);
  range.max = Math.floor(max * scale);

  var label = this.create_('span');
  function mirror() {
    label.textContent = Math.round(range.getValue() * scale) / scale;
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

  var descr = this.create_('span');
  descr.textContent = name;
  this.add(descr);
  this.add(range);
  this.add(label);

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
