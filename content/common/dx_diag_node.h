// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A tree of name value pairs that report contain DirectX diagnostic
// information.

#ifndef CONTENT_COMMON_DX_DIAG_NODE_H_
#define CONTENT_COMMON_DX_DIAG_NODE_H_

#include <map>
#include <string>

struct DxDiagNode {
  DxDiagNode();
  ~DxDiagNode();

  std::map<std::string, std::string> values;
  std::map<std::string, DxDiagNode> children;
};

#endif  // CONTENT_COMMON_DX_DIAG_NODE_H_
