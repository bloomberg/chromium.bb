// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a "bottom half" helper to assist with raw enroll
 * requests.
 */
'use strict';

/**
 * A helper for enroll requests.
 * @extends {Closeable}
 * @interface
 */
function EnrollHelper() {}

/**
 * Attempts to enroll using the provided data.
 * @param {EnrollHelperRequest} request The enroll helper request.
 * @param {function(EnrollHelperReply)} cb Called back with the result of the
 *     enroll request.
 */
EnrollHelper.prototype.doEnroll = function(request, cb) {};

/** Closes this helper. */
EnrollHelper.prototype.close = function() {};

/**
 * A factory for creating enroll helpers.
 * @interface
 */
function EnrollHelperFactory() {}

/**
 * Creates a new enroll helper.
 * @return {EnrollHelper} the newly created helper.
 */
EnrollHelperFactory.prototype.createHelper = function() {};
