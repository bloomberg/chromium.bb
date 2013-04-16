// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_DEPENDENCY_NODE_H_
#define CHROME_BROWSER_PROFILES_DEPENDENCY_NODE_H_

// Base class representing a node in a DependencyGraph.
class DependencyNode {
 protected:
  // This is intended to be used by the subclasses, not directly.
  DependencyNode() {}
  ~DependencyNode() {}
};

#endif  // CHROME_BROWSER_PROFILES_DEPENDENCY_NODE_H_
