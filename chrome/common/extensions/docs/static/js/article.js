// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scroll handling.
//
// Switches the sidebar between floating on the left and position:fixed
// depending on whether it's scrolled into view, and manages the scroll-to-top
// button: click logic, and when to show it.
(function() {

var isLargerThanMobileQuery =
    window.matchMedia('screen and (min-width: 581px)');

var sidebar = document.querySelector('.inline-toc');
var sidebarOffsetTop = null;
var articleBody = document.querySelector('[itemprop="articleBody"]');

// Bomb out unless we're on an article page and have a TOC.
if (!(sidebar && articleBody)) {
  return;
}

function addPermalink(el) {
  el.classList.add('has-permalink');
  var id = el.id || el.textContent.toLowerCase().replace(' ', '-');
  el.insertAdjacentHTML('beforeend',
      '<a class="permalink" title="Permalink" href="#' + id + '">#</a>');
}

// Add permalinks to heading elements.
function addPermalinkHeadings(container) {
  if (container) {
    ['h2','h3','h4'].forEach(function(h, i) {
      [].forEach.call(container.querySelectorAll(h), addPermalink);
    });
  }
}

function onScroll(e) {
  window.scrollY >= sidebarOffsetTop ? sidebar.classList.add('sticky') :
                                       sidebar.classList.remove('sticky');
}

function onMediaQuery(e) {
  if (e.matches) {
    // On tablet & desktop, show permalinks, manage TOC position.
    document.body.classList.remove('no-permalink');
    sidebarOffsetTop = sidebar.offsetParent.offsetTop
    document.addEventListener('scroll', onScroll);
  } else {
    // On mobile, hide permalinks. TOC is hidden, doesn't need to scroll.
    document.body.classList.add('no-permalink');
    document.removeEventListener('scroll', onScroll);
  }
}

// Toggle collapsible sections (mobile).
articleBody.addEventListener('click', function(e) {
  if (e.target.localName == 'h2' && !isLargerThanMobileQuery.matches) {
    e.target.parentElement.classList.toggle('active');
  }
});

sidebar.querySelector('#toc').addEventListener('click', function(e) {
  var parent = e.target.parentElement;
  if (e.target.localName == 'a' && parent.classList.contains('toplevel')) {
    // Allow normal link click if  h2 toplevel heading doesn't have h3s.
    if (parent.querySelector('.toc')) {
      e.preventDefault();
      parent.classList.toggle('active');
    }
  }
});

// Add +/- expander to headings with subheading children.
[].forEach.call(toc.querySelectorAll('.toplevel'), function(heading) {
  if (heading.querySelector('.toc')) {
    heading.firstChild.classList.add('hastoc');
  }
});

isLargerThanMobileQuery.addListener(onMediaQuery);
onMediaQuery(isLargerThanMobileQuery);

addPermalinkHeadings(articleBody);

}());
