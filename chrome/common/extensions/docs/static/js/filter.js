// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is used for the search box on the left navigation bar. The APIs are
// filtered and displayed based on what the user is typing in the box.
(function() {
  var search_box = document.getElementById('api_search');
  var filtered_apis = document.getElementById('filtered_apis');

  function filterAPIs() {
    var search_text = search_box.value.toLowerCase();
    var apis = window.bootstrap.api_names;
    if (!search_text) {
      filtered_apis.style.display = 'none';
      return;
    }
    var api_list = ''
    for (var i = 0; i < apis.length; ++i) {
      if (apis[i].name.toLowerCase().indexOf(search_text) != -1)
        api_list += '<li class="filtered_item"><a href="' + apis[i].name +
            '.html">' + apis[i].name + '</a></li>'
    }
    if (api_list != filtered_apis.innerHtml)
      filtered_apis.innerHTML = api_list;
    if (!api_list)
      filtered_apis.style.display = 'none';
    else
      filtered_apis.style.display = '';
  }

  filtered_apis.style.display = 'none';
  search_box.addEventListener('search', filterAPIs);
  search_box.addEventListener('keyup', filterAPIs);
})();
