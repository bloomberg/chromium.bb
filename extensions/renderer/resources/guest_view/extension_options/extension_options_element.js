// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The <extensionoptions> custom element.

var registerElement = require('guestViewContainerElement').registerElement;
var GuestViewContainerElement =
    require('guestViewContainerElement').GuestViewContainerElement;
var ExtensionOptionsImpl = require('extensionOptions').ExtensionOptionsImpl;

class ExtensionOptionsElement extends GuestViewContainerElement {}

registerElement(
    'ExtensionOptions', ExtensionOptionsElement, ExtensionOptionsImpl);
