// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains functions that control several animations in the print
// preview tab (scrollbars, showing hiding options, resizing).

// Scrollbar will never get smaller than this
const TRANSIENT_MIN_SCROLLBAR_SIZE = 40;
//  Timeout duration in milliseconds used for showing the scrollbars.
const HIDE_SCROLLBARS_TIMEOUT = 250;
// Timeout id used to reset the timer when needed.
var transientHideScrollbarsTimeoutId = [];
//  Holds all elements that have scrollbars attached to them.
var transientScrollbarEls = [];
// Counter used to give webkit animations unique names.
var animationCounter = 0;

/**
 * Makes the scrollbars visible. If |el| has already a scrollbar timer, it
 * resets its value.
 *
 * @param {HTMLDivElement} el The element associated with the scrollbars
 */
function showTransientScrollbars(el) {
  el.scrollHorEl.classList.remove('invisible');
  el.scrollVertEl.classList.remove('invisible');

  if (el.transientHideScrollbarsTimeoutId)
    window.clearTimeout(el.transientHideScrollbarsTimeoutId);

  el.transientHideScrollbarsTimeoutId =
      window.setTimeout(function() { hideTransientScrollbars(el) },
                        HIDE_SCROLLBARS_TIMEOUT);
}

/**
 * Hides the scrollbars.
 *
 * @param {HTMLElement} el The element associated with the scrollbars
*/
function hideTransientScrollbars(el) {
  el.scrollHorEl.classList.add('invisible');
  el.scrollVertEl.classList.add('invisible');
}

/**
 * Handles a mouse move event, takes care of updating the scrollbars.
 *
 * @param {event} event The event that triggered this handler
*/
function handleTransientMouseMove(event) {
  var el = event.target;

  while (!el.classList.contains('scrollbar-inside') && el != document.body)
    el = el.parentNode;

  showTransientScrollbars(el);
}

/**
 * Updates the scrollbars associated with the the input element.
 *
 * @param {HTMLElement} el
 */
function updateTransientScrollbars(el) {
  var scrollLeft = el.scrollLeft;
  var scrollTop = el.scrollTop;

  var scrollWidth = el.scrollWidth;
  var scrollHeight = el.scrollHeight;

  var offsetWidth = el.offsetWidth - el.scrollbarWidth;
  var offsetHeight = el.offsetHeight - el.scrollbarHeight;

  var elevatorWidth = offsetWidth / scrollWidth * offsetWidth;
  var elevatorHeight = offsetHeight / scrollHeight * offsetHeight;

  // Make sure the scrollbars are big enough.
  if (elevatorWidth < TRANSIENT_MIN_SCROLLBAR_SIZE)
    elevatorWidth = TRANSIENT_MIN_SCROLLBAR_SIZE;
  if (elevatorHeight < TRANSIENT_MIN_SCROLLBAR_SIZE)
    elevatorHeight = TRANSIENT_MIN_SCROLLBAR_SIZE;

  if (offsetWidth >= scrollWidth) {
    if (!el.scrollHorEl.classList.contains('hidden'))
      el.scrollHorEl.classList.add('hidden');
  } else {
    if (el.scrollHorEl.classList.contains('hidden'))
      el.scrollHorEl.classList.remove('hidden');
    var x = scrollLeft / (scrollWidth - offsetWidth);

    // TODO(mwichary): 6 shouldn’t be hardcoded
    el.scrollHorEl.style.left = (x * (offsetWidth - elevatorWidth - 6)) + 'px';
    el.scrollHorEl.style.width = elevatorWidth + 'px';
  }

  if (offsetHeight >= scrollHeight) {
    if (!el.scrollVertEl.classList.contains('hidden'))
      el.scrollVertEl.classList.add('hidden');
  } else {
    if (el.scrollVertEl.classList.contains('hidden'))
      el.scrollVertEl.classList.remove('hidden');

    var y = scrollTop / (scrollHeight - offsetHeight);

    // TODO(mwichary): 6 shouldn’t be hardcoded
    el.scrollVertEl.style.top =
        (y * (offsetHeight - elevatorHeight - 6)) + 'px';
    el.scrollVertEl.style.height = elevatorHeight + 'px';
  }
}

