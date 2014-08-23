// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_CONSTANTS_H_
#define CHROME_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_CONSTANTS_H_

namespace extensionoptions {

// API namespace.
extern const char kAPINamespace[];

// Attributes.
extern const char kAttributeAutoSize[];
extern const char kAttributeMaxHeight[];
extern const char kAttributeMaxWidth[];
extern const char kAttributeMinHeight[];
extern const char kAttributeMinWidth[];

extern const char kExtensionId[];

// SizeChanged event properties.
extern const char kNewHeight[];
extern const char kNewWidth[];
extern const char kOldHeight[];
extern const char kOldWidth[];

}  // namespace extensionoptions

#endif  // CHROME_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_CONSTANTS_H_
