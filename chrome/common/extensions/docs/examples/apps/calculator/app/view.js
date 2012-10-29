/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 **/

function View(window) {
  var buttons = window.document.querySelectorAll('#calculator .buttons button');
  Array.prototype.forEach.call(buttons, function(button) {
    button.onclick = function(event) {
      var button = event.target.dataset.button;
      if (this.onButton)
        this.onButton.call(this, button);
    }.bind(this);
  }, this);
  this.display = window.document.querySelector('#calculator .display');
  window.onkeydown = function(event) {
    var key = event.shiftKey ? ('^' + event.which) : event.which;
    if (this.onKey)
      this.onKey.call(this, key);
  }.bind(this);
}

View.prototype.clearDisplay = function(values) {
  this.display.innerHTML = '';
  this.addValues(values);
};

View.prototype.addResults = function(values) {
  this.appendChild_(this.display, 'div', 'hr');
  this.addValues(values);
};

View.prototype.addValues = function(values) {
  var equation = this.makeElement_('div', 'equation');
  this.appendChild_(equation, 'div', 'accumulator', values.accumulator);
  this.appendChild_(equation, 'div', 'operation');
  this.appendChild_(equation.children[1], 'span', 'operator', values.operator);
  this.appendChild_(equation.children[1], 'span', 'operand', values.operand);
  this.setAttribute_(equation, '.accumulator', 'aria-hidden', 'true');
  this.display.appendChild(equation).scrollIntoView();
};

View.prototype.setValues = function(values) {
  var equation = this.display.lastElementChild;
  this.setContent_(equation, '.accumulator', values.accumulator || '');
  this.setContent_(equation, '.operator', values.operator || '');
  this.setContent_(equation, '.operand', values.operand || '');
};

View.prototype.getValues = function() {
  var equation = this.display.lastElementChild;
  return {
    accumulator: this.getContent_(equation, '.accumulator') || null,
    operator: this.getContent_(equation, '.operator') || null,
    operand: this.getContent_(equation, '.operand') || null,
  };
};

/** @private */
View.prototype.makeElement_ = function(tag, classes, content) {
  var element = this.display.ownerDocument.createElement(tag);
  element.setAttribute('class', classes);
  element.textContent = content || '';
  return element;
};

/** @private */
View.prototype.appendChild_ = function(parent, tag, classes, content) {
  parent.appendChild(this.makeElement_(tag, classes, content));
};

/** @private */
View.prototype.setAttribute_ = function(root, selector, name, value) {
  var element = root && root.querySelector(selector);
  if (element)
    element.setAttribute(name, value);
};

/** @private */
View.prototype.setContent_ = function(root, selector, content) {
  var element = root && root.querySelector(selector);
  if (element)
    element.textContent = content || '';
};

/** @private */
View.prototype.getContent_ = function(root, selector) {
  var element = root && root.querySelector(selector);
  return element ? element.textContent : null;
};
