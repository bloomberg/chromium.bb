/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 **/

function View(model) {
  var view = this;
  var model = model;
  var events = this.defineEvents_();

  this.buttonsElement = $('#calculator #buttons');
  this.displayElement = $('#calculator #display');
  this.keyPrefix = '';

  $('#calculator #buttons .button').click(function (event) {
    var button = $(event.target).attr('class').split(' ')[1];
    view.handleEvent_(model, events.byButton[button]);
  });

  $(document).keydown(function (event) {
    var key = view.keyPrefix + event.which;
    if (key === '16') {
      view.keyPrefix = '^'
    } else {
      view.handleEvent_(model, events.byKey[key]);
    }
  });

  $(document).keyup(function (event) {
    if (event.which === '16') {
      view.keyPrefix = '';
    }
  });
}

/** @private */
View.prototype.defineEvents_ = function () {
  var events = {byName: {}, byButton: {}, byKey: {}};
  this.defineEvent_(events, '0',     'zero',     '48');
  this.defineEvent_(events, '1',     'one',      '49');
  this.defineEvent_(events, '2',     'two',      '50');
  this.defineEvent_(events, '3',     'three',    '51');
  this.defineEvent_(events, '4',     'four',     '52');
  this.defineEvent_(events, '5',     'five',     '53');
  this.defineEvent_(events, '6',     'six',      '54');
  this.defineEvent_(events, '7',     'seven',    '55');
  this.defineEvent_(events, '8',     'eight',    '56');
  this.defineEvent_(events, '9',     'nine',     '57');
  this.defineEvent_(events, '.',     'point',    '190');
  this.defineEvent_(events, '+',     'add',      '^187', true);
  this.defineEvent_(events, '-',     'subtract', '189', true);
  this.defineEvent_(events, '*',     'multiply', '^56', true);
  this.defineEvent_(events, '/',     'divide',   '191', true);
  this.defineEvent_(events, '=',     'equals',   '187');
  this.defineEvent_(events, '=',     ' ',        '13');
  this.defineEvent_(events, '+ / -', 'negate',   '32');
  this.defineEvent_(events, 'AC',    'clear',    '67');
  this.defineEvent_(events, 'back',  ' ',        '8');
  return events;
}

/** @private */
View.prototype.defineEvent_ = function (events, name, button, key, operator) {
  var event = {name: name, button: button, key: key, operator: !!operator};
  events.byButton[button] = event;
  events.byKey[key] = event;
};

/** @private */
View.prototype.handleEvent_ = function (model, event) {
  if (event) {
    var results = model.handle(event.name);
    console.log('results:', JSON.stringify(results));
    if (!results.accumulator && !results.operator && !results.operand) {
      this.displayElement.empty();
      this.addDisplayEquation_(results);
    } else if (event.name === '=') {
      results = {accumulator: results.accumulator, operand:results.accumulator};
      this.displayElement.append('<div class="hr"></div>');
      this.addDisplayEquation_(results);
    } else if (event.operator) {
      this.updateLastDisplayEquation_({accumulator: results.accumulator});
      this.addDisplayEquation_({operator: results.operator});
    } else {
      results = {operator: results.operator, operand: results.operand};
      if (!this.updateLastDisplayEquation_(results)) {
        this.addDisplayEquation_(results);
      }
    }
  }
}

/** @private */
View.prototype.addDisplayEquation_ = function (values) {
  console.log('add:', JSON.stringify(values));
  var zero = (!values.accumulator && !values.operator);
  var operand = values.operand || (zero ? '0' : '');
  this.displayElement.append(
      '<div class="equation">' +
        '<div class="operand">' + operand + '</div>' +
        '<div class="operator">' + (values.operator || '') + '</div>' +
        '<div class="accumulator">' + (values.accumulator || '') + '</div>' +
      '</div>');
  this.displayElement.scrollTop(this.displayElement[0].scrollHeight);
}

/** @private */
View.prototype.updateLastDisplayEquation_ = function (values) {
  var equation = this.displayElement.find('.equation').last();
  var accumulator = equation.find('.accumulator');
  var operator = equation.find('.operator');
  var operand = equation.find('.operand');
  var update = !accumulator.text();
  if (update) {
    if (values.accumulator !== undefined) {
      accumulator.text(values.accumulator || '');
    }
    if (values.operator !== undefined) {
      operator.text(values.operator || '');
    }
    if (values.operand !== undefined) {
      operand.text(values.operand || (operator.text() ? '' : '0'));
    }
  }
  return update;
}
