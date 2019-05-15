// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_REMOTE_COCOA_APP_SHIM_EXPORT_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_REMOTE_COCOA_APP_SHIM_EXPORT_H_

// Defines REMOTE_COCOA_APP_SHIM_EXPORT so that functionality implemented by the
// RemoteMacViews module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(VIEWS_BRIDGE_MAC_IMPLEMENTATION)
#define REMOTE_COCOA_APP_SHIM_EXPORT __declspec(dllexport)
#else
#define REMOTE_COCOA_APP_SHIM_EXPORT __declspec(dllimport)
#endif  // defined(VIEWS_BRIDGE_MAC_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(VIEWS_BRIDGE_MAC_IMPLEMENTATION)
#define REMOTE_COCOA_APP_SHIM_EXPORT __attribute__((visibility("default")))
#else
#define REMOTE_COCOA_APP_SHIM_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define REMOTE_COCOA_APP_SHIM_EXPORT
#endif

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_REMOTE_COCOA_APP_SHIM_EXPORT_H_
