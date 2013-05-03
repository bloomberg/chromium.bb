/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <string.h>
#include <gmock/gmock.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/pp_directory_entry.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_instance.h>
#if defined(WIN32)
#include <windows.h>  // For Sleep()
#endif

#include "mock_util.h"
#include "nacl_io/mount_html5fs.h"
#include "nacl_io/osdirent.h"
#include "pepper_interface_mock.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::WithArgs;

namespace {

class MountHtml5FsMock : public MountHtml5Fs {
 public:
  MountHtml5FsMock(StringMap_t map, PepperInterfaceMock* ppapi) {
    Init(1, map, ppapi);
  }

  ~MountHtml5FsMock() {
    Destroy();
  }
};

class MountHtml5FsTest : public ::testing::Test {
 public:
  MountHtml5FsTest();
  ~MountHtml5FsTest();
  void SetUpFilesystemExpectations(PP_FileSystemType, int,
                                   bool async_callback=false);

 protected:
  PepperInterfaceMock* ppapi_;
  PP_CompletionCallback open_filesystem_callback_;

  static const PP_Instance instance_ = 123;
  static const PP_Resource filesystem_resource_ = 234;
};

MountHtml5FsTest::MountHtml5FsTest()
    : ppapi_(new PepperInterfaceMock(instance_)) {
}

MountHtml5FsTest::~MountHtml5FsTest() {
  delete ppapi_;
}

void MountHtml5FsTest::SetUpFilesystemExpectations(
    PP_FileSystemType fstype,
    int expected_size,
    bool async_callback) {
  FileSystemInterfaceMock* filesystem = ppapi_->GetFileSystemInterface();
  EXPECT_CALL(*filesystem, Create(instance_, fstype))
      .Times(1)
      .WillOnce(Return(filesystem_resource_));

  if (async_callback) {
    EXPECT_CALL(*filesystem, Open(filesystem_resource_, expected_size, _))
        .WillOnce(DoAll(SaveArg<2>(&open_filesystem_callback_),
                        Return(int32_t(PP_OK))));
    EXPECT_CALL(*ppapi_, IsMainThread()).WillOnce(Return(PP_TRUE));
  } else {
    EXPECT_CALL(*filesystem, Open(filesystem_resource_, expected_size, _))
        .WillOnce(CallCallback<2>(int32_t(PP_OK)));
    EXPECT_CALL(*ppapi_, IsMainThread()).WillOnce(Return(PP_FALSE));
  }

  EXPECT_CALL(*ppapi_, ReleaseResource(filesystem_resource_));
}

class MountHtml5FsNodeTest : public MountHtml5FsTest {
 public:
  MountHtml5FsNodeTest();
  virtual void SetUp();
  virtual void TearDown();

  void SetUpNodeExpectations();
  void InitFilesystem();
  void InitNode();

 protected:
  MountHtml5FsMock* mnt_;
  MountNode* node_;

  FileRefInterfaceMock* fileref_;
  FileIoInterfaceMock* fileio_;

