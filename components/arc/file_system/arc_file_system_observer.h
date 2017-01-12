// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_FILE_SYSTEM_ARC_FILE_SYSTEM_OBSERVER_H_
#define COMPONENTS_ARC_FILE_SYSTEM_ARC_FILE_SYSTEM_OBSERVER_H_

namespace arc {

class ArcFileSystemObserver {
 public:
  virtual ~ArcFileSystemObserver() = default;

  // Called when ARC file systems are ready.
  virtual void OnFileSystemsReady() = 0;

  // Called when ARC file systems are closed.
  virtual void OnFileSystemsClosed() = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_FILE_SYSTEM_ARC_FILE_SYSTEM_OBSERVER_H_
