# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [{
    'target_name': 'usb_service',
    'type': '<(component)',
    'dependencies': [
      '../base/base.gyp:base',
      '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      '../content/content.gyp:content_browser',
      '../net/net.gyp:net',
      '../third_party/libusb/libusb.gyp:libusb',
    ],
    'defines': [
      'USB_SERVICE_IMPLEMENTATION',
    ],
    'include_dirs': [
      '..',
    ],
    'sources': [
      'usb_service/usb_context.cc',
      'usb_service/usb_context.h',
      'usb_service/usb_device.cc',
      'usb_service/usb_device.h',
      'usb_service/usb_device_handle.cc',
      'usb_service/usb_device_handle.h',
      'usb_service/usb_interface.cc',
      'usb_service/usb_interface.h',
      'usb_service/usb_service.cc',
      'usb_service/usb_service.h',
    ]
  }],
}
