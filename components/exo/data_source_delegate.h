// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_SOURCE_DELEGATE_H_
#define COMPONENTS_EXO_DATA_SOURCE_DELEGATE_H_

#include <string>

#include "base/files/scoped_file.h"

namespace exo {

class DataSource;

// Handles events on data devices in context-specific ways.
class DataSourceDelegate {
 public:
  // Called at the top of the data device's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnDataSourceDestroying(DataSource* source) = 0;

  // Called when a target accepts an offered mime type.
  virtual void OnTarget(const std::string& mime_type) = 0;

  // Called when the data is requested.
  virtual void OnSend(const std::string& mime_type, base::ScopedFD fd) = 0;

  // Called when selection was cancelled.
  virtual void OnCancelled() = 0;

  // Called when the drag-and-drop operation physically finished.
  virtual void OnDndDropPerformed() = 0;

  // Called when the drag-and-drop operation concluded.
  virtual void OnDndFinished() = 0;

  // Called when the action is selected by the compositor.
  virtual void OnAction(DndAction dnd_action) = 0;

 protected:
  virtual ~DataSourceDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_SOURCE_DELEGATE_H_
