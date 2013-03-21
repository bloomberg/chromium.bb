// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scroll handling.
//
// Switches the sidebar between floating on the left and position:fixed
// depending on whether it's scrolled into view.
(function() {

var sidebar = document.getElementById('gc-sidebar');
var offsetTop = sidebar.offsetTop;

window.addEventListener('scroll', function() {
  // Obviously, this code executes every time the window scrolls, so avoid
  // putting things in here.
  if (sidebar.classList.contains('floating')) {
    if (window.scrollY < offsetTop)
      sidebar.classList.remove('floating');
  } else {
    if (window.scrollY > offsetTop) {
      sidebar.classList.add('floating');
      sidebar.scrollTop = 0;
    }
  }
});

}());
