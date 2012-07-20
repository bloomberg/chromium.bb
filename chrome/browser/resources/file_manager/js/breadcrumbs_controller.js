// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends cr.EventTarget
 * @param {HTMLDivElement} div Div container for breadcrumbs.
 */
function BreadcrumbsController(div) {
  this.bc_ = div;
  this.hideLast_ = false;
}

/**
 * Extends cr.EventTarget.
 */
BreadcrumbsController.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Whether to hide the last part of the path.
 * @param {boolean} value True if hide.
 */
BreadcrumbsController.prototype.setHideLast = function(value) {
  this.hideLast_ = value;
};

/**
 * Update the breadcrumb display.
 * @param {string} rootPath Path to root.
 * @param {string} path Path to directory.
 */
BreadcrumbsController.prototype.update = function(rootPath, path) {
  this.bc_.textContent = '';

  var relativePath = path.substring(rootPath.length).replace(/\/$/, '');
  var pathNames = relativePath.split('/');
  if (pathNames[0] == '')
    pathNames.splice(0, 1);

  // We need a first breadcrumb for root, so placing last name from
  // rootPath as first name of relativePath.
  var rootPathNames = rootPath.replace(/\/$/, '').split('/');
  pathNames.splice(0, 0, rootPathNames[rootPathNames.length - 1]);
  rootPathNames.splice(rootPathNames.length - 1, 1);
  path = rootPathNames.join('/') + '/';

  var doc = this.bc_.ownerDocument;

  for (var i = 0;
       i < (this.hideLast_ ? pathNames.length - 1 : pathNames.length);
       i++) {
    if (i > 0) {
      var spacer = doc.createElement('div');
      spacer.className = 'separator';
      this.bc_.appendChild(spacer);
    }
    var pathName = pathNames[i];
    path += pathName;

    var div = doc.createElement('div');

    div.className = 'breadcrumb-path';
    div.textContent = i == 0 ? PathUtil.getRootLabel(path) : pathName;

    path = path + '/';
    div.path = path;

    this.bc_.appendChild(div);

    if (i == pathNames.length - 1) {
      div.classList.add('breadcrumb-last');
    } else {
      div.addEventListener('click', this.onClick_.bind(this));
    }
  }
  this.truncate();
};

/**
 * Updates breadcrumbs widths in order to truncate it properly.
 */
BreadcrumbsController.prototype.truncate = function() {
  if (!this.bc_.firstChild)
   return;

  // Assume style.width == clientWidth (items have no margins or paddings).

  for (var item = this.bc_.firstChild; item; item = item.nextSibling) {
    item.removeAttribute('style');
    item.removeAttribute('collapsed');
  }

  var containerWidth = this.bc_.clientWidth;

  var pathWidth = 0;
  var currentWidth = 0;
  var lastSeparator;
  for (var item = this.bc_.firstChild; item; item = item.nextSibling) {
    if (item.className == 'separator') {
      pathWidth += currentWidth;
      currentWidth = item.clientWidth;
      lastSeparator = item;
    } else {
      currentWidth += item.clientWidth;
    }
  }
  if (pathWidth + currentWidth <= containerWidth)
    return;
  if (!lastSeparator) {
    this.bc_.lastChild.style.width = Math.min(currentWidth, containerWidth) +
                                      'px';
    return;
  }
  var lastCrumbSeparatorWidth = lastSeparator.clientWidth;
  // Current directory name may occupy up to 70% of space or even more if the
  // path is short.
  var maxPathWidth = Math.max(Math.round(containerWidth * 0.3),
                              containerWidth - currentWidth);
  maxPathWidth = Math.min(pathWidth, maxPathWidth);

  var parentCrumb = lastSeparator.previousSibling;
  var collapsedWidth = 0;
  if (parentCrumb && pathWidth - maxPathWidth > parentCrumb.clientWidth) {
    // At least one crumb is hidden completely (or almost completely).
    // Show sign of hidden crumbs like this:
    // root > some di... > ... > current directory.
    parentCrumb.setAttribute('collapsed', '');
    collapsedWidth = Math.min(maxPathWidth, parentCrumb.clientWidth);
    maxPathWidth -= collapsedWidth;
    if (parentCrumb.clientWidth != collapsedWidth)
      parentCrumb.style.width = collapsedWidth + 'px';

    lastSeparator = parentCrumb.previousSibling;
    if (!lastSeparator)
      return;
    collapsedWidth += lastSeparator.clientWidth;
    maxPathWidth = Math.max(0, maxPathWidth - lastSeparator.clientWidth);
  }

  pathWidth = 0;
  for (var item = this.bc_.firstChild; item != lastSeparator;
       item = item.nextSibling) {
    // TODO(serya): Mixing access item.clientWidth and modifying style and
    // attributes could cause multiple layout reflows.
    if (pathWidth + item.clientWidth <= maxPathWidth) {
      pathWidth += item.clientWidth;
    } else if (pathWidth == maxPathWidth) {
      item.style.width = '0';
    } else if (item.classList.contains('separator')) {
      // Do not truncate separator. Instead let the last crumb be longer.
      item.style.width = '0';
      maxPathWidth = pathWidth;
    } else {
      // Truncate the last visible crumb.
      item.style.width = (maxPathWidth - pathWidth) + 'px';
      pathWidth = maxPathWidth;
    }
  }

  currentWidth = Math.min(currentWidth,
                          containerWidth - pathWidth - collapsedWidth);
  this.bc_.lastChild.style.width =
      (currentWidth - lastCrumbSeparatorWidth) + 'px';
};

/**
 * Show breadcrumbs.
 * @param {string} rootPath Path to root.
 * @param {string} path Path to directory.
 */
BreadcrumbsController.prototype.show = function(rootPath, path) {
  console.log('show', rootPath, path);
  this.bc_.style.display = '-webkit-box';
  this.update(rootPath, path);
};

/**
 * Hide breadcrumbs div.
 */
BreadcrumbsController.prototype.hide = function() {
  this.bc_.style.display = 'none';
};

/**
 * Handle a click event on a breadcrumb element.
 * @private
 * @param {Event} event The click event.
 */
BreadcrumbsController.prototype.onClick_ = function(event) {
  var newEvent = new cr.Event('pathclick');
  newEvent.path = event.srcElement.path;
  this.dispatchEvent(newEvent);
};

