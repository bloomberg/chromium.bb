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
  el.setAttribute('id', name);
  document.body.appendChild(el);

  return name;
}

/**
 * Fades in an element. Used for both printing options and error messages
 * appearing underneath the textfields.
 * @param {HTMLElement} el The element to be faded in.
 */
function fadeInElement(el) {
  if (el.classList.contains('visible'))
    return;
  el.classList.remove('closing');
  el.style.height = 'auto';
  var height = el.offsetHeight;
  el.style.height = height + 'px';
  var animName = addAnimation(
    '0% { opacity: 0; height: 0; } ' +
    '80% { height: ' + (height + 4) + 'px; }' +
    '100% { opacity: 1; height: ' + height + 'px; }');
  el.style.webkitAnimationName = animName;
  el.classList.add('visible');
  el.addEventListener('webkitAnimationEnd', function() {
    el.style.height = '';
    el.style.webkitAnimationName = '';
    fadeInOutCleanup(animName);
    el.removeEventListener('webkitAnimationEnd', arguments.callee, false);
  }, false );
}

/**
 * Fades out an element. Used for both printing options and error messages
 * appearing underneath the textfields.
 * @param {HTMLElement} el The element to be faded out.
 */
function fadeOutElement(el) {
  if (!el.classList.contains('visible'))
    return;
  el.style.height = 'auto';
  var height = el.offsetHeight;
  el.style.height = height + 'px';
  var animName = addAnimation(
    '0% { opacity: 1; height: ' + height + 'px; }' +
    '100% { opacity: 1; height: 0; }');
  el.style.webkitAnimationName = animName;
  el.classList.add('closing');
  el.classList.remove('visible');
  el.addEventListener('webkitAnimationEnd', function() {
    fadeInOutCleanup(animName);
    el.removeEventListener('webkitAnimationEnd', arguments.callee, false);
  }, false );
}

/**
 * Removes the <style> element corrsponding to |animationName| from the DOM.
 * @param {string} animationName The name of the animation to be removed.
 */
function fadeInOutCleanup(animationName) {
  animEl = document.getElementById(animationName);
  if (animEl)
    animEl.parentNode.removeChild(animEl);
}

/**
 * Displays |message| followed by three dancing dots animation.
 */
function showCustomMessage(message) {
  $('custom-message-with-dots').innerHTML = message +
      $('dancing-dots-text').innerHTML;
  $('custom-message-with-dots').hidden = false;
  var pdfViewer = $('pdf-viewer');
  if (pdfViewer)
    pdfViewer.classList.add('invisible');
  $('overlay-layer').classList.remove('invisible');
}

/**
 * Displays the "Preview loading..." animation.
 */
function showLoadingAnimation() {
  showCustomMessage(localStrings.getString('loading'));
}

/**
 * Hides the |overlay-layer| and any messages currently displayed.
 */
function hideOverlayLayer() {
  if (hasPendingPrintDocumentRequest)
    return;

  var overlayLayer = $('overlay-layer');
  overlayLayer.addEventListener('webkitTransitionEnd', loadingAnimationCleanup);
  var pdfViewer = $('pdf-viewer');
  if (pdfViewer)
    pdfViewer.classList.remove('invisible');
  overlayLayer.classList.add('invisible');
}

function loadingAnimationCleanup() {
  $('custom-message-with-dots').hidden = true;
  $('overlay-layer').removeEventListener('webkitTransitionEnd',
                                         loadingAnimationCleanup);
}
