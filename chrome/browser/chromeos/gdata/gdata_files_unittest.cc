// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_files.h"

#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_directory_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {
namespace {

const char kResumableEditMediaUrl[] = "http://resumable-edit-media/";

}

TEST(GDataEntryTest, FromProto_DetectBadUploadUrl) {
  GDataEntryProto proto;
  proto.set_title("test.txt");

  GDataDirectoryService directory_service;

  scoped_ptr<GDataEntry> entry(directory_service.CreateGDataFile());
  // This should fail as the upload URL is empty.
  ASSERT_FALSE(entry->FromProto(proto));

  // Set a upload URL.
  proto.set_upload_url(kResumableEditMediaUrl);

  // This should succeed as the upload URL is set.
  ASSERT_TRUE(entry->FromProto(proto));
  EXPECT_EQ(kResumableEditMediaUrl, entry->upload_url().spec());
}

}  // namespace gdata
