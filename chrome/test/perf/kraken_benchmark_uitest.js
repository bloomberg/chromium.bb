/*
  Copyright (c) 2012 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

var automation = {
  setDone: function() {
    document.cookie = '__done=1; path=/';
  },

  getResults: function() {
    results = {'score': String(mean)};
    for (category in categoryMeans) {
      results[category] = String(categoryMeans[category]);
      for (test in testMeansByCategory[category]) {
        results[test] = String(testMeansByCategory[category][test]);
      }
    }
    return JSON.stringify(results);
  }
};
