/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {assert} from 'chai';
import {TimeseriesMerger} from './timeseries-merger.js';

suite('TimeseriesMerger', function() {
  test('minRevision', async function() {
    // Filter out data points before minRevision.

    const merged = [...new TimeseriesMerger([
      [
        {revision: 5, avg: 2, count: 1},
        {revision: 10, avg: 2, count: 1},
        {revision: 15, avg: 2, count: 1},
      ],
      [
        {revision: 5, avg: 4, count: 1},
        {revision: 10, avg: 4, count: 1},
        {revision: 15, avg: 4, count: 1},
      ],
    ], {
      minRevision: 10,
    })];

    assert.lengthOf(merged, 2);
    assert.strictEqual(10, merged[0][0]);
    assert.strictEqual(15, merged[1][0]);
  });

  test('maxRevision', async function() {
    // Filter out data points after maxRevision.

    const merged = [...new TimeseriesMerger([
      [
        {revision: 20, avg: 2, count: 5},
        {revision: 25, avg: 2, count: 5},
        {revision: 30, avg: 2, count: 5},
      ],
      [
        {revision: 20, avg: 4, count: 5},
        {revision: 25, avg: 4, count: 5},
        {revision: 30, avg: 4, count: 5},
      ],
    ], {
      maxRevision: 25,
    })];

    assert.lengthOf(merged, 2);
    assert.strictEqual(20, merged[0][0]);
    assert.strictEqual(25, merged[1][0]);
  });

  test('avg', async function() {
    // Compute weighted average

    const merged = [...new TimeseriesMerger([
      [
        {revision: 10, avg: 10, count: 1},
      ],
      [
        {revision: 10, avg: 30, count: 3},
      ],
    ], {
    })];

    assert.strictEqual(25, merged[0][1].avg);
  });

  test('std', async function() {
    // Merge standard deviation using Welford's algorithm.
    const merged = [...new TimeseriesMerger([
      [
        {revision: 10, avg: 2, std: 1, count: 5},
      ],
      [
        {revision: 10, avg: 4, std: 2, count: 5},
      ],
    ], {
    })];
    assert.closeTo(3.872983, merged[0][1].std, 1e-6);
  });

  test('count', async function() {
    // Sum sample count statistic.

    const merged = [...new TimeseriesMerger([
      [
        {revision: 10, avg: 2, count: 5},
      ],
      [
        {revision: 10, avg: 4, count: 5},
      ],
    ], {
    })];

    assert.strictEqual(10, merged[0][1].count);
  });

  test('sum', async function() {
    // Sum sample sum statistic.

    const merged = [...new TimeseriesMerger([
      [
        {revision: 10, avg: 2, count: 5, sum: 10},
      ],
      [
        {revision: 10, avg: 4, count: 5, sum: 20},
      ],
    ], {
    })];

    assert.strictEqual(30, merged[0][1].sum);
  });

  test('min', async function() {
    // Compute the min of the sample min statistic.

    const merged = [...new TimeseriesMerger([
      [
        {revision: 10, avg: 2, count: 5, min: 1},
      ],
      [
        {revision: 10, avg: 4, count: 5, min: 2},
      ],
    ], {
    })];

    assert.strictEqual(1, merged[0][1].min);
  });

  test('max', async function() {
    // Compute the max of the sample max statistic.

    const merged = [...new TimeseriesMerger([
      [
        {revision: 10, avg: 2, count: 5, max: 1},
      ],
      [
        {revision: 10, avg: 4, count: 5, max: 5},
      ],
    ], {
    })];

    assert.strictEqual(5, merged[0][1].max);
  });

  test('diagnostics', async function() {
    // Merge DiagnosticMaps.

    const merged = [...new TimeseriesMerger([
      [
        {revision: 10, avg: 2, count: 1, diagnostics:
          tr.v.d.DiagnosticMap.fromObject({a: new tr.v.d.GenericSet(['a'])})},
        {revision: 15, avg: 2, count: 1, diagnostics:
          tr.v.d.DiagnosticMap.fromObject({a: new tr.v.d.GenericSet(['a'])})},
        {revision: 20, avg: 2, count: 1},
        {revision: 25, avg: 2, count: 1},
      ],
      [
        {revision: 10, avg: 4, count: 1, diagnostics:
          tr.v.d.DiagnosticMap.fromObject({a: new tr.v.d.GenericSet(['a'])})},
        {revision: 15, avg: 4, count: 1},
        {revision: 20, avg: 4, count: 1, diagnostics:
          tr.v.d.DiagnosticMap.fromObject({a: new tr.v.d.GenericSet(['a'])})},
        {revision: 25, avg: 4, count: 1},
      ],
    ], {
    })];

    assert.strictEqual('a', tr.b.getOnlyElement(
        merged[0][1].diagnostics.get('a')));
    assert.strictEqual('a', tr.b.getOnlyElement(
        merged[1][1].diagnostics.get('a')));
    assert.strictEqual('a', tr.b.getOnlyElement(
        merged[2][1].diagnostics.get('a')));
    assert.isUndefined(merged[3][1].diagnostics);
  });

  test('unaligned', async function() {
    const merged = [...new TimeseriesMerger([
      [
        {revision: 12, avg: 10, count: 1},
        {revision: 14, avg: 20, count: 1},
        {revision: 18, avg: 30, count: 1},
      ],
      [
        {revision: 10, avg: 100, count: 1},
        {revision: 15, avg: 200, count: 1},
        {revision: 20, avg: 300, count: 1},
      ],
    ], {
    })];

    assert.lengthOf(merged, 6);

    assert.strictEqual(10, merged[0][0]);
    assert.strictEqual(12, merged[1][0]);
    assert.strictEqual(14, merged[2][0]);
    assert.strictEqual(15, merged[3][0]);
    assert.strictEqual(18, merged[4][0]);
    assert.strictEqual(20, merged[5][0]);

    assert.strictEqual(55, merged[0][1].avg);
    assert.strictEqual(105, merged[1][1].avg);
    assert.strictEqual(110, merged[2][1].avg);
    assert.strictEqual(115, merged[3][1].avg);
    assert.strictEqual(165, merged[4][1].avg);
    assert.strictEqual(165, merged[5][1].avg);
  });
});
