// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <gmock/gmock.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_instance.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "fake_pepper_interface_url_loader.h"

#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/mount_http.h"
#include "nacl_io/mount_node_dir.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/osunistd.h"

using namespace nacl_io;

namespace {

class MountHttpForTesting : public MountHttp {
 public:
  MountHttpForTesting(StringMap_t map, PepperInterface* ppapi) {
    MountInitArgs args(1);
    args.string_map = map;
    args.ppapi = ppapi;
    EXPECT_EQ(0, Init(args));
  }

  using MountHttp::GetNodeCacheForTesting;
  using MountHttp::ParseManifest;
  using MountHttp::FindOrCreateDir;
};

enum {
  kStringMapParamCacheNone = 0,
  kStringMapParamCacheContent = 1,
  kStringMapParamCacheStat = 2,
  kStringMapParamCacheContentStat =
      kStringMapParamCacheContent | kStringMapParamCacheStat,
};
typedef uint32_t StringMapParam;

StringMap_t MakeStringMap(StringMapParam param) {
  StringMap_t smap;
  if (param & kStringMapParamCacheContent)
    smap["cache_content"] = "true";
  else
    smap["cache_content"] = "false";

  if (param & kStringMapParamCacheStat)
    smap["cache_stat"] = "true";
  else
    smap["cache_stat"] = "false";
  return smap;
}

class MountHttpTest : public ::testing::TestWithParam<StringMapParam> {
 public:
  MountHttpTest();

 protected:
  FakePepperInterfaceURLLoader ppapi_;
  MountHttpForTesting mnt_;
};

MountHttpTest::MountHttpTest() :
    mnt_(MakeStringMap(GetParam()), &ppapi_) {
}

}  // namespace


TEST_P(MountHttpTest, Access) {
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("foo", "", NULL));

  ASSERT_EQ(0, mnt_.Access(Path("/foo"), R_OK));
  ASSERT_EQ(EACCES, mnt_.Access(Path("/foo"), W_OK));
  ASSERT_EQ(EACCES, mnt_.Access(Path("/foo"), X_OK));
  ASSERT_EQ(ENOENT, mnt_.Access(Path("/bar"), F_OK));
}

TEST_P(MountHttpTest, OpenAndCloseServerError) {
  EXPECT_TRUE(ppapi_.server_template()->AddError("file", 500));

  ScopedMountNode node;
  ASSERT_EQ(EIO, mnt_.Open(Path("/file"), O_RDONLY, &node));
}

TEST_P(MountHttpTest, ReadPartial) {
  const char contents[] = "0123456789abcdefg";
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("file", contents, NULL));
  ppapi_.server_template()->set_allow_partial(true);

  int result_bytes = 0;

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ScopedMountNode node;
  ASSERT_EQ(0, mnt_.Open(Path("/file"), O_RDONLY, &node));
  HandleAttr attr;
  EXPECT_EQ(0, node->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_EQ(sizeof(buf) - 1, result_bytes);
  EXPECT_STREQ("012345678", &buf[0]);

  // Read is clamped when reading past the end of the file.
  attr.offs = 10;
  ASSERT_EQ(0, node->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  ASSERT_EQ(strlen("abcdefg"), result_bytes);
  buf[result_bytes] = 0;
  EXPECT_STREQ("abcdefg", &buf[0]);

  // Read nothing when starting past the end of the file.
  attr.offs = 100;
  EXPECT_EQ(0, node->Read(attr, &buf[0], sizeof(buf), &result_bytes));
  EXPECT_EQ(0, result_bytes);
}

TEST_P(MountHttpTest, ReadPartialNoServerSupport) {
  const char contents[] = "0123456789abcdefg";
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("file", contents, NULL));
  ppapi_.server_template()->set_allow_partial(false);

  int result_bytes = 0;

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ScopedMountNode node;
  ASSERT_EQ(0, mnt_.Open(Path("/file"), O_RDONLY, &node));
  HandleAttr attr;
  EXPECT_EQ(0, node->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_EQ(sizeof(buf) - 1, result_bytes);
  EXPECT_STREQ("012345678", &buf[0]);

  // Read is clamped when reading past the end of the file.
  attr.offs = 10;
  ASSERT_EQ(0, node->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  ASSERT_EQ(strlen("abcdefg"), result_bytes);
  buf[result_bytes] = 0;
  EXPECT_STREQ("abcdefg", &buf[0]);

  // Read nothing when starting past the end of the file.
  attr.offs = 100;
  EXPECT_EQ(0, node->Read(attr, &buf[0], sizeof(buf), &result_bytes));
  EXPECT_EQ(0, result_bytes);
}

TEST_P(MountHttpTest, Write) {
  const char contents[] = "contents";
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("file", contents, NULL));

  ScopedMountNode node;
  ASSERT_EQ(0, mnt_.Open(Path("/file"), O_WRONLY, &node));

  // Writing always fails.
  HandleAttr attr;
  attr.offs = 3;
  int bytes_written = 1;  // Set to a non-zero value.
  EXPECT_EQ(EACCES, node->Write(attr, "struct", 6, &bytes_written));
  EXPECT_EQ(0, bytes_written);
}

TEST_P(MountHttpTest, GetStat) {
  const char contents[] = "contents";
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("file", contents, NULL));

  ScopedMountNode node;
  ASSERT_EQ(0, mnt_.Open(Path("/file"), O_RDONLY, &node));

  struct stat statbuf;
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_EQ(S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
            statbuf.st_mode);
  EXPECT_EQ(strlen(contents), statbuf.st_size);
  // These are not currently set.
  EXPECT_EQ(0, statbuf.st_atime);
  EXPECT_EQ(0, statbuf.st_ctime);
  EXPECT_EQ(0, statbuf.st_mtime);
}

