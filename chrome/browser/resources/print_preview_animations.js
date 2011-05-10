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
