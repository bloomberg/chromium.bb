// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_CONTENT_CLIENT_H_
#define CONTENT_TEST_TEST_CONTENT_CLIENT_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "content/public/common/content_client.h"
#include "ui/base/resource/data_pack.h"

namespace content {

class TestContentClient : public ContentClient {
 public:
  TestContentClient();
  virtual ~TestContentClient();

  // ContentClient:
  virtual std::string GetUserAgent() const OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const OVERRIDE;

 private:
  ui::DataPack data_pack_;

  DISALLOW_COPY_AND_ASSIGN(TestContentClient);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_CONTENT_CLIENT_H_
