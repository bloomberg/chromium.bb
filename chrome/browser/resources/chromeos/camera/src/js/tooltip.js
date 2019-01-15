// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for tooltip.
 */
cca.tooltip = cca.tooltip || {};

/**
 * Wrapper element that shows tooltip.
 * @type {HTMLElement}
 */
cca.tooltip.wrapper_ = null;

/**
 * Hovered element whose tooltip to be shown.
 * @type {HTMLElement}
 */
cca.tooltip.hovered_ = null;

/**
 * Sets up tooltips for elements.
 * @param {NodeList<Element>} elements Elements whose tooltips to be shown.
 * @return {NodeList<Element>} Elements whose tooltips have been set up.
 */
cca.tooltip.setup = function(elements) {
  cca.tooltip.wrapper_ = document.querySelector('#tooltip');
  elements.forEach((element) => {
    var handler = () => {
      // Handler hides tooltip only when it's for the element.
      if (element == cca.tooltip.hovered_) {
        cca.tooltip.hide();
      }
    };
    element.addEventListener('mouseout', handler);
    element.addEventListener('click', handler);
    element.addEventListener('mouseover',
        cca.tooltip.show_.bind(undefined, element));
  });
  return elements;
};

/**
 * Positions the tooltip wrapper over the hovered element.
 * @private
 */
cca.tooltip.position_ = function() {
  const [edgeMargin, elementMargin] = [5, 8];
  var wrapper = cca.tooltip.wrapper_;
  var hovered = cca.tooltip.hovered_;
  var rect = hovered.getBoundingClientRect();
  var tooltipTop = rect.top - wrapper.offsetHeight - elementMargin;
  if (tooltipTop < edgeMargin) {
    tooltipTop = rect.bottom + elementMargin;
  }
  wrapper.style.top = tooltipTop + 'px';

  // Center over the hovered element but avoid touching edges.
  var hoveredCenter = rect.left + hovered.offsetWidth / 2;
  var left = Math.min(
      Math.max(hoveredCenter - wrapper.clientWidth / 2, edgeMargin),
      document.body.offsetWidth - wrapper.offsetWidth - edgeMargin);
  wrapper.style.left = Math.round(left) + 'px';
};

/**
 * Shows a tooltip over the hovered element.
 * @param {HTMLElement} element Hovered element whose tooltip to be shown.
 * @private
 */
cca.tooltip.show_ = function(element) {
  cca.tooltip.hide();
  cca.tooltip.wrapper_.textContent = element.getAttribute('aria-label');
  cca.tooltip.hovered_ = element;
  cca.tooltip.position_();
  cca.tooltip.wrapper_.classList.add('visible');
};

/**
 * Hides the shown tooltip if any.
 */
cca.tooltip.hide = function() {
  if (cca.tooltip.hovered_) {
    cca.tooltip.hovered_ = null;
    cca.tooltip.wrapper_.textContent = '';
    cca.tooltip.wrapper_.classList.remove('visible');
  }
};
