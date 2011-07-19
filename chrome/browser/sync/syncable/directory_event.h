// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_EVENT_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_EVENT_H_
#pragma once

namespace syncable {

// This kind of Event is emitted when the state of a Directory object changes
// somehow, such as the directory being opened or closed. Don't confuse it with
// a DirectoryChangeEvent, which is what happens when one or more of the Entry
// contents of a Directory have been updated.
enum DirectoryEvent {
  DIRECTORY_CLOSED,
  DIRECTORY_DESTROYED,
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_DIRECTORY_EVENT_H_
