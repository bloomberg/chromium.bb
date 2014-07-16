// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addToPage(html) {
  var div = document.createElement('div');
  div.innerHTML = html;
  document.getElementById('content').appendChild(div);
}

function showLoadingIndicator(isLastPage) {
  document.getElementById('loadingIndicator').className =
      isLastPage ? 'hidden' : 'visible';
}

// Maps JS theme to CSS class and then changes body class name.
// CSS classes must agree with distilledpage.css.
function useTheme(theme) {
  var cssClass;
  if (theme == "sepia") {
    cssClass = "sepia";
  } else if (theme == "dark") {
    cssClass = "dark";
  } else {
    cssClass = "light";
  }
  document.body.className = cssClass;
}
