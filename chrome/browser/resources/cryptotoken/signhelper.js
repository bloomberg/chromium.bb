// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a "bottom half" helper to assist with raw sign
 * requests.
 */
'use strict';

/**
 * A helper for sign requests.
 * @extends {Closeable}
 * @interface
 */
function SignHelper() {}

/**
 * Attempts to sign the provided challenges.
 * @param {SignHelperRequest} request The sign request.
 * @param {function(SignHelperReply, string=)} cb Called with the result of the
 *     sign request and an optional source for the sign result.
 * @return {boolean} Whether the challenges were successfully added.
 */
SignHelper.prototype.doSign = function(request, cb) {};

/** Closes this helper. */
SignHelper.prototype.close = function() {};

/**
 * A factory for creating sign helpers.
 * @interface
 */
function SignHelperFactory() {}

/**
 * Creates a new sign helper.
 * @return {SignHelper} The newly created helper.
 */
SignHelperFactory.prototype.createHelper = function() {};