  static const char path_[];
  static const PP_Resource fileref_resource_ = 235;
  static const PP_Resource fileio_resource_ = 236;
};

// static
const char MountHtml5FsNodeTest::path_[] = "/foo";

MountHtml5FsNodeTest::MountHtml5FsNodeTest()
    : mnt_(NULL),
      node_(NULL),
      fileref_(NULL),
      fileio_(NULL) {
}

void MountHtml5FsNodeTest::SetUp() {
  fileref_ = ppapi_->GetFileRefInterface();
  fileio_ = ppapi_->GetFileIoInterface();
}

void MountHtml5FsNodeTest::TearDown() {
  if (mnt_) {
    mnt_->ReleaseNode(node_);
    delete mnt_;
  }
}

void MountHtml5FsNodeTest::SetUpNodeExpectations() {
  // Open.
  EXPECT_CALL(*fileref_, Create(filesystem_resource_, StrEq(&path_[0])))
      .WillOnce(Return(fileref_resource_));
  EXPECT_CALL(*fileio_, Create(instance_)).WillOnce(Return(fileio_resource_));
  int32_t open_flags = PP_FILEOPENFLAG_READ | PP_FILEOPENFLAG_WRITE |
      PP_FILEOPENFLAG_CREATE;
  EXPECT_CALL(*fileio_,
              Open(fileio_resource_, fileref_resource_, open_flags, _))
      .WillOnce(Return(int32_t(PP_OK)));

  // Close.
  EXPECT_CALL(*fileio_, Close(fileio_resource_));
  EXPECT_CALL(*ppapi_, ReleaseResource(fileref_resource_));
  EXPECT_CALL(*ppapi_, ReleaseResource(fileio_resource_));
  EXPECT_CALL(*fileio_, Flush(fileio_resource_, _));
}

void MountHtml5FsNodeTest::InitFilesystem() {
  StringMap_t map;
  mnt_ = new MountHtml5FsMock(map, ppapi_);
}

void MountHtml5FsNodeTest::InitNode() {
  node_ = mnt_->Open(Path(path_), O_CREAT | O_RDWR);
  ASSERT_NE((MountNode*)NULL, node_);
}

// Node test where the filesystem is opened synchronously; that is, the
// creation of the mount blocks until the filesystem is ready.
class MountHtml5FsNodeSyncTest : public MountHtml5FsNodeTest {
 public:
  virtual void SetUp();
};

void MountHtml5FsNodeSyncTest::SetUp() {
  MountHtml5FsNodeTest::SetUp();
  SetUpFilesystemExpectations(PP_FILESYSTEMTYPE_LOCALPERSISTENT, 0);
  InitFilesystem();
  SetUpNodeExpectations();
  InitNode();
}

void ReadDirectoryEntriesAction(const PP_ArrayOutput& output) {
  const int fileref_resource_1 = 238;
  const int fileref_resource_2 = 239;

  std::vector<PP_DirectoryEntry> entries;
  PP_DirectoryEntry entry1 = { fileref_resource_1, PP_FILETYPE_REGULAR };
  PP_DirectoryEntry entry2 = { fileref_resource_2, PP_FILETYPE_REGULAR };
  entries.push_back(entry1);
  entries.push_back(entry2);

  void* dest = output.GetDataBuffer(
      output.user_data, 2, sizeof(PP_DirectoryEntry));
  memcpy(dest, &entries[0], sizeof(PP_DirectoryEntry) * 2);
}

class MountHtml5FsNodeAsyncTest : public MountHtml5FsNodeTest {
 public:
  virtual void SetUp();
  virtual void TearDown();

 private:
  static void* ThreadThunk(void* param);
  void Thread();

  enum {
    STATE_INIT,
    STATE_INIT_NODE,
    STATE_INIT_NODE_FINISHED,
  } state_;

  pthread_t thread_;
  pthread_cond_t cond_;
  pthread_mutex_t mutex_;
};

void MountHtml5FsNodeAsyncTest::SetUp() {
  MountHtml5FsNodeTest::SetUp();

  state_ = STATE_INIT;

  pthread_create(&thread_, NULL, &MountHtml5FsNodeAsyncTest::ThreadThunk, this);
  pthread_mutex_init(&mutex_, NULL);
  pthread_cond_init(&cond_, NULL);

  // This test shows that even if the filesystem open callback happens after an
  // attempt to open a node, it still works (opening the node blocks until the
  // filesystem is ready).
  // true => asynchronous filesystem open.
  SetUpFilesystemExpectations(PP_FILESYSTEMTYPE_LOCALPERSISTENT, 0, true);
  InitFilesystem();
  SetUpNodeExpectations();

  // Signal the other thread to try opening a Node.
  pthread_mutex_lock(&mutex_);
  state_ = STATE_INIT_NODE;
  pthread_cond_signal(&cond_);
  pthread_mutex_unlock(&mutex_);

  // Wait for a bit...
  // TODO(binji): this will be flaky. How to test this better?
#if defined(WIN32)
  Sleep(500);  // milliseconds
#else
  usleep(500*1000);  // microseconds
#endif

  // Call the filesystem open callback.
  (*open_filesystem_callback_.func)(open_filesystem_callback_.user_data, PP_OK);

  // Wait for the other thread to unblock and signal us.
  pthread_mutex_lock(&mutex_);
  while (state_ != STATE_INIT_NODE_FINISHED)
    pthread_cond_wait(&cond_, &mutex_);
  pthread_mutex_unlock(&mutex_);
}

void MountHtml5FsNodeAsyncTest::TearDown() {
  pthread_cond_destroy(&cond_);
  pthread_mutex_destroy(&mutex_);

  MountHtml5FsNodeTest::TearDown();
}

void* MountHtml5FsNodeAsyncTest::ThreadThunk(void* param) {
  static_cast<MountHtml5FsNodeAsyncTest*>(param)->Thread();
  return NULL;
}

void MountHtml5FsNodeAsyncTest::Thread() {
  // Wait for the "main" thread to tell us to open the Node.
  pthread_mutex_lock(&mutex_);
  while (state_ != STATE_INIT_NODE)
    pthread_cond_wait(&cond_, &mutex_);
  pthread_mutex_unlock(&mutex_);

  // Opening the node blocks until the filesystem is open...
  InitNode();

  // Signal the "main" thread to tell it we're unblocked.
  pthread_mutex_lock(&mutex_);
  state_ = STATE_INIT_NODE_FINISHED;
  pthread_cond_signal(&cond_);
  pthread_mutex_unlock(&mutex_);
}

}  // namespace


