/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {assert} from 'chai';
import {TriageExisting} from './triage-existing.js';

suite('triage-existing', function() {
  test('filterBugs', async function() {
    const bugs = [
      {revisionRange: tr.b.math.Range.fromExplicitRange(9, 9)},
      {revisionRange: tr.b.math.Range.fromExplicitRange(9, 10)},
      {revisionRange: tr.b.math.Range.fromExplicitRange(20, 21)},
      {revisionRange: tr.b.math.Range.fromExplicitRange(21, 21)},
    ];
    const selectedRange = tr.b.math.Range.fromExplicitRange(10, 20);
    assert.deepEqual([], TriageExisting.filterBugs(bugs, false, undefined));
    assert.deepEqual([], TriageExisting.filterBugs(
        undefined, false, selectedRange));
    assert.strictEqual(bugs, TriageExisting.filterBugs(
        bugs, false, selectedRange));

    const actual = TriageExisting.filterBugs(bugs, true, selectedRange);
    assert.notInclude(actual, bugs[0]);
    assert.include(actual, bugs[1]);
    assert.include(actual, bugs[2]);
    assert.notInclude(actual, bugs[3]);
  });
});
