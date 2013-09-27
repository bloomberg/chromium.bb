// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var searchBox = document.getElementById('search_input');
  var samples = document.getElementsByClassName('sample');

  function filterSamples() {
    var searchText = searchBox.value.toLowerCase();
    for (var i = 0; i < samples.length; ++i) {
      var sample = samples[i];
      var sampleTitle = '';
      if (sample.getElementsByTagName('h2').length > 0)
        sampleTitle = sample.getElementsByTagName('h2')[0].textContent;
      if (sample.getAttribute('tags').toLowerCase().indexOf(searchText) < 0 &&
          sampleTitle.toLowerCase().indexOf(searchText) < 0)
        sample.style.display = 'none';
      else
        sample.style.display = '';
    }
  }
  searchBox.addEventListener('search', filterSamples);
  searchBox.addEventListener('keyup', filterSamples);

  var apiFilterItems = document.getElementById('api_filter_items');
  apiFilterItems.addEventListener('click', function(event) {
    if (event.target instanceof HTMLAnchorElement) {
      searchBox.value = event.target.innerText;
      filterSamples();
    }
  });
})();
