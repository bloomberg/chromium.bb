// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Counter used to give webkit animations unique names.
var animationCounter = 0;

function addAnimation(code) {
  var name = 'anim' + animationCounter;
  animationCounter++;
  var rules = document.createTextNode(
      '@-webkit-keyframes ' + name + ' {' + code + '}');
  var el = document.createElement('style');
  el.type = 'text/css';
  el.appendChild(rules);
  document.body.appendChild(el);

  return name;
}

function showInvalidHint(el) {
  if (el.classList.contains('visible'))
    return;
  el.style.height = 'auto';
  var height = el.offsetHeight;
  el.style.height = height + 'px';
  var animName = addAnimation(
    '0% { opacity: 0; height: 0; } ' +
    '80% { height: ' + (height + 4) + 'px; }' +
    '100% { opacity: 1; height: ' + height + 'px; }');
  el.style.webkitAnimationName = animName;
  el.classList.add('visible');
}

function hideInvalidHint(el) {
  if (!el.classList.contains('visible'))
    return;
  el.style.webkitAnimationName = '';
  el.classList.remove('visible');
  el.style.height = '';
}

function handleZippyClick(event) {
  handleZippyClickEl(event.target);
  event.preventDefault();
}

function handleZippyClickEl(el) {
  while (el.tagName != 'LI' && el.className.indexOf('hidden-section') == -1)
    el = el.parentNode;

  var extraEl = el.querySelector('.extra');

  if (!el.classList.contains('visible')) {
    extraEl.style.height = 'auto';
    var height = extraEl.offsetHeight - 10;
    extraEl.style.height = height + 'px';
    el.classList.add('visible');
    var animName = addAnimation(
      '0% { opacity: 0; height: 0; margin-top: -10px; } ' +
      '80% { height: ' + (height + 2) +
      'px; margin-top: 0; padding-top: 2px; }' +
      '100% { opacity: 1; height: ' + height +
      'px; margin-top: 0; padding-top: 0; }');
    extraEl.style.webkitAnimationName = animName;
  } else {
    extraEl.style.webkitAnimationName = '';
    extraEl.style.height = 'auto';
    el.classList.remove('visible');
    el.classList.add('closing');
    window.setTimeout(function() {
        extraEl.style.height = '';
        el.classList.remove('closing');
    }, 120);
  }
}

function fireEvent(el, eventName) {
  var evt = document.createEvent('HTMLEvents');
  evt.initEvent(eventName, true, true);
  el.dispatchEvent(evt);
}

function initializeZippies() {
  var els = document.querySelectorAll('.zippy');
  for (var i = 0; i < els.length; i++)
    els[i].addEventListener('click', handleZippyClick, false);
}

initializeZippies();