/**
 * Updates all exising scrollbars.
 */
function updateAllTransientScrollbars() {
  for (var i = 0; i < transientScrollbarEls.length; i++) {
    var el = transientScrollbarEls[i];
    updateTransientScrollbars(el);
  }
}

/**
 * Handles a scroll event.
 */
function handleTransientScroll(event) {
  var el = event.target;

  if (el == document)
    el = document.body;

  updateTransientScrollbars(el);

  // Make sure to show the scrollbars if they are hidden.
  showTransientScrollbars(el);
}

/**
 * Adds scrollbars to the input element.
 *
 * @param {HTMLElement} scrollableEl The scrollable element
 */
function addTransientScrollbars(scrollableEl) {
  var insideEl = scrollableEl.querySelector('.scrollbar-inside');

  if (!insideEl)
    insideEl = scrollableEl;

  // Determine the width/height of native scrollbar elements.
  insideEl.scrollbarWidth = insideEl.offsetWidth - insideEl.clientWidth;
  insideEl.scrollbarHeight = insideEl.offsetHeight - insideEl.clientHeight;

  // Create scrollbar elements
  insideEl.scrollHorEl = document.createElement('div');
  insideEl.scrollHorEl.className = 'scrollbar hor';
  scrollableEl.appendChild(insideEl.scrollHorEl);

  insideEl.scrollVertEl = document.createElement('div');
  insideEl.scrollVertEl.className = 'scrollbar vert';
  scrollableEl.appendChild(insideEl.scrollVertEl);

  // Need to make sure the scrollbars are absolutely positioned vis-a-vis
  // their parent element. If not, make its position relative.
  if (insideEl.scrollVertEl.offsetParent != scrollableEl)
    scrollableEl.style.position = 'relative';

  if (insideEl == document.body)
    window.addEventListener('scroll', handleTransientScroll, false);
  else
    insideEl.addEventListener('scroll', handleTransientScroll, false);
  insideEl.addEventListener('mousemove', handleTransientMouseMove, false);

  updateTransientScrollbars(insideEl);
  showTransientScrollbars(insideEl);

  transientScrollbarEls.push(insideEl);
}

/**
 * Creates webkit animation as specified by |code.
 *
 * @param {string} code The code specifying the animation.
 */
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

/**
 * Shows/hides elements of class "hidden-section" depending on their
 * current state.
 *
 * @param {HTLMElement} el The element to be shown/hidden
 */
function handleZippyClickEl(el) {
  while (el.tagName != 'LI' && !el.classList.contains('hidden-section'))
    el = el.parentNode;

  var extraEl = el.querySelector('.extra');

  if (!el.classList.contains('opened')) {
    extraEl.style.height = 'auto';
    var height = extraEl.offsetHeight;

    extraEl.style.height = height + 'px';

    el.classList.add('opened');

    var animName = addAnimation(
      '0% { opacity: 0; height: 0; padding-top: 0; } ' +
      '80% { height: ' + (height + 2) + 'px; padding-top: 12px; }' +
      '100% { opacity: 1; height: ' + height + 'px; padding-top: 10px; }');

    extraEl.style.webkitAnimationName = animName;

  } else {
    extraEl.style.webkitAnimationName = '';
    extraEl.style.height = 'auto';
    el.classList.remove('opened');
    el.classList.add('closing');

    // DEBUG
    window.setTimeout(
      function() { handleZippyClickCleanup(el, extraEl); }, 120);
  }

  window.setTimeout(updateAllTransientScrollbars, 100);
}

/**
 * Cleans up the show/hide animation of function handleZippyClickEl().
 */
function handleZippyClickCleanup(el, extraEl) {
  extraEl.style.height = '';
  el.classList.remove('closing');
}

/**
 * Handler for the load event.
 */
function handleBodyLoad() {
  document.body.classList.add('loaded');
}

/**
 * Initializes the scrollbars animation.
 */
function initializeAnimation() {
  if (document.querySelector('body > .sidebar'))
    addTransientScrollbars(document.querySelector('body > .sidebar'));
  else
    addTransientScrollbars(document.body);

  window.addEventListener('resize', updateAllTransientScrollbars, false);
  window.addEventListener('load', handleBodyLoad, false);
}
