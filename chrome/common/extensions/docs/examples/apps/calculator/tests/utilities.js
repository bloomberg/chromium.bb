/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The utility class defined in this file allow calculator tests to be written
 * more succinctly.
 *
 * Tests that would be written with QUnit like this:
 *
 *   test('Two Plus Two', function() {
 *     var model = new Model();
 *     deepEqual(model.handle('2'), [null, null, '2'], '2');
 *     deepEqual(model.handle('+'), ['2', '+', null], '+');
 *     deepEqual(model.handle('2'), ['2', '+', '2'], '2');
 *     deepEqual(model.handle('='), ['4', '=', null], '=');
 *   }
 *
 * can instead be written as:
 *
 *   var run = calculatorTestRun.create();
 *   run.test('Two Plus Two', '2 + 2 = [4]');
 */

'use strict';

window.calculatorTestRun = {

  /**
   * Returns an object representing a run of calculator tests.
   */
  create: function() {
    var run = Object.create(this);
    run.tests = [];
    run.success = true;
    return run;
  },

  /**
   * Runs a test defined as either a sequence or a function.
   */
  test: function(name, test) {
    this.tests.push({name: name, steps: [], success: true});
    if (typeof test === 'string')
      this.testSequence_(name, test);
    else if (typeof test === 'function')
      test.call(this, new Model());
    else
      this.fail(this.getDescription_('invalid test: ', test));
  },

  /**
   * Log test failures to the console.
   */
  log: function() {
    var counts, lines, success, prefix, number, counts, i, j, k;
    if (!this.success) {
      counts = {passed: 0, failed: 0};
      lines = ['', '', ''];
      for (i = 0; i < this.tests.length; ++i) {
        success = this.tests[i].success;
        counts.passed += success ? 1 : 0;
        counts.failed += success ? 0 : 1;
        prefix = success ? 'PASS: ' : 'FAIL: ';
        number = ('0' + (i + 1)).slice(-Math.max(2, String(i + 1).length));
        lines.push('', number + ') ' + prefix + this.tests[i].name);
        for (j = 0; j < this.tests[i].steps.length; ++j) {
          prefix = this.tests[i].steps[j].success ? 'PASS: ' : 'FAIL: ';
          for (k = 0; k < this.tests[i].steps[j].messages.length; ++k) {
            lines.push('    ' + prefix + this.tests[i].steps[j].messages[k]);
            prefix = '      ';
          }
        }
      }
      lines[2] = counts.passed + ' tests passed, ' + counts.failed + ' failed:';
      lines.push('', '');
      console.log(lines.join('\n'));
    }
  },

  /**
   * Verify that actual values after a test step match expectations.
   */
  verify: function(expected, actual, message) {
    if (this.areEqual_(expected, actual))
      this.succeed(message);
    else
      this.fail(message, expected, actual);
  },

  /**
   * Record a successful test step.
   */
  succeed: function(message) {
    var test = this.tests[this.tests.length - 1];
    test.steps.push({success: true, messages: [message]});
  },

  /**
   * Fail the current test step. Expected and actual values are optional.
   */
  fail: function(message, expected, actual) {
    var test = this.tests[this.tests.length - 1];
    var failure = {success: false, messages: [message]};
    if (expected !== undefined) {
      failure.messages.push(this.getDescription_('expected: ', expected));
      failure.messages.push(this.getDescription_('actual:   ', actual));
    }
    test.steps.push(failure);
    test.success = false;
    this.success = false;
  },

  /**
   * @private
   * Tests how a new calculator model handles a sequence of numbers, operations,
   * and commands, verifying that the model has expected accumulator, operator,
   * and operand values after each input handled by the model.
   *
   * Within the sequence string, expected values must be specified as arrays of
   * the form described below. The strings '~', '<', and 'A' is interpreted as
   * the commands '+ / -', 'back', and 'AC' respectively, and other strings are
   * interpreted as the digits, periods, operations, and commands represented
   * by those strings.
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
   * described in the comments for the |testNumber_()| method above. Expected
   * value arrays must be specified just after the sequence element which they
   * are meant to test, like this:
   *
   *   run.testSequence_(model, '12 - 34 = [-22]')
   *
   * When expected values are not specified for an element, the following rules
   * are applied to obtain best guesses for the expected values for that
   * element's tests:
   *
   *   - The initial expected values are:
   *
   *       [null, null, null].
   *
   *   - If the element being tested is a number, the expected operand values
   *     are set and changed as described in the comments for the
   *     |testNumber_()| method above.
   *
   *   - If the element being tested is the '+ / -' operation the expected
   *     values will be changed as follows:
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
   *   - If the element being tested is the '=' command, the expected values
   *     will be changed as for the '+' operation except that the expected
   *     operator value will be set to null.
   *
   * So for example this call:
   *
   *   run.testSequence_('My Test', '12 + 34 - [46] 56 = [-10] 0')
   *
   * would yield the following test:
   *
   *   run.verify(model.handle('1'), ['0',  null, '1'],  '1');
   *   run.verify(model.handle('2'), ['0',  null, '12'], '2');
   *   run.verify(model.handle('+'), ['12', '+',  null], '+');
   *   run.verify(model.handle('3'), ['12', '+',  '3'],  '3');
   *   run.verify(model.handle('4'), ['12', '+',  '34'], '4');
   *   run.verify(model.handle('-'), ['46', '-',  null], '-');
   *   run.verify(model.handle('2'), ['46', '-',  '2']), '2';
   *   run.verify(model.handle('1'), ['46', '-',  '21'], '1');
   *   run.verify(model.handle('='), ['25', '=',  null], '=');
   *   run.verify(model.handle('0'), ['25', null, '0'],  '0');
   */
  testSequence_: function(name, sequence) {
    // Define constants and variables.
    var NUMBER = /(-?[\d.][\d.=]*)|(E)/g;           // -123.4 || E
    var OPERATION = /([+*/-])/g;                    // + || - || * || /
    var COMMAND = /([=~<CA])/g;                     // = || ~ || < || C || A
    var VALUES = /(\[[^\[\]]*(\[[^\[\]]+\])?\])/g;  // [x, y, z] || [x, y, [z]]
    var ABBREVIATIONS = {'~': '+ / -', '<': 'back', 'A': 'AC'};  // ~ || < || A
    var match, before, after, replacement;
    var model, expected, current, next, operand, i;
    // Parse the sequence into JSON array contents, so '2 + 2 = [4]' becomes...
    sequence = sequence.replace(NUMBER, ',$1$2,');  // ',2, + ,2, = [,4,]'
    sequence = sequence.replace(OPERATION, ',$1,'); // ',2, ,+, ,2,= [,4,]'
    sequence = sequence.replace(COMMAND, ',$1,');   // ',2, ,+, ,2, ,=, [,4,]'
    sequence = sequence.replace(VALUES, ',$1,');    // ',2, ,+, ,2, ,=, ,[,4,],'
    sequence = sequence.replace(/(null)/g, ',$1,'); // ',2, ,+, ,2, ,=, ,[,4,],'
    sequence = sequence.replace(/\s+/g, '');        // ',2,,+,,2,,=,,[,4,],'
    sequence = sequence.replace(/,,+/g, ',');       // ',2,+,2,=,[,4,],'
    sequence = sequence.replace(/\[,/g, '[');       // ',2,+,2,=,[4,],'
    sequence = sequence.replace(/,\]/g, ']');       // ',2,+,2,=,[4],'
    sequence = sequence.replace(/(^,)|(,$)/g, '');  // '2,+,2,=,[4]'
    sequence = sequence.replace(NUMBER, '"$1$2"');  // '"2",+,"2",=,["4"]'
    sequence = sequence.replace(OPERATION, '"$1"'); // '"2","+","2",=,["4"]'
    sequence = sequence.replace(COMMAND, '"$1"');   // '"2","+","2","=",["4"]'
    // Fix some errors generated by the parsing above. For example the original
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
    // Convert the sequence string to an array.
    sequence = JSON.parse('[' + sequence + ']');  // ['2', '+', '2', '=', ['4']]
    // Set up the test and test each element of the sequence.
    model = new Model();
    expected = [null, null, null];
    for (i = 0; i < sequence.length; ++i) {
      // Test the current sequence element, skipping any expected values already
      // processed.
      current = sequence[i];
      if (!Array.isArray(current)) {
        // Apply expected value rules.
        operand = expected[2] && expected[2].join('');
        if (current.match(OPERATION))
          expected = [operand || expected[0] || '0', current, null];
        else if (current === '=')
          expected = [operand || expected[0] || '0', null, null];
        else if (current === '~' && operand)
          expected[2] = ['-' + operand];
        // Adjust expected values with explicitly specified ones.
        next = sequence[i + 1];
        if (Array.isArray(next) && Array.isArray(next[0]))
          expected = expected.slice(0, 2).concat([next[0]]).slice(0, 3);
        else if (Array.isArray(next))
          expected = next.concat(expected.slice(next.length)).slice(0, 3);
        // Test.
        if (current.match(NUMBER))
          this.testNumber_(model, current, expected);
        else
          this.testKey_(model, ABBREVIATIONS[current] || current, expected);
      };
    }
  },

  /**
   * @private
   * Tests how a calculator model handles a sequence of digits and periods
   * representing a number. During the test, the expected operand values are
   * updated before each digit and period of the input according to these rules:
   *
   *   - If the passed in expected values array has a null expected operand
   *     array, the expected operand used for the tests starts with the first
   *     digit or period of the numeric sequence and the following digits and
   *     periods of that sequence are appended before each new key's test.
   *
   *   - If the passed in expected values array has an expected operand array of
   *     the form [operand], the expected operand used for the tests start with
   *     the first character of that array's element and one additional
   *     character of that element is added before each of the following digit
   *     and period key tests.
   *
   *   - If the passed in expected values array has an expected operand array of
   *     the form [prefix, operand], the expected operand used for the tests
   *     starts with the first element in that array and the first character of
   *     the second element, and one character of that second element is added
   *     before each of the following digit and period key tests.
   *
   *   - In all of these cases, leading zeros and occurrences of the '='
   *     character in the expected operand are ignored.
   *
   * For example the sequence of calls:
   *
   *   run.testNumber_(model, '00', [x, y, ['0=']])
   *   run.testNumber_(model, '1.2.3', [x, y, ['1.2=3']])
   *   run.testNumber_(model, '45', [x, y, ['1.23', '45']])
   *
   * would yield the following tests:
   *
   *   run.verify(model.handle('0'), [x, y, '0'], '0');
   *   run.verify(model.handle('0'), [x, y, '0'], '0');
   *   run.verify(model.handle('1'), [x, y, '1'], '1');
   *   run.verify(model.handle('.'), [x, y, '1.'], '.');
   *   run.verify(model.handle('2'), [x, y, '1.2'], '2');
   *   run.verify(model.handle('.'), [x, y, '1.2'], '.');
   *   run.verify(model.handle('3'), [x, y, '1.23'], '3');
   *   run.verify(model.handle('4'), [x, y, '1.234'], '4');
   *   run.verify(model.handle('5'), [x, y, '1.2345'], '5');
   *
   * It would also changes the expected value array to the following:
   *
   *   [x, y, ['1.2345']]
   */
  testNumber_: function(model, number, expected) {
    var multiple = (expected[2] && expected[2].length > 1);
    var prefix = multiple ? expected[2][0] : '';
    var suffix = expected[2] ? expected[2][multiple ? 1 : 0] : number;
    for (var i = 0; i < number.length; ++i) {
      expected[2] = [prefix + suffix.slice(0, i + 1)];
      expected[2] = [expected[2][0].replace(/^0+([0-9])/, '$1')];
      expected[2] = [expected[2][0].replace(/=/g, '')];
      this.testKey_(model, number[i], expected);
    }
  },

  /**
   * @private
   * Tests how a calculator model handles a single key of input, logging the
   * state of the model before and after the test.
   */
  testKey_: function(model, key, expected) {
    var description = this.addDescription_(['"', key, '": '], model, ' => ');
    var result = model.handle(key);
    var accumulator = result.accumulator;
    var operator = result.operator;
    var operand = result.operand;
    var actual = [accumulator, operator, operand && [operand]];
    this.verify(expected, actual, this.getDescription_(description, result));
  },

  /** @private */
  areEqual_: function(x, y) {
    return Array.isArray(x) ? this.areArraysEqual_(x, y) : (x == y);
  },

  /** @private */
  areArraysEqual_: function(a, b) {
    return Array.isArray(a) &&
           Array.isArray(b) &&
           a.length === b.length &&
           a.every(function(element, i) {
             return this.areEqual_(a[i], b[i]);
           }, this);
  },

  /** @private */
  getDescription_: function(prefix, object, suffix) {
    var strings = Array.isArray(prefix) ? prefix : prefix ? [prefix] : [];
    return this.addDescription_(strings, object, suffix).join('');
  },

  /** @private */
  addDescription_: function(prefix, object, suffix) {
    var strings = Array.isArray(prefix) ? prefix : prefix ? [prefix] : [];
    if (Array.isArray(object)) {
      strings.push('[', '');
      object.forEach(function(element) {
        this.addDescription_(strings, element, ', ');
      }, this);
      strings.pop();  // Pops the last ', ', or pops '' for empty arrays.
      strings.push(']');
    } else if (typeof object === 'number') {
      strings.push('#');
      strings.push(String(object));
    } else if (typeof object === 'string') {
      strings.push('"');
      strings.push(object);
      strings.push('"');
    } else if (object instanceof Model) {
      strings.push('(');
      this.addDescription_(strings, object.accumulator, ' ');
      this.addDescription_(strings, object.operator, ' ');
      this.addDescription_(strings, object.operand, ' | ');
      this.addDescription_(strings, object.defaults.operator, ' ');
      this.addDescription_(strings, object.defaults.operand, ')');
    } else {
      strings.push(String(object));
    }
    strings.push(suffix || '');
    return strings;
  }

}
