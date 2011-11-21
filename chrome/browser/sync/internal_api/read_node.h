// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_READ_NODE_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_READ_NODE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/internal_api/base_node.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace sync_api {

// ReadNode wraps a syncable::Entry to provide the functionality of a
// read-only BaseNode.
class ReadNode : public BaseNode {
 public:
  // Create an unpopulated ReadNode on the given transaction.  Call some flavor
  // of Init to populate the ReadNode with a database entry.
  explicit ReadNode(const BaseTransaction* transaction);
  virtual ~ReadNode();

  // A client must use one (and only one) of the following Init variants to
  // populate the node.

  // BaseNode implementation.
  virtual bool InitByIdLookup(int64 id) OVERRIDE;
  virtual bool InitByClientTagLookup(syncable::ModelType model_type,
                                     const std::string& tag) OVERRIDE;

  // There is always a root node, so this can't fail.  The root node is
  // never mutable, so root lookup is only possible on a ReadNode.
  void InitByRootLookup();

  // Each server-created permanent node is tagged with a unique string.
  // Look up the node with the particular tag.  If it does not exist,
  // return false.
  bool InitByTagLookup(const std::string& tag);

  // Implementation of BaseNode's abstract virtual accessors.
  virtual const syncable::Entry* GetEntry() const OVERRIDE;
  virtual const BaseTransaction* GetTransaction() const OVERRIDE;

 protected:
  ReadNode();

 private:
  void* operator new(size_t size);  // Node is meant for stack use only.

  // The underlying syncable object which this class wraps.
  syncable::Entry* entry_;

  // The sync API transaction that is the parent of this node.
  const BaseTransaction* transaction_;

  DISALLOW_COPY_AND_ASSIGN(ReadNode);
};

}  // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_READ_NODE_H_