TEST_F(MountHtml5FsTest, FilesystemType) {
  SetUpFilesystemExpectations(PP_FILESYSTEMTYPE_LOCALPERSISTENT, 100);

  StringMap_t map;
  map["type"] = "PERSISTENT";
  map["expected_size"] = "100";
  MountHtml5FsMock mnt(map, ppapi_);
}

TEST_F(MountHtml5FsTest, Mkdir) {
  const char path[] = "/foo";
  const PP_Resource fileref_resource = 235;

  // These are the default values.
  SetUpFilesystemExpectations(PP_FILESYSTEMTYPE_LOCALPERSISTENT, 0);

  FileRefInterfaceMock* fileref = ppapi_->GetFileRefInterface();

  EXPECT_CALL(*fileref, Create(filesystem_resource_, StrEq(&path[0])))
      .WillOnce(Return(fileref_resource));
  EXPECT_CALL(*fileref, MakeDirectory(fileref_resource, _, _))
      .WillOnce(Return(int32_t(PP_OK)));
  EXPECT_CALL(*ppapi_, ReleaseResource(fileref_resource));

  StringMap_t map;
  MountHtml5FsMock mnt(map, ppapi_);

  const int permissions = 0;  // unused.
  int32_t result = mnt.Mkdir(Path(path), permissions);
  ASSERT_EQ(0, result);
}

TEST_F(MountHtml5FsTest, Remove) {
  const char path[] = "/foo";
  const PP_Resource fileref_resource = 235;

  // These are the default values.
  SetUpFilesystemExpectations(PP_FILESYSTEMTYPE_LOCALPERSISTENT, 0);

  FileRefInterfaceMock* fileref = ppapi_->GetFileRefInterface();

  EXPECT_CALL(*fileref, Create(filesystem_resource_, StrEq(&path[0])))
      .WillOnce(Return(fileref_resource));
  EXPECT_CALL(*fileref, Delete(fileref_resource, _))
      .WillOnce(Return(int32_t(PP_OK)));
  EXPECT_CALL(*ppapi_, ReleaseResource(fileref_resource));

  StringMap_t map;
  MountHtml5FsMock mnt(map, ppapi_);

  int32_t result = mnt.Remove(Path(path));
  ASSERT_EQ(0, result);
}

TEST_F(MountHtml5FsNodeAsyncTest, AsyncFilesystemOpen) {
}

TEST_F(MountHtml5FsNodeSyncTest, OpenAndClose) {
}

TEST_F(MountHtml5FsNodeSyncTest, Write) {
  const int offset = 10;
  const int count = 20;
  const char buffer[30] = {0};

  EXPECT_CALL(*fileio_, Write(fileio_resource_, offset, &buffer[0], count, _))
      .WillOnce(Return(count));

  int result = node_->Write(offset, &buffer, count);
  EXPECT_EQ(count, result);
}

