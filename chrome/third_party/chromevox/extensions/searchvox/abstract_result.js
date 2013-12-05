// Copyright 2012 Google Inc. All Rights Reserved.

/**
 * @fileoverview Defines a result type interface.
 * @author peterxiao@google.com (Peter Xiao)
 */

goog.provide('cvox.AbstractResult');

goog.require('cvox.SearchUtil');

/**
 * @constructor
 */
cvox.AbstractResult = function() { };

/**
 * Checks the result if it is an unknown result.
 * @param {Element} result Result to be checked.
 * @return {boolean} Whether or not the element is an unknown result.
 */
cvox.AbstractResult.prototype.isType = function(result) {
  return false;
};

/**
 * Speak a generic search result.
 * @param {Element} result Generic result to be spoken.
 * @return {boolean} Whether or not the result was spoken.
 */
cvox.AbstractResult.prototype.speak = function(result) {
  return false;
};

/**
 * Extracts the wikipedia URL from knowledge panel.
 * @param {Element} result Result to extract from.
 * @return {?string} URL.
 */
cvox.AbstractResult.prototype.getURL = cvox.SearchUtil.extractURL;

/**
 * Returns the node to sync to.
 * @param {Element} result Result.
 * @return {?Node} Node to sync to.
 */
cvox.AbstractResult.prototype.getSyncNode = function(result) {
  return result;
};
