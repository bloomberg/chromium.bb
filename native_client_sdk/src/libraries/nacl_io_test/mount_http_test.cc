/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <gmock/gmock.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_instance.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/mount_http.h"
#include "nacl_io/mount_node_dir.h"
#include "nacl_io/osdirent.h"
#include "pepper_interface_mock.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

class MountHttpTest : public ::testing::Test {
 public:
  MountHttpTest() {
    ki_init(NULL);
  }

  ~MountHttpTest() {
    ki_uninit();
  }
};

class MountHttpMock : public MountHttp {
 public:
  MountHttpMock() : MountHttp() {};

  virtual bool Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
    return MountHttp::Init(dev, args, ppapi);
  }

  bool ParseManifest(char *txt) {
    return MountHttp::ParseManifest(txt);
  }

  MountNodeDir* FindOrCreateDir(const Path& path) {
    return MountHttp::FindOrCreateDir(path);
  }

  NodeMap_t& GetMap() { return node_cache_; }

  friend class MountHttpTest;
};

TEST_F(MountHttpTest, MountEmpty) {
  MountHttpMock mnt;
  StringMap_t args;

  EXPECT_TRUE(mnt.Init(1, args, NULL));
}

TEST_F(MountHttpTest, ParseManifest) {
  MountHttpMock mnt;
  StringMap_t args;

  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  EXPECT_TRUE(mnt.Init(1, args, NULL));
  EXPECT_TRUE(mnt.ParseManifest(manifest));

  MountNodeDir* root = mnt.FindOrCreateDir(Path("/"));
  EXPECT_EQ(2, root->ChildCount());

  MountNodeDir* dir = mnt.FindOrCreateDir(Path("/mydir"));
  EXPECT_EQ(1, dir->ChildCount());

  MountNode* node = mnt.GetMap()["/mydir/foo"];
  EXPECT_TRUE(node);
  EXPECT_EQ(123, node->GetSize());

  // Since these files are cached thanks to the manifest, we can open them
  // without accessing the PPAPI URL API.
  MountNode* foo = mnt.Open(Path("/mydir/foo"), O_RDONLY);
  MountNode* bar = mnt.Open(Path("/thatdir/bar"), O_RDWR);

  struct stat sfoo;
  struct stat sbar;

  EXPECT_FALSE(foo->GetStat(&sfoo));
  EXPECT_FALSE(bar->GetStat(&sbar));

  EXPECT_EQ(123, sfoo.st_size);
  EXPECT_EQ(S_IFREG | S_IREAD, sfoo.st_mode);

  EXPECT_EQ(234, sbar.st_size);
  EXPECT_EQ(S_IFREG | S_IREAD | S_IWRITE, sbar.st_mode);
}
