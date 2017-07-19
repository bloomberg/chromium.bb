// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_SOURCE_H_
#define COMPONENTS_EXO_DATA_SOURCE_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/macros.h"

namespace exo {

class DataSourceDelegate;
enum class DndAction;

// Object representing transferred data offered by a client.
class DataSource {
 public:
  explicit DataSource(DataSourceDelegate* delegate);
  ~DataSource();

  // Adds an offered mime type.
  void Offer(const std::string& mime_type);

  // Sets the available drag-and-drop actions.
  void SetActions(const base::flat_set<DndAction>& dnd_actions);

 private:
  DataSourceDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(DataSource);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_SOURCE_H_
