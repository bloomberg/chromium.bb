/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 **/

function View(window, model) {
  var view = this;
  var model = model;
  var events = this.defineEvents_();
  var display = window.document.querySelector('#display');
  var buttons = window.document.querySelectorAll('#buttons .button');

  Array.prototype.forEach.call(buttons, function (button) {
    button.onclick = function (click) {
      var button = click.target.getAttribute('class').split(' ')[1];
      var event = events.byButton[button];
      if (event) {
        var values = model.handle(event.name);
        view.updateDisplay_(display, values, event);
      }
    };
  });

  window.onkeydown = function (keydown) {
    var key = keydown.shiftKey ? ('^' + keydown.which) : keydown.which;
    var event = events.byKey[key];
    if (event) {
      var values = model.handle(event.name);
      view.updateDisplay_(display, values, event);
    }
  };

}

/** @private */
View.prototype.defineEvents_ = function () {
  var events = {byName: {}, byButton: {}, byKey: {}};
  this.defineEvent_(events, '0', 'zero', '48');
  this.defineEvent_(events, '1', 'one', '49');
  this.defineEvent_(events, '2', 'two', '50');
  this.defineEvent_(events, '3', 'three', '51');
  this.defineEvent_(events, '4', 'four', '52');
  this.defineEvent_(events, '5', 'five', '53');
  this.defineEvent_(events, '6', 'six', '54');
  this.defineEvent_(events, '7', 'seven', '55');
  this.defineEvent_(events, '8', 'eight', '56');
  this.defineEvent_(events, '9', 'nine', '57');
  this.defineEvent_(events, '.', 'point', '190');
  this.defineEvent_(events, '+', 'add', '^187', true);
  this.defineEvent_(events, '-', 'subtract', '189', true);
  this.defineEvent_(events, '*', 'multiply', '^56', true);
  this.defineEvent_(events, '/', 'divide', '191', true);
  this.defineEvent_(events, '=', 'equals', '187');
  this.defineEvent_(events, '=', ' ', '13');
  this.defineEvent_(events, '+ / -', 'negate', '32');
  this.defineEvent_(events, 'AC', 'clear', '67');
  this.defineEvent_(events, 'back', ' ', '8');
  return events;
}

/** @private */
View.prototype.defineEvent_ = function (events, name, button, key, operator) {
  var event = {name: name, button: button, key: key, operator: !!operator};
  events.byButton[button] = event;
  events.byKey[key] = event;
};

/** @private */
View.prototype.updateDisplay_ = function (display, values, event) {
  var operation = {operator: values.operator, operand: values.operand};
  var result = values.accumulator;
  if (event.name === 'AC') {
    display.innerHTML = '';
    this.addEquation_(display, {operand: '0'});
  } else if (event.name === '=') {
    display.appendChild(this.createDiv_(display, 'hr'));
    this.addEquation_(display, {accumulator: result, operand:result});
  } else if (event.operator) {
    this.updateEquation_(display.lastElementChild, {accumulator: result});
    this.addEquation_(display, {operator: operation.operator});
  } else if (!this.updateEquation_(display.lastElementChild, operation)) {
    this.addEquation_(display, operation);
  }
}

/** @private */
View.prototype.addEquation_ = function (display, values) {
  // The order of the equation children below makes them stack correctly.
  var equation = this.createDiv_(display, 'equation');
  equation.appendChild(this.createDiv_(display, 'operand'));
  equation.appendChild(this.createDiv_(display, 'operator'));
  equation.appendChild(this.createDiv_(display, 'accumulator'));
  this.updateEquation_(equation, values);
  display.appendChild(equation).scrollIntoView();
}

/** @private */
View.prototype.updateEquation_ = function (equation, values) {
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
View.prototype.updateValue_ = function (element, value, zero) {
  if (value !== undefined) {
    element.textContent = zero ? (value || '0') : (value || '');
  }
}

/** @private */
View.prototype.createDiv_ = function (display, classes) {
  var div = display.ownerDocument.createElement('div');
  div.setAttribute('class', classes);
  return div;
}
