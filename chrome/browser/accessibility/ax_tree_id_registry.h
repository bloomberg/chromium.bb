// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_
#define CHROME_BROWSER_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_

#include <map>

#include "base/stl_util.h"

template <typename T>
struct DefaultSingletonTraits;

// A class which generates a unique id given a process id and routing id.
// Currently, placeholder implementation pending finalization of out of process
// iframes.
class AXTreeIDRegistry {
 public:
  typedef int AXTreeID;
  typedef std::pair<int, int> FrameID;

  // Get the single instance of this class.
  static AXTreeIDRegistry* GetInstance();

  // Obtains a unique id given a |process_id| and |routing_id|. Placeholder
  // for full implementation once out of process iframe accessibility finalizes.
  int GetOrCreateAXTreeID(int process_id, int routing_id);
  FrameID GetFrameID(int ax_tree_id);
  void RemoveAXTreeID(int ax_tree_id);

 private:
  friend struct DefaultSingletonTraits<AXTreeIDRegistry>;

  AXTreeIDRegistry();
  virtual ~AXTreeIDRegistry();

  // Tracks the current unique ax frame id.
  int ax_tree_id_counter_;

  // Maps an accessibility tree to its frame via ids.
  std::map<AXTreeID, FrameID> ax_tree_to_frame_id_map_;

  // Maps frames to an accessibility tree via ids.
  std::map<FrameID, AXTreeID> frame_to_ax_tree_id_map_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeIDRegistry);
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_
