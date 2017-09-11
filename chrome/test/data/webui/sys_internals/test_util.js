// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TestUtil = TestUtil || {};

TestUtil.assertCloseTo = function(value, equ, delta, optMessage) {
  chai.assert.closeTo(value, equ, delta, optMessage);
};
