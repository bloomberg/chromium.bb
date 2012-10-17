/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 **/

function View(window, model) {
  var view = this;
  var model = model;
  var inputs = this.defineInputs_();
  var display = window.document.querySelector('#calculator .display');
  var buttons = window.document.querySelectorAll('#calculator .button');

  Array.prototype.forEach.call(buttons, function(button) {
    button.onclick = function(event) {
      var button = event.target.getAttribute('class').split(' ')[1];
      var input = inputs.byButton[button];
      if (input) {
        var values = model.handle(input.name);
        view.updateDisplay_(display, values, input);
      }
    };
  });

  window.onkeydown = function(event) {
    var key = event.shiftKey ? ('^' + event.which) : event.which;
    var input = inputs.byKey[key];
    if (input) {
      var values = model.handle(input.name);
      view.updateDisplay_(display, values, input);
    }
  };

}

/** @private */
View.prototype.defineInputs_ = function() {
  var inputs = {byName: {}, byButton: {}, byKey: {}};
  this.defineInput_(inputs, '0', 'zero', '48');
  this.defineInput_(inputs, '1', 'one', '49');
  this.defineInput_(inputs, '2', 'two', '50');
  this.defineInput_(inputs, '3', 'three', '51');
  this.defineInput_(inputs, '4', 'four', '52');
  this.defineInput_(inputs, '5', 'five', '53');
  this.defineInput_(inputs, '6', 'six', '54');
  this.defineInput_(inputs, '7', 'seven', '55');
  this.defineInput_(inputs, '8', 'eight', '56');
  this.defineInput_(inputs, '9', 'nine', '57');
  this.defineInput_(inputs, '.', 'point', '190');
  this.defineInput_(inputs, '+', 'add', '^187', true);
  this.defineInput_(inputs, '-', 'subtract', '189', true);
  this.defineInput_(inputs, '*', 'multiply', '^56', true);
  this.defineInput_(inputs, '/', 'divide', '191', true);
  this.defineInput_(inputs, '=', 'equals', '187');
  this.defineInput_(inputs, '=', ' ', '13');
  this.defineInput_(inputs, '+ / -', 'negate', '32');
  this.defineInput_(inputs, 'AC', 'clear', '67');
  this.defineInput_(inputs, 'back', ' ', '8');
  return inputs;
}

/** @private */
View.prototype.defineInput_ = function(inputs, name, button, key, operator) {
  var input = {name: name, button: button, key: key, operator: !!operator};
  inputs.byButton[button] = input;
  inputs.byKey[key] = input;
};

/** @private */
View.prototype.updateDisplay_ = function(display, values, input) {
  var operation = {operator: values.operator, operand: values.operand};
  var result = values.accumulator;
  if (input.name === 'AC') {
    display.innerHTML = '';
    this.addEquation_(display, {operand: '0'});
  } else if (input.name === '=') {
    display.appendChild(this.createDiv_(display, 'hr'));
    this.addEquation_(display, {accumulator: result, operand:result});
  } else if (input.operator) {
    this.updateEquation_(display.lastElementChild, {accumulator: result});
    this.addEquation_(display, {operator: operation.operator});
  } else if (!this.updateEquation_(display.lastElementChild, operation)) {
    this.addEquation_(display, operation);
  }
}

/** @private */
View.prototype.addEquation_ = function(display, values) {
  // The order of the equation children below makes them stack correctly.
  var equation = this.createDiv_(display, 'equation');
  var operation = this.createDiv_(display, 'operation');
  operation.appendChild(this.createSpan_(display, 'operator'));
  operation.appendChild(this.createSpan_(display, 'operand'));
  equation.appendChild(operation);
  var accumulator = this.createDiv_(display, 'accumulator');
  accumulator.setAttribute('aria-hidden', 'true');
  equation.appendChild(accumulator);
  this.updateEquation_(equation, values);
  display.appendChild(equation).scrollIntoView();
}

/** @private */
View.prototype.updateEquation_ = function(equation, values) {
  // Equations which are "finalized" (which have an accumulator value) shouldn't
  // and won't be updated, and this method returns whether an update occurred.
  var accumulator = equation && equation.querySelector('.accumulator');
  var operator = equation && equation.querySelector('.operator');
  var operand = equation && equation.querySelector('.operand');
  var update = accumulator && !accumulator.textContent;
  if (update) {
    this.updateValue_(accumulator, values.accumulator);
    this.updateValue_(operator, values.operator);
    this.updateValue_(operand, values.operand, !operator.text);
    equation.scrollIntoView();
  }
  return update;
}

/** @private */
View.prototype.updateValue_ = function(element, value, zero) {
  if (value !== undefined) {
    element.textContent = zero ? (value || '0') : (value || '');
  }
}

/** @private */
View.prototype.createDiv_ = function(display, classes) {
  var div = display.ownerDocument.createElement('div');
  div.setAttribute('class', classes);
  return div;
}

/** @private */
View.prototype.createSpan_ = function(display, classes) {
  var span = display.ownerDocument.createElement('span');
  span.setAttribute('class', classes);
  return span;
}