TEST_F(MountHtml5FsNodeSyncTest, Read) {
  const int offset = 10;
  const int count = 20;
  char buffer[30] = {0};

  EXPECT_CALL(*fileio_, Read(fileio_resource_, offset, &buffer[0], count, _))
      .WillOnce(Return(count));

  int result = node_->Read(offset, &buffer, count);
  EXPECT_EQ(count, result);
}

TEST_F(MountHtml5FsNodeSyncTest, GetStat) {
  const int size = 123;
  const int creation_time = 1000;
  const int access_time = 2000;
  const int modified_time = 3000;

  PP_FileInfo info;
  info.size = size;
  info.type = PP_FILETYPE_REGULAR;
  info.system_type = PP_FILESYSTEMTYPE_LOCALPERSISTENT;
  info.creation_time = creation_time;
  info.last_access_time = access_time;
  info.last_modified_time = modified_time;

  EXPECT_CALL(*fileio_, Query(fileio_resource_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(info),
                      Return(int32_t(PP_OK))));

  struct stat statbuf;
  int result = node_->GetStat(&statbuf);

  EXPECT_EQ(0, result);
  EXPECT_EQ(S_IFREG | S_IWRITE | S_IREAD, statbuf.st_mode);
  EXPECT_EQ(size, statbuf.st_size);
  EXPECT_EQ(access_time, statbuf.st_atime);
  EXPECT_EQ(modified_time, statbuf.st_mtime);
  EXPECT_EQ(creation_time, statbuf.st_ctime);
}

TEST_F(MountHtml5FsNodeSyncTest, Truncate) {
  const int size = 123;
  EXPECT_CALL(*fileio_, SetLength(fileio_resource_, size, _))
      .WillOnce(Return(int32_t(PP_OK)));

  int result = node_->Truncate(size);
  EXPECT_EQ(0, result);
}

TEST_F(MountHtml5FsNodeSyncTest, GetDents) {
  const int fileref_resource_1 = 238;
  const int fileref_resource_2 = 239;

  const int fileref_name_id_1 = 240;
  const char fileref_name_cstr_1[] = "bar";
  PP_Var fileref_name_1;
  fileref_name_1.type = PP_VARTYPE_STRING;
  fileref_name_1.value.as_id = fileref_name_id_1;

  const int fileref_name_id_2 = 241;
  const char fileref_name_cstr_2[] = "quux";
  PP_Var fileref_name_2;
  fileref_name_2.type = PP_VARTYPE_STRING;
  fileref_name_2.value.as_id = fileref_name_id_2;

  VarInterfaceMock* var = ppapi_->GetVarInterface();

  EXPECT_CALL(*fileref_, ReadDirectoryEntries(fileref_resource_, _, _))
      .WillOnce(DoAll(WithArgs<1>(Invoke(ReadDirectoryEntriesAction)),
                      Return(int32_t(PP_OK))));

  EXPECT_CALL(*fileref_, GetName(fileref_resource_1))
      .WillOnce(Return(fileref_name_1));
  EXPECT_CALL(*fileref_, GetName(fileref_resource_2))
      .WillOnce(Return(fileref_name_2));

  EXPECT_CALL(*var, VarToUtf8(IsEqualToVar(fileref_name_1), _))
      .WillOnce(Return(fileref_name_cstr_1));
  EXPECT_CALL(*var, VarToUtf8(IsEqualToVar(fileref_name_2), _))
      .WillOnce(Return(fileref_name_cstr_2));

  EXPECT_CALL(*ppapi_, ReleaseResource(fileref_resource_1));
  EXPECT_CALL(*ppapi_, ReleaseResource(fileref_resource_2));

  struct dirent dirents[2];
  memset(&dirents[0], 0, sizeof(dirents));
  int result = node_->GetDents(0, &dirents[0], sizeof(dirent) * 2);

  EXPECT_EQ(0, result);
  EXPECT_STREQ(&fileref_name_cstr_1[0], &dirents[0].d_name[0]);
  EXPECT_STREQ(&fileref_name_cstr_2[0], &dirents[1].d_name[0]);
}
