// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_MOCK_H__
#pragma once

#include "chrome/browser/sync/glue/model_associator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class ModelAssociatorMock : public AssociatorInterface {
 public:
  ModelAssociatorMock();
  virtual ~ModelAssociatorMock();

  MOCK_METHOD0(AssociateModels, bool());
  MOCK_METHOD0(DisassociateModels, bool());
  MOCK_METHOD1(SyncModelHasUserCreatedNodes, bool(bool* has_nodes));
  MOCK_METHOD0(AbortAssociation, void());
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_MOCK_H__
