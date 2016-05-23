// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "ios/mojo/public/js/handle_util"
//
// This module provides utilities to work with mojo handles.
// JavaScript and native code use different types for Mojo handles.
// In JavaScript handles are objects of type
// {nativeHandle: number, isValidMojoHandle: bool}. In native code handle is
// an integer and corresponds to {@code nativeHandle } field of JavaScript
// handle object.

define("ios/mojo/public/js/handle_util", [], function() {

  /**
   * Returns JavaScript handle from the given native handle.
   * @param {!MojoHandle} Native mojo handle to convert.
   * @return {!Object} JavaScript Mojo handle.
   */
  function getJavaScriptHandle(nativeHandle) {
    return {
      nativeHandle: nativeHandle,
      isValidMojoHandle: true
    };
  };

  /**
   * Returns native handle from the given JavaScript handle.
   * @param {Object} javaScriptHandle Handle to convert.
   * @return {MojoHandle} native Mojo handle.
   */
  function getNativeHandle(javaScriptHandle) {
    return javaScriptHandle ? javaScriptHandle.nativeHandle : null;
  };

  /**
   * Returns native handles from the given JavaScript handles.
   * @param {!Array<Object>} javaScriptHandles Handles to unwrap.
   * @return {!Array<MojoHandle>} array of native Mojo handles.
   */
  function getNativeHandles(javaScriptHandles) {
    return javaScriptHandles.map(function(handle) {
      return getNativeHandle(handle);
    });
  };

  /**
   * @param {!Object} JavaScript handle to check.
   * @return {boolean} true if the given JavaScript handle is a valid handle.
   */
  function isValidHandle(handle) {
    return handle.isValidMojoHandle;
  };

  /**
   * Invalidates the given JavaScript handle.
   *
   * Subsequent {@code isValidHandle} calls will return true for this handle.
   * @param {!Object} handle JavaScript handle to invalidate.
   */
  function invalidateHandle(handle) {
    handle.isValidMojoHandle = false;
  };

  var exports = {};
  exports.getJavaScriptHandle = getJavaScriptHandle;
  exports.getNativeHandle = getNativeHandle;
  exports.getNativeHandles = getNativeHandles;
  exports.isValidHandle = isValidHandle;
  exports.invalidateHandle = invalidateHandle;
  return exports;
});
