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

// Maps JS Font Family to CSS class and then changes body class name.
// CSS classes must agree with distilledpage.css.
function useFontFamily(fontFamily) {
  var cssClass;
  if (fontFamily == "serif") {
    cssClass = "serif";
  } else if (fontFamily == "monospace") {
    cssClass = "monospace";
  } else {
    cssClass = "sans-serif";
  }
  // Relies on the classname order of the body being Theme class, then Font
  // Family class.
  var themeClass = document.body.className.split(" ")[0];
  document.body.className = themeClass + " " + cssClass;
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
  // Relies on the classname order of the body being Theme class, then Font
  // Family class.
  var fontFamilyClass = document.body.className.split(" ")[1];
  document.body.className = cssClass + " " + fontFamilyClass;
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
