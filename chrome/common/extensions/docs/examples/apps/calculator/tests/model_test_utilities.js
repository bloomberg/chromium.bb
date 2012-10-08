/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The utility functions defined in this file allow tests like the following:
 *
 *   test('Two Plus Two', function () {
 *     var model = new Model();
 *     deepEqual(model.handle('2'), [null, null, '2'], '2');
 *     deepEqual(model.handle('+'), ['2', '+', null], '+');
 *     deepEqual(model.handle('2'), ['2', '+', '2'], '2');
 *     deepEqual(model.handle('='), ['4', '=', null], '=');
 *   }
 *
 * to be more succinctly written as:
 *
 *   testModel('Two Plus Two', '2 + 2 = [4]');
 */

/**
 * Describes models, strings, numbers, and arrays used in tests.
 */
var describeTested = function(strings, object, suffix) {
  if (object === null) {
    strings.push('null');
  } else if (Array.isArray(object)) {
    strings.push('[');
    for (var i = 0; i < object.length; ++i) {
      describeTested(strings, object[i], (i + 1 < object.length) ? ', ' : ']');
    }
  } else if (typeof object === 'number') {
    strings.push('#');
    strings.push(String(object));
  } else if (typeof object === 'String') {
    strings.push('"');
    strings.push(object);
    strings.push('"');
  } else if (typeof object === 'object') {
    strings.push('(');
    describeTested(strings, object.accumulator, ' ');
    describeTested(strings, object.operator, ' ');
    describeTested(strings, object.operand, ' | ');
    describeTested(strings, object.defaults && object.defaults.operator, ' ');
    describeTested(strings, object.defaults && object.defaults.operand, ')');
  } else {
    strings.push(String(object));
  }
  strings.push(suffix || '');
  return strings;
}

/**
 * Tests how a calculator model handles a single event, logging the state of the
 * model before and after the test.
 */
var testEvent = function (model, event, expected) {
  var before = describeTested([], model).join('');
  var results = model.handle(event);
  var accumulator = results.accumulator;
  var operator = results.operator;
  var operand = results.operand;
  var actual = [accumulator, operator, operand && [operand]];
  var after = describeTested(describeTested([], actual, ' '), model).join('');
  deepEqual(actual, expected, event + ' ' + before + ' => ' + after);
}

/**
 * Tests how a calculator model handles a sequence of digits and periods,
 * updating the expected operand values before each digit and period event is
 * tested according to these rules:
 *
 *   - If the passed in expected values array has a null expected operand array,
 *     the expected operand used for the tests starts with the first digit or
 *     period of the sequence of number events and the following digits and
 *     periods of that sequence are appended before each new event's test.
 *
 *   - If the passed in expected values array has an expected operand array of
 *     the form [operand], the expected operand used for the tests start with
 *     the first character of that array's element and one additional character
 *     of that element is added before each of the following digit and period
 *     event tests.
 *
 *   - If the passed in expected values array has an expected operand array of
 *     the form [prefix, operand], the expected operand used for the tests
 *     starts with the first element in that array and the first character of
 *     the second element, and one character of that second element is added
 *     before each of the following digit and period event tests.
 *
 *   - In all of these cases, leading zeros and occurences of the '=' character
 *     in the expected operand are ignored.
 *
 * For example the sequence of calls:
 *
 *   testDigits(model, '00', [x, y, ['0=']])
 *   testDigits(model, '1.2.3', [x, y, ['1.2=3']])
 *   testDigits(model, '45', [x, y, ['1.23', '45']])
 *
 * would yield the following tests:
 *
 *   deepEqual(model.handle('0'), [x, y, '0'], '0');
 *   deepEqual(model.handle('0'), [x, y, '0'], '0');
 *   deepEqual(model.handle('1'), [x, y, '1'], '1');
 *   deepEqual(model.handle('.'), [x, y, '1.'], '.');
 *   deepEqual(model.handle('2'), [x, y, '1.2'], '2');
 *   deepEqual(model.handle('.'), [x, y, '1.2'], '.');
 *   deepEqual(model.handle('3'), [x, y, '1.23'], '3');
 *   deepEqual(model.handle('4'), [x, y, '1.234'], '4');
 *   deepEqual(model.handle('5'), [x, y, '1.2345'], '5');
 *
 * and changes the expected value array to the following:
 *
 *   [x, y, ['1.2345']]
 */
var testNumber = function (model, number, expected) {
  var multiple = (expected[2] && expected[2].length > 1);
  var prefix = multiple ? expected[2][0] : '';
  var suffix = expected[2] ? expected[2][multiple ? 1 : 0] : number;
  for (var i = 0; i < number.length; ++i) {
    expected[2] = [prefix + suffix.slice(0, i + 1)];
    expected[2] = [expected[2][0].replace(/^0+([0-9])/, '$1')];
    expected[2] = [expected[2][0].replace(/=/g, '')];
    this.testEvent(model, number[i], expected);
  }
};

/**
 * Tests how a new calculator model handles a sequence of numbers, operations,
 * and commands, verifying that the model has expected accumulator, operator,
 * and operand values after each event handled by the model.
 *
 * Within the sequence string, expected values must be specified as arrays of
 * the form described below. The strings '~', '<', and 'A' will be interpreted
 * as the commands '+ / -', 'back', and 'AC' respectively, and other strings
 * will be interpreted as the corresponding digits, periods, operations, and
 * commands.
 *
 * Expected value arrays must have one of the following forms:
 *
 *   [accumulator]
 *   [accumulator operator]
 *   [accumulator operator [operand]]
 *   [accumulator operator [prefix operand]]
 *   [[operand]]
 *   [[prefix operand]]
 *
 * where |accumulator| is a number or |null|, |operator| one of the operator
 * character or |null|, |prefix| a number, and |operand| also a number. Both
 * |prefix| and |operand| numbers may contain leading zeros and their use is
 * described in the comments for the |testNumber()| method above. Expected value
 * arrays must be specified just after the sequence element which they are meant
 * to test, like this:
 *
 *   testCalculation(model, '12 - 34 = [-22]')
 *
 * When expected values are not specified for an element, the following rules
 * are applied to obtain best guesses for the expected values for that
 * element's tests:
 *
 *   - The initial expected values are:
 *
 *       [null, null, null].
 *
 *   - If the element being tested is a number, the expected operand values are
 *     set and changed as described in the comments for the |testNumber()|
 *     method above.
 *
 *   - If the element being tested is the '+ / -' operation the expected values
 *     will be changed as follows:
 *
 *       [x,    y, null]    -> [x, y, null]
 *       [x,    y, [z]]     -> [x, y, [-z]]
 *       [x,    y, [z1 z2]] -> [x, y, ['-' + z1 + z2]]
 *
 *   - If the element |e| being tested is the '+', '-', '*', or '/' operation
 *     the expected values will be changed as follows:
 *
 *       [null, y, null]    -> [0,            e, null]
 *       [x,    y, null]    -> [x,            e, null]
 *       [x,    y, [z]]     -> [z,            e, null]
 *       [x,    y, [z1 z2]] -> ['' + z1 + z2, e, null]
 *
 *   - If the element being tested is the '=' command, the expected values will
 *     be changed as for the '+' operation except that the expected operator
 *     value will be set to null.
 *
 * So for example this call:
 *
 *   testModel('My Test', '12 + 34 - [46] 56 = [-10] 0')
 *
 * would yield the following tests:
 *
 *   test('My Test', function () {
 *     var model = new Model();
 *     deepEqual(model.handle('1'), ['0',  null, '1'],  '1');
 *     deepEqual(model.handle('2'), ['0',  null, '12'], '2');
 *     deepEqual(model.handle('+'), ['12', '+',  null], '+');
 *     deepEqual(model.handle('3'), ['12', '+',  '3'],  '3');
 *     deepEqual(model.handle('4'), ['12', '+',  '34'], '4');
 *     deepEqual(model.handle('-'), ['46', '-',  null], '-');
 *     deepEqual(model.handle('2'), ['46', '-',  '2']), '2';
 *     deepEqual(model.handle('1'), ['46', '-',  '21'], '1');
 *     deepEqual(model.handle('='), ['25', '=',  null], '=');
 *     deepEqual(model.handle('0'), ['25', null, '0'],  '0');
 *   });
 */
var testModel = function (name, sequence) {
  // Define constants and variables for matching.
  var NUMBER = /(-?[\d.][\d.=]*)|(E)/g;                        // '2'
  var OPERATION = /([+*/-])/g;                                 // '+'
  var COMMAND = /([=~<CA])/g;                                  // '='
  var VALUES = /(\[[^\[\]]*(\[[^\[\]]+\])?\])/g;               // '[x, y, [z]]'
  var ABBREVIATIONS = {'~': '+ / -', '<': 'back', 'A': 'AC'};  // '~'
  var match, before, after, replacement;
  // Parse the sequence into JSON array contents, so '1 - 2 = [-1]' becomes...
  sequence = sequence.replace(NUMBER, ',$1$2,');   // ',2, + ,2, = [,4,]'
  sequence = sequence.replace(OPERATION, ',$1,');  // ',2, ,+, ,2,= [,4,]'
  sequence = sequence.replace(COMMAND, ',$1,');    // ',2, ,+, ,2, ,=, [,4,]'
  sequence = sequence.replace(VALUES, ',$1,');     // ',2, ,+, ,2, ,=, ,[,4,],'
  sequence = sequence.replace(/(null)/g, ',$1,');  // ',2, ,+, ,2, ,=, ,[,4,],'
  sequence = sequence.replace(/\s+/g, '');         // ',2,,+,,2,,=,,[,4,],'
  sequence = sequence.replace(/,,+/g, ',');        // ',2,+,2,=,[,4,],'
  sequence = sequence.replace(/\[,/g, '[');        // ',2,+,2,=,[4,],'
  sequence = sequence.replace(/,\]/g, ']');        // ',2,+,2,=,[4],'
  sequence = sequence.replace(/(^,)|(,$)/g, '');   // '2,+,2,=,[4]'
  sequence = sequence.replace(NUMBER, '"$1$2"');   // '"2",+,"2",=,["4"]'
  sequence = sequence.replace(OPERATION, '"$1"');  // '"2","+","2",=,["4"]'
  sequence = sequence.replace(COMMAND, '"$1"');    // '"2","+","2","=",["4"]'
  // Fix some expected value characters parse errors. For example the original
  // sequences '[[0=]]" and "[-1]' would have become '[["0","="]]' and
  // '["-","1"]' and would need to be fixed to '[["0="]]' and '["-1"]'.
  while ((match = VALUES.exec(sequence)) != null) {
    before = sequence.slice(0, match.index);
    after = sequence.slice(match.index + match[0].length);
    replacement = match[0].replace(/","=/g, '=').replace(/=","/g, '=');
    replacement = replacement.replace(/-","/g, '-');
    sequence = before + replacement + after;
    VALUES.lastIndex = match.index + replacement.length;
  }
  // Convert the sequence to an array and run the test.
  sequence = JSON.parse('[' + sequence + ']');     // ['2','-','2','=',['4']]
  test(name, function () {
    var model = new Model();
    var expected = [null, null, null];
    for (var i = 0; i < sequence.length; ++i) {
      // Peek ahead to get expected value, then test the current sequence
      // element, skipping any expected values already processed.
      var next = sequence[i + 1] && sequence[i + 1];
      var current = sequence[i];
      if (!Array.isArray(current)) {
        // Apply expected value rules.
        var operand = expected[2] && expected[2].join('');
        if (current.match(OPERATION)) {
          expected = [operand || expected[0] || '0', current, null];
        } else if (current === '=') {
          expected = [operand || expected[0] || '0', null, null];
        } else if (current === '~' && operand) {
          expected[2] = ['-' + operand];
        }
        // Adjust expected values with explicitly specified ones.
        if (Array.isArray(next) && Array.isArray(next[0])) {
          expected = expected.slice(0, 2).concat([next[0]]).slice(0, 3);
        } else if (Array.isArray(next)) {
          expected = next.concat(expected.slice(next.length)).slice(0, 3);
        }
        // Test.
        if (current.match(NUMBER)) {
          testNumber(model, current, expected);
        } else {
          this.testEvent(model, ABBREVIATIONS[current] || current, expected);
        }
      };
    }
  }.bind(this));
}
