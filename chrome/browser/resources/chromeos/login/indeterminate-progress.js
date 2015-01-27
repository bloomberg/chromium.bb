// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('indeterminate-progress', {
  runnerPortion: 20,
  rate: 1,
  runnerColor: '#0f9d58',
  backgroundColor: '#c8c8c8',

  progress: 0,
  min: Math.min,
  max: Math.max,

  computed: {
    scaledProgress: '((100 + runnerPortion) * progress / 100) - runnerPortion',
    primaryProgress: 'max(0, scaledProgress)',
    secondaryProgress: 'min(scaledProgress + runnerPortion, 100)',
    timeout: '20 / min(max(rate, 1), 10)'
  },

  ready: function() {
  },

  doProgress: function() {
    if (this.progress + 1 > 100)
      this.progress = 0;
    else
      ++this.progress;
    this.async(this.doProgress, null, this.timeout);
  }
});
