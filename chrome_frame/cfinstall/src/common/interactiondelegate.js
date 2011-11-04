// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Describes an interface clients may implement to customize the
 * look and feel of the Chrome Frame installation flow on their site.
 *
 */

goog.provide('google.cf.installer.InteractionDelegate');

/**
 * Allows clients to customize the display of the IFrame holding the Chrome
 * Frame installation flow.
 * @interface
 */
google.cf.installer.InteractionDelegate = function() {};

/**
 * Returns the element under which the IFrame should be located. Note that the
 * IFrame must remain in the DOM for the duration of the connection.
 * Re-parenting the IFrame or any ancestor will cause undefined behaviour.
 * @return {!HTMLElement} The element that should be the parent of the IFrame.
 */
google.cf.installer.InteractionDelegate.prototype.getIFrameContainer =
    function() {};

/**
 * Receives a reference to the newly created IFrame. May optionally modify the
 * element's attributes, style, etc. The InteractionDelegate is not responsible
 * for inserting the IFrame in the DOM (this will occur later).
 * @param {!HTMLIFrameElement} iframe The newly created IFrame element.
 */
google.cf.installer.InteractionDelegate.prototype.customizeIFrame =
    function(iframe) {};

/**
 * Make the IFrame and its decoration (if any) visible. Its dimensions will
 * already have been adjusted to those of the contained content.
 */
google.cf.installer.InteractionDelegate.prototype.show = function() {};


/**
 * Receives notification that the installation flow is complete and that the
 * IFrame and its decoration (if any) should be hidden and resources freed.
 */
google.cf.installer.InteractionDelegate.prototype.reset = function() {};
