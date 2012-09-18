// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var search_box = document.getElementById('search_input');
  var samples = document.getElementsByClassName('sample');

  function filterSamples() {
    var search_text = search_box.value.toLowerCase();
    for (var i = 0; i < samples.length; ++i) {
      var sample = samples[i]
      if (sample.getAttribute('tags').toLowerCase().indexOf(search_text) < 0)
        sample.style.display = 'none';
      else
        sample.style.display = '';
    }
  }
  search_box.addEventListener('search', filterSamples);
  search_box.addEventListener('keyup', filterSamples);

  var api_filter_items = document.getElementById('api_filter_items');
  api_filter_items.addEventListener('click', function(event) {
    if (event.target instanceof HTMLAnchorElement) {
      search_box.value = 'chrome.' + event.target.innerText;
      filterSamples();
    }
  });
})();
