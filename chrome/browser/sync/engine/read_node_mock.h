// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_READ_NODE_MOCK_H_
#define CHROME_BROWSER_SYNC_ENGINE_READ_NODE_MOCK_H_
#pragma once

#include <string>

#include "chrome/browser/sync/engine/syncapi.h"
#include "testing/gmock/include/gmock/gmock.h"

class ReadNodeMock : public sync_api::ReadNode {
 public:
  ReadNodeMock();
  virtual ~ReadNodeMock();

  MOCK_METHOD2(InitByClientTagLookup,
               bool(syncable::ModelType model_type, const std::string& tag));
  MOCK_CONST_METHOD0(GetAutofillProfileSpecifics,
                     const sync_pb::AutofillProfileSpecifics&());
  MOCK_CONST_METHOD0(GetId, int64());
  MOCK_CONST_METHOD0(GetFirstChildId, int64());
  MOCK_CONST_METHOD0(GetFirstChild, int64());
  MOCK_CONST_METHOD0(GetSuccessorId, int64());
  MOCK_METHOD1(InitByIdLookup, bool(int64 id));
};

#endif  // CHROME_BROWSER_SYNC_ENGINE_READ_NODE_MOCK_H_

