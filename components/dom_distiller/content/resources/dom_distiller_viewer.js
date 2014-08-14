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
  updateLoadingIndicator(isLastPage);
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

var updateLoadingIndicator = function() {
  var colors = ["red", "yellow", "green", "blue"];
  return function(isLastPage) {
    if (!isLastPage && typeof this.colorShuffle == "undefined") {
      var loader = document.getElementById("loader");
      if (loader) {
        var colorIndex = -1;
        this.colorShuffle = setInterval(function() {
          colorIndex = (colorIndex + 1) % colors.length;
          loader.className = colors[colorIndex];
        }, 600);
      }
    } else if (isLastPage && typeof this.colorShuffle != "undefined") {
      clearInterval(this.colorShuffle);
    }
  };
}();
