// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements some utilities for the legacy 'inline' and 'overlay'
 * UI modes.
 **/

goog.provide('google.cf.installer.frame');

/**
 * Plucks properties from the passed arguments and sets them on the passed
 * DOM node
 * @param {Node} node The node to set properties on
 * @param {Object} args A map of user-specified properties to set
 */
google.cf.installer.frame.setProperties = function(node, args) {
  var cssText = args['cssText'] || '';
  node.style.cssText = ' ' + cssText;

  var classText = args['className'] || '';
  node.className = classText;

  var srcNode = args['node'];
  if (typeof srcNode == 'string')
    srcNode = document.getElementById(srcNode);
  return args['id'] || (srcNode ? srcNode['id'] || '' : '');
};

/**
 * Determines the parent node to create the IFrame in, based on the provided
 * arguments. Note that this should only be called once.
 * @param {Object} args A map of user-specified properties.
 */
google.cf.installer.frame.getParentNode = function(args) {
  var srcNode = args['node'];
  if (typeof srcNode == 'string')
    srcNode = document.getElementById(srcNode);
  if (srcNode) {
    var parentNode = srcNode.parentNode;
    parentNode.removeChild(srcNode);
    return parentNode;
  }
  return document.body;
};