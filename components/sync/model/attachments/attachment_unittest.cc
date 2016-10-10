// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/attachments/attachment.h"

#include <string>

#include "components/sync/engine/attachments/attachment_util.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

const char kAttachmentData[] = "some data";

}  // namespace

class AttachmentTest : public testing::Test {};

TEST_F(AttachmentTest, Create_UniqueIdIsUnique) {
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment a1 = Attachment::Create(some_data);
  Attachment a2 = Attachment::Create(some_data);
  EXPECT_NE(a1.GetId(), a2.GetId());
  EXPECT_EQ(a1.GetData(), a2.GetData());
}

TEST_F(AttachmentTest, Create_WithEmptyData) {
  scoped_refptr<base::RefCountedString> empty_data(new base::RefCountedString);
  Attachment a = Attachment::Create(empty_data);
  EXPECT_EQ(empty_data, a.GetData());
}

TEST_F(AttachmentTest, CreateFromParts_HappyCase) {
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  uint32_t crc32c = ComputeCrc32c(some_data);
  AttachmentId id = AttachmentId::Create(some_data->size(), crc32c);
  Attachment a = Attachment::CreateFromParts(id, some_data);
  EXPECT_EQ(id, a.GetId());
  EXPECT_EQ(some_data, a.GetData());
}

}  // namespace syncer
