// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_DOM_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_DOM_TRACKER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/test/chromedriver/devtools_event_listener.h"

namespace base {
class DictionaryValue;
class Value;
}

class Status;

// Tracks the state of the DOM and execution context creation.
class DomTracker : public DevToolsEventListener {
 public:
  DomTracker();
  virtual ~DomTracker();

  Status GetFrameIdForNode(int node_id, std::string* frame_id);
  Status GetContextIdForFrame(const std::string& frame_id, int* context_id);

  // Overridden from DevToolsEventListener:
  virtual void OnEvent(const std::string& method,
                       const base::DictionaryValue& params) OVERRIDE;

 private:
  bool ProcessNodeList(const base::Value* nodes);
  bool ProcessNode(const base::Value* node);

  typedef std::map<int, std::string> NodeToFrameMap;
  typedef std::map<std::string, int> FrameToContextMap;
  NodeToFrameMap node_to_frame_map_;
  FrameToContextMap frame_to_context_map_;

  DISALLOW_COPY_AND_ASSIGN(DomTracker);
};

#endif  // CHROME_TEST_CHROMEDRIVER_DOM_TRACKER_H_
