/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {assert} from 'chai';
import * as utils from './utils.js';

suite('utils', function() {
  test('setImmutable', function() {
    assert.deepEqual({b: 'c'}, utils.setImmutable(
        {a: []}, '', {b: 'c'}));
    assert.deepEqual({a: [], b: 'c'}, utils.setImmutable(
        {a: []}, '', rootState => {
          return {...rootState, b: 'c'};
        }));
    assert.deepEqual({a: [{b: 'c'}]}, utils.setImmutable(
        {a: []}, 'a.0.b', 'c'));
    assert.deepEqual({a: {b: 1, c: 2}}, utils.setImmutable(
        {a: {b: 1}}, 'a', o => {
          return {...o, c: 2};
        }));
  });

  test('deepFreeze', function() {
    const frozen = {
      str: 'a',
      array: ['b'],
      obj: {a: 'c'},
      instance: new class {
        constructor() {
          this.property = 'd';
        }
      },
    };
    utils.deepFreeze(frozen);
    assert.throws(() => {
      frozen.str = 'x';
    }, 'Cannot assign to read only property');
    assert.throws(() => {
      frozen.array[0] = 'x';
    }, 'Cannot assign to read only property');
    assert.throws(() => {
      frozen.array[1] = 'x';
    }, 'Cannot add property');
    assert.throws(() => {
      frozen.obj.a = 'x';
    }, 'Cannot assign to read only property');
    frozen.instance.property = 'x';
    assert.strictEqual('x', frozen.instance.property);
  });

  test('isElementChildOf', function() {
    assert.isTrue(utils.isElementChildOf(
        document.body, document.body.parentElement));
    assert.isTrue(utils.isElementChildOf(
        document.body.children[0], document.body.parentElement));
    assert.isFalse(utils.isElementChildOf(
        document.body, document.body));
    assert.isFalse(utils.isElementChildOf(
        document.body.parentElement, document.body));

    const div = document.createElement('div');
    document.body.appendChild(div);
    div.root = div.attachShadow({mode: 'open'});
    const child = document.createElement('div');
    div.root.appendChild(child);
    assert.isTrue(utils.isElementChildOf(child, document.body));
    document.body.removeChild(div);
  });

  test('getActiveElement', function() {
    const input = document.createElement('input');
    document.body.appendChild(input);
    input.focus();
    assert.strictEqual(input, utils.getActiveElement());
    document.body.removeChild(input);
  });

  test('measureElement', async function() {
    const input = document.createElement('input');
    input.style.margin = '1px';
    document.body.appendChild(input);
    const rect = await utils.measureElement(input);
    assert.isBelow(0, rect.bottom);
    assert.isBelow(0, rect.height);
    assert.isBelow(0, rect.left);
    assert.isBelow(0, rect.right);
    assert.isBelow(0, rect.top);
    assert.isBelow(0, rect.width);
    assert.isBelow(0, rect.x);
    assert.isBelow(0, rect.y);
    document.body.removeChild(input);
  });

  test('measureText cache', async function() {
    assert.strictEqual(await utils.measureText('hello'),
        await utils.measureText('hello'));
  });

  test('measureText', async function() {
    const [rect, larger] = await Promise.all([
      utils.measureText('hello'),
      utils.measureText('hello', {fontSize: 'larger'}),
    ]);
    assert.isBelow(0, rect.height);
    assert.isBelow(0, rect.width);
    assert.isBelow(rect.height, larger.height);
    assert.isBelow(rect.width, larger.width);
  });

  test('measureTrace', function() {
    tr.b.Timing.mark('spa/utils-test', 'measureTrace').end();
    tr.b.Timing.mark('spa/utils-test', 'measureTrace').end();
    tr.b.Timing.mark('spa/utils-test', 'measureTrace').end();
    const trace = utils.measureTrace();
    assert.lengthOf(trace.filter(e =>
      e.name === 'spa/utils-test:measureTrace'), 3);
  });

  test('measureHistograms', function() {
    tr.b.Timing.mark('spa/utils-test', 'measureHistograms').end();
    tr.b.Timing.mark('spa/utils-test', 'measureHistograms').end();
    tr.b.Timing.mark('spa/utils-test', 'measureHistograms').end();
    const histograms = utils.measureHistograms();
    const hist = histograms.getHistogramNamed(
        'spa/utils-test:measureHistograms');
    assert.strictEqual(3, hist.numValues);
  });

  test('measureTable', function() {
    tr.b.Timing.mark('spa/utils-test', 'measureTable').end();
    tr.b.Timing.mark('spa/utils-test', 'measureTable').end();
    tr.b.Timing.mark('spa/utils-test', 'measureTable').end();
    const rows = utils.measureTable().split('\n');
    assert.include(rows, '0     spa/utils-test:measureTable');
  });

  test('normalize', function() {
    assert.deepEqual({a: 1, b: 2}, utils.normalize(['a', 'b'], [1, 2]));
  });

  test('BatchIterator empty', async function() {
    const batches = [];
    for await (const batch of new utils.BatchIterator()) {
      batches.push(batch);
    }
    assert.lengthOf(batches, 0);
  });

  test('BatchIterator 1 Promise success', async function() {
    let complete;
    const batches = new utils.BatchIterator([new Promise(resolve => {
      complete = resolve;
    })]);
    const iterations = [];
    const done = (async() => {
      for await (const {results, errors} of batches) {
        iterations.push({results, errors});
      }
    })();
    assert.lengthOf(iterations, 0);
    complete('hello');
    await done;
    assert.lengthOf(iterations, 1);
    assert.lengthOf(iterations[0].errors, 0);
    assert.lengthOf(iterations[0].results, 1);
    assert.strictEqual('hello', iterations[0].results[0]);
  });

  test('BatchIterator 1 Promise error', async function() {
    let complete;
    const batches = new utils.BatchIterator([new Promise((resolve, reject) => {
      complete = reject;
    })]);
    const iterations = [];
    const done = (async() => {
      for await (const {results, errors} of batches) {
        iterations.push({results, errors});
      }
    })();
    assert.lengthOf(iterations, 0);
    complete('hello');
    await done;
    assert.lengthOf(iterations, 1);
    assert.lengthOf(iterations[0].errors, 1);
    assert.lengthOf(iterations[0].results, 0);
    assert.strictEqual('hello', iterations[0].errors[0]);
  });

  test('BatchIterator 1 generator-1 success', async function() {
    const batches = new utils.BatchIterator([(async function* () {
      yield 'hello';
    })()]);
    const iterations = [];
    for await (const {results, errors} of batches) {
      iterations.push({results, errors: errors.map(e => e.message)});
    }
    assert.lengthOf(iterations, 1);
    assert.lengthOf(iterations[0].errors, 0);
    assert.lengthOf(iterations[0].results, 1);
    assert.strictEqual('hello', iterations[0].results[0]);
  });

  test('BatchIterator 1 generator-1 error', async function() {
    const batches = new utils.BatchIterator([(async function* () {
      throw new Error('hello');
    })()]);
    const iterations = [];
    for await (const {results, errors} of batches) {
      iterations.push({results, errors});
    }
    assert.lengthOf(iterations, 1);
    assert.lengthOf(iterations[0].errors, 1);
    assert.lengthOf(iterations[0].results, 0);
    assert.strictEqual('hello', iterations[0].errors[0].message);
  });

  test('BatchIterator 1 generator, 2 batches', async function() {
    const resolves = [];
    const promiseA = new Promise(resolve => resolves.push(resolve));
    const batches = new utils.BatchIterator([(async function* () {
      yield 'a';
      yield 'b';
      await promiseA;
      yield 'c';
    })()]);
    const iterations = [];
    for await (const {results, errors} of batches) {
      iterations.push({results, errors: errors.map(e => e.message)});
      const resolve = resolves.shift();
      if (resolve) resolve();
    }
    assert.lengthOf(iterations, 2, JSON.stringify(iterations));
    assert.deepEqual([], iterations[0].errors);
    assert.deepEqual(['a', 'b'], iterations[0].results);
    assert.deepEqual([], iterations[1].errors);
    assert.deepEqual(['c'], iterations[1].results);
  });

  test('BatchIterator 1 generator, 3 batches', async function() {
    const resolves = [];
    const promiseA = new Promise(resolve => resolves.push(resolve));
    const promiseB = new Promise(resolve => resolves.push(resolve));
    const batches = new utils.BatchIterator([(async function* () {
      yield 'a';
      await promiseA;
      yield 'b';
      await promiseB;
      yield 'c';
    })()]);
    const iterations = [];
    for await (const {results, errors} of batches) {
      iterations.push({results, errors: errors.map(e => e.message)});
      const resolve = resolves.shift();
      if (resolve) resolve();
    }
    assert.lengthOf(iterations, 3, JSON.stringify(iterations));
    assert.deepEqual([], iterations[0].errors);
    assert.deepEqual(['a'], iterations[0].results);
    assert.deepEqual([], iterations[1].errors);
    assert.deepEqual(['b'], iterations[1].results);
    assert.deepEqual([], iterations[2].errors);
    assert.deepEqual(['c'], iterations[2].results);
  });

  test('BatchIterator 3 generators, 3 batches', async function() {
    const resolves = [];
    const promiseA = new Promise(resolve => resolves.push(resolve));
    const promiseB = new Promise(resolve => resolves.push(resolve));
    const batches = new utils.BatchIterator([
      (async function* () {
        yield 'a0';
        await promiseA;
        yield 'a1';
      })(),
      (async function* () {
        yield 'b0';
        await promiseA;
        yield 'b1';
        throw new Error('bbb');
      })(),
      (async function* () {
        await promiseA;
        yield 'c0';
        await promiseB;
        yield 'c1';
      })(),
    ]);
    const iterations = [];
    for await (const {results, errors} of batches) {
      iterations.push({results, errors: errors.map(e => e.message)});
      const resolve = resolves.shift();
      if (resolve) resolve();
    }
    assert.lengthOf(iterations, 3, JSON.stringify(iterations));
    assert.deepEqual([], iterations[0].errors);
    assert.deepEqual(['a0', 'b0'], iterations[0].results);
    assert.deepEqual(['c0', 'a1', 'b1'], iterations[1].results);
    assert.deepEqual(['bbb'], iterations[1].errors);
    assert.deepEqual([], iterations[2].errors);
    assert.deepEqual(['c1'], iterations[2].results);
  });

  test('breakWords', function() {
    assert.strictEqual(utils.NON_BREAKING_SPACE, utils.breakWords(''));
    const Z = utils.ZERO_WIDTH_SPACE;
    assert.strictEqual(`a${Z}_b`, utils.breakWords('a_b'));
    assert.strictEqual(`a:${Z}b`, utils.breakWords('a:b'));
    assert.strictEqual(`a.${Z}b`, utils.breakWords('a.b'));
    assert.strictEqual(`a${Z}B`, utils.breakWords('aB'));
  });

  test('plural', function() {
    assert.strictEqual('s', utils.plural(0));
    assert.strictEqual('', utils.plural(1));
    assert.strictEqual('s', utils.plural(2));

    assert.strictEqual('es', utils.plural(0, 'es', 'x'));
    assert.strictEqual('x', utils.plural(1, 'es', 'x'));
    assert.strictEqual('es', utils.plural(2, 'es', 'x'));
  });

  test('generateColors', function() {
    assert.lengthOf(utils.generateColors(10), 10);
  });
});
