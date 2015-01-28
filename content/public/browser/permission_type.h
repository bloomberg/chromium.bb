// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_TYPE_H_

namespace content {

// This enum is also used for UMA purposes, so it needs to adhere to
// the UMA guidelines.
// Make sure you update histograms.xml if you add new permission types.
// Never delete or reorder an entry; only add new entries
// immediately before PERMISSION_NUM
enum PermissionType {
  PERMISSION_MIDI_SYSEX = 1,
  PERMISSION_PUSH_MESSAGING = 2,
  PERMISSION_NOTIFICATIONS = 3,
  PERMISSION_GEOLOCATION = 4,
  PERMISSION_PROTECTED_MEDIA_IDENTIFIER = 5,

  // Always keep this at the end.
  PERMISSION_NUM,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_TYPE_H_
