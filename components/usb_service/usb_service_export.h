// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USB_SERVICE_USB_SERVICE_EXPORT_H_
#define COMPONENTS_USB_SERVICE_USB_SERVICE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(USB_SERVICE_IMPLEMENTATION)
#define USB_SERVICE_EXPORT __declspec(dllexport)
#else
#define USB_SERVICE_EXPORT __declspec(dllimport)
#endif  // defined(USB_SERVICE_EXPORT)

#else  // defined(WIN32)
#if defined(USB_SERVICE_IMPLEMENTATION)
#define USB_SERVICE_EXPORT __attribute__((visibility("default")))
#else
#define USB_SERVICE_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define USB_SERVICE_EXPORT
#endif

#endif  // COMPONENTS_USB_SERVICE_USB_SERVICE_EXPORT_H_
