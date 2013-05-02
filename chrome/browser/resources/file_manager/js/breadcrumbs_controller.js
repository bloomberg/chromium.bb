// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @extends cr.EventTarget
 * @param {HTMLDivElement} div Div container for breadcrumbs.
 * @param {MetadataCache} metadataCache To retrieve metadata.
 * @constructor
 */
function BreadcrumbsController(div, metadataCache) {
  this.metadataCache_ = metadataCache;
  this.bc_ = div;
  this.hideLast_ = false;
  this.rootPath_ = null;
  this.path_ = null;
  this.rootPathOverride_ = null;
  div.addEventListener('click', this.onClick_.bind(this));
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
 * TODO(haruki): Remove |rootPath|. It only needs |path|, as |rootPath| can be
 * derived.
 *
 * @param {string} rootPath Path to root.
 * @param {string} path Path to directory.
 */
BreadcrumbsController.prototype.update = function(rootPath, path) {
  if (path == this.path_)
    return;

  // Clear the content and store |path|.
  this.bc_.textContent = '';
  this.rootPath_ = rootPath;
  this.path_ = path;

  // determineRootPathOverride_() may call MetadataCache to retrieve
  // shared-with-me property and call the callback asynchronously.
  this.determineRootPathOverride_(
      rootPath, path, function(requestedPath, rootPathOverride) {
        if (requestedPath != path) {
          // Another update() is called. Ignore this old callback.
          return;
        }
        this.rootPathOverride_ = rootPathOverride;
        this.updateInternal_(rootPath, path, rootPathOverride);
      }.bind(this));
};

/**
 * Check the given path and determines if the breadcrumb should show "Shared
 * with me" as the first crumb. RootDirectory.DRIVE_SHARED_WITH_ME is used as
 * the first crumb if |path| is a shared-with-me directory outside "My Drive".
 * See updateInternal_().
 *
 * @param {string} rootPath Path to root.
 * @param {string} path Path to directory.
 * @param {function(string, !string)} callback Function to call with |path| and
 *     the determined root path override.
 * @private
 */
BreadcrumbsController.prototype.determineRootPathOverride_ =
    function(rootPath, path, callback) {
  if (rootPath != RootDirectory.DRIVE + '/' + DriveSubRootDirectory.OTHER ||
      rootPath == path) {
    callback(path, null);  // No need for Drive properties.
    return;
  }

  var delimiterPos = path.indexOf('/', rootPath.length + 1);
  var firstDirPath = delimiterPos > 0 ? path.substring(0, delimiterPos) : path;
  var entryUrl = util.makeFilesystemUrl(firstDirPath);

  this.metadataCache_.getOne(entryUrl, 'drive', function(result) {
    callback(path,
             result && result.sharedWithMe ?
                 RootDirectory.DRIVE_SHARED_WITH_ME :
                 null);
  });
};

/**
 * Update the breadcrumb display.
 * @private
 */
BreadcrumbsController.prototype.updateInternal_ = function() {
  var relativePath =
      this.path_.substring(this.rootPath_.length).replace(/\/$/, '');
  var pathNames = relativePath.split('/');
  if (pathNames[0] == '')
    pathNames.splice(0, 1);

  // We need a first breadcrumb for root, so placing last name from
  // rootPath as first name of relativePath.
  var rootPathNames = this.rootPath_.replace(/\/$/, '').split('/');
  pathNames.splice(0, 0, rootPathNames[rootPathNames.length - 1]);
  rootPathNames.splice(rootPathNames.length - 1, 1);
  var path = rootPathNames.join('/') + '/';

  var doc = this.bc_.ownerDocument;

  var divAdded = false;
  for (var i = 0;
       i < (this.hideLast_ ? pathNames.length - 1 : pathNames.length);
       i++) {
    if (divAdded) {
      var spacer = doc.createElement('div');
      spacer.className = 'separator';
      this.bc_.appendChild(spacer);
    }
    var pathName = pathNames[i];
    path += pathName;

    // We have a special case for the root crumb /drive/other, which contains
    // the Drive entries not in "My Drive". It is hidden as this path is not
    // meaningful for the user.
    if (i == 0 &&
        path === RootDirectory.DRIVE + '/' + DriveSubRootDirectory.OTHER &&
        this.rootPathOverride_ == null) {
        continue;
    }

    // Create a crumb.
    var div = doc.createElement('div');
    div.className = 'breadcrumb-path';
    div.textContent = i == 0 ?
        PathUtil.getRootLabel(this.rootPathOverride_ || path) :
        pathName;

    path = path + '/';

    this.bc_.appendChild(div);
    divAdded = true;

    if (i == pathNames.length - 1)
      div.classList.add('breadcrumb-last');
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
 * @param {Event} event The click event.
 * @private
 */
BreadcrumbsController.prototype.onClick_ = function(event) {
  var path = this.getTargetPath(event);
  if (!path)
    return;

  var newEvent = new cr.Event('pathclick');
  newEvent.path = path;
  this.dispatchEvent(newEvent);
};

/**
 * Returns path associated with the event target. Returns empty string for
 * inactive elements: separators, empty space and the last chunk.
 * @param {Event} event The UI event.
 * @return {string} Full path or empty string.
 */
BreadcrumbsController.prototype.getTargetPath = function(event) {
  if (!event.target.classList.contains('breadcrumb-path') ||
      event.target.classList.contains('breadcrumb-last')) {
    return '';
  }

  var items = this.bc_.querySelectorAll('.breadcrumb-path');
  var path = this.rootPath_;

  if (event.target == items[0]) {
    // The first crumb can be overridden.
    return this.rootPathOverride_ || path;
  } else {
    for (var i = 1; items[i - 1] != event.target; i++) {
      path += '/' + items[i].textContent;
    }
  }
  return path;
};

/**
 * Returns the breadcrumbs container.
 * @return {HTMLElement} Breadcumbs container HTML element.
 */
BreadcrumbsController.prototype.getContainer = function() {
  return this.bc_;
};