TEST_P(MountHttpTest, FTruncate) {
  const char contents[] = "contents";
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("file", contents, NULL));

  ScopedMountNode node;
  ASSERT_EQ(0, mnt_.Open(Path("/file"), O_RDWR, &node));
  EXPECT_EQ(EACCES, node->FTruncate(4));
}

// Instantiate the above tests for all caching types.
INSTANTIATE_TEST_CASE_P(
    Default,
    MountHttpTest,
    ::testing::Values((uint32_t)kStringMapParamCacheNone,
                      (uint32_t)kStringMapParamCacheContent,
                      (uint32_t)kStringMapParamCacheStat,
                      (uint32_t)kStringMapParamCacheContentStat));

TEST(MountHttpDirTest, Mkdir) {
  StringMap_t args;
  MountHttpForTesting mnt(args, NULL);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  ASSERT_EQ(0, mnt.ParseManifest(manifest));
  // mkdir of existing directories should give "File exists".
  EXPECT_EQ(EEXIST, mnt.Mkdir(Path("/"), 0));
  EXPECT_EQ(EEXIST, mnt.Mkdir(Path("/mydir"), 0));
  // mkdir of non-existent directories should give "Permission denied".
  EXPECT_EQ(EACCES, mnt.Mkdir(Path("/non_existent"), 0));
}

TEST(MountHttpDirTest, Rmdir) {
  StringMap_t args;
  MountHttpForTesting mnt(args, NULL);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  ASSERT_EQ(0, mnt.ParseManifest(manifest));
  // Rmdir on existing dirs should give "Permission Denied"
  EXPECT_EQ(EACCES, mnt.Rmdir(Path("/")));
  EXPECT_EQ(EACCES, mnt.Rmdir(Path("/mydir")));
  // Rmdir on existing files should give "Not a direcotory"
  EXPECT_EQ(ENOTDIR, mnt.Rmdir(Path("/mydir/foo")));
  // Rmdir on non-existent files should give "No such file or directory"
  EXPECT_EQ(ENOENT, mnt.Rmdir(Path("/non_existent")));
}

TEST(MountHttpDirTest, Unlink) {
  StringMap_t args;
  MountHttpForTesting mnt(args, NULL);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  ASSERT_EQ(0, mnt.ParseManifest(manifest));
  // Unlink of existing files should give "Permission Denied"
  EXPECT_EQ(EACCES, mnt.Unlink(Path("/mydir/foo")));
  // Unlink of existing directory should give "Is a directory"
  EXPECT_EQ(EISDIR, mnt.Unlink(Path("/mydir")));
  // Unlink of non-existent files should give "No such file or directory"
  EXPECT_EQ(ENOENT, mnt.Unlink(Path("/non_existent")));
}

TEST(MountHttpDirTest, Remove) {
  StringMap_t args;
  MountHttpForTesting mnt(args, NULL);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  ASSERT_EQ(0, mnt.ParseManifest(manifest));
  // Remove of existing files should give "Permission Denied"
  EXPECT_EQ(EACCES, mnt.Remove(Path("/mydir/foo")));
  // Remove of existing directory should give "Permission Denied"
  EXPECT_EQ(EACCES, mnt.Remove(Path("/mydir")));
  // Unlink of non-existent files should give "No such file or directory"
  EXPECT_EQ(ENOENT, mnt.Remove(Path("/non_existent")));
}

TEST(MountHttpDirTest, ParseManifest) {
  StringMap_t args;
  size_t result_size = 0;

  MountHttpForTesting mnt(args, NULL);

  // Multiple consecutive newlines or spaces should be ignored.
  char manifest[] = "-r-- 123 /mydir/foo\n\n-rw-   234  /thatdir/bar\n";
  ASSERT_EQ(0, mnt.ParseManifest(manifest));

  ScopedMountNode root;
  EXPECT_EQ(0, mnt.FindOrCreateDir(Path("/"), &root));
  ASSERT_NE((MountNode*)NULL, root.get());
  EXPECT_EQ(2, root->ChildCount());

  ScopedMountNode dir;
  EXPECT_EQ(0, mnt.FindOrCreateDir(Path("/mydir"), &dir));
  ASSERT_NE((MountNode*)NULL, dir.get());
  EXPECT_EQ(1, dir->ChildCount());

  MountNode* node = (*mnt.GetNodeCacheForTesting())["/mydir/foo"].get();
  EXPECT_NE((MountNode*)NULL, node);
  EXPECT_EQ(0, node->GetSize(&result_size));
  EXPECT_EQ(123, result_size);

  // Since these files are cached thanks to the manifest, we can open them
  // without accessing the PPAPI URL API.
  ScopedMountNode foo;
  ASSERT_EQ(0, mnt.Open(Path("/mydir/foo"), O_RDONLY, &foo));

  ScopedMountNode bar;
  ASSERT_EQ(0, mnt.Open(Path("/thatdir/bar"), O_RDWR, &bar));

  struct stat sfoo;
  struct stat sbar;

  EXPECT_FALSE(foo->GetStat(&sfoo));
  EXPECT_FALSE(bar->GetStat(&sbar));

  EXPECT_EQ(123, sfoo.st_size);
  EXPECT_EQ(S_IFREG | S_IRALL, sfoo.st_mode);

  EXPECT_EQ(234, sbar.st_size);
  EXPECT_EQ(S_IFREG | S_IRALL | S_IWALL, sbar.st_mode);
}
