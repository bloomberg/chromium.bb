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

#include "mock_util.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/mount_http.h"
#include "nacl_io/mount_node_dir.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/osunistd.h"
#include "pepper_interface_mock.h"

using namespace nacl_io;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

class MountHttpMock : public MountHttp {
 public:
  MountHttpMock(StringMap_t map, PepperInterfaceMock* ppapi) {
    EXPECT_EQ(0, Init(1, map, ppapi));
  }

  ~MountHttpMock() {
    Destroy();
  }

  NodeMap_t& GetMap() { return node_cache_; }

  using MountHttp::ParseManifest;
  using MountHttp::FindOrCreateDir;
};

class MountHttpTest : public ::testing::Test {
 public:
  MountHttpTest();
  ~MountHttpTest();

 protected:
  PepperInterfaceMock ppapi_;
  MountHttpMock* mnt_;

  static const PP_Instance instance_ = 123;
};

MountHttpTest::MountHttpTest()
    : ppapi_(instance_),
      mnt_(NULL) {
}

MountHttpTest::~MountHttpTest() {
  delete mnt_;
}


TEST_F(MountHttpTest, MountEmpty) {
  StringMap_t args;
  mnt_ = new MountHttpMock(args, &ppapi_);
}

TEST_F(MountHttpTest, Mkdir) {
  StringMap_t args;
  mnt_ = new MountHttpMock(args, &ppapi_);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  EXPECT_EQ(0, mnt_->ParseManifest(manifest));
  // mkdir of existing directories should give "File exists".
  EXPECT_EQ(EEXIST, mnt_->Mkdir(Path("/"), 0));
  EXPECT_EQ(EEXIST, mnt_->Mkdir(Path("/mydir"), 0));
  // mkdir of non-existent directories should give "Permission denied".
  EXPECT_EQ(EACCES, mnt_->Mkdir(Path("/non_existent"), 0));
}

TEST_F(MountHttpTest, Rmdir) {
  StringMap_t args;
  mnt_ = new MountHttpMock(args, &ppapi_);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  EXPECT_EQ(0, mnt_->ParseManifest(manifest));
  // Rmdir on existing dirs should give "Permission Denied"
  EXPECT_EQ(EACCES, mnt_->Rmdir(Path("/")));
  EXPECT_EQ(EACCES, mnt_->Rmdir(Path("/mydir")));
  // Rmdir on existing files should give "Not a direcotory"
  EXPECT_EQ(ENOTDIR, mnt_->Rmdir(Path("/mydir/foo")));
  // Rmdir on non-existent files should give "No such file or directory"
  EXPECT_EQ(ENOENT, mnt_->Rmdir(Path("/non_existent")));
}

TEST_F(MountHttpTest, Unlink) {
  StringMap_t args;
  mnt_ = new MountHttpMock(args, &ppapi_);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  EXPECT_EQ(0, mnt_->ParseManifest(manifest));
  // Unlink of existing files should give "Permission Denied"
  EXPECT_EQ(EACCES, mnt_->Unlink(Path("/mydir/foo")));
  // Unlink of existing directory should give "Is a directory"
  EXPECT_EQ(EISDIR, mnt_->Unlink(Path("/mydir")));
  // Unlink of non-existent files should give "No such file or directory"
  EXPECT_EQ(ENOENT, mnt_->Unlink(Path("/non_existent")));
}

TEST_F(MountHttpTest, Remove) {
  StringMap_t args;
  mnt_ = new MountHttpMock(args, &ppapi_);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  EXPECT_EQ(0, mnt_->ParseManifest(manifest));
  // Remove of existing files should give "Permission Denied"
  EXPECT_EQ(EACCES, mnt_->Remove(Path("/mydir/foo")));
  // Remove of existing directory should give "Permission Denied"
  EXPECT_EQ(EACCES, mnt_->Remove(Path("/mydir")));
  // Unlink of non-existent files should give "No such file or directory"
  EXPECT_EQ(ENOENT, mnt_->Remove(Path("/non_existent")));
}

TEST_F(MountHttpTest, ParseManifest) {
  StringMap_t args;
  size_t result_size = 0;

  mnt_ = new MountHttpMock(args, &ppapi_);

  // Multiple consecutive newlines or spaces should be ignored.
  char manifest[] = "-r-- 123 /mydir/foo\n\n-rw-   234  /thatdir/bar\n";
  EXPECT_EQ(0, mnt_->ParseManifest(manifest));

  ScopedMountNode root;
  EXPECT_EQ(0, mnt_->FindOrCreateDir(Path("/"), &root));
  ASSERT_NE((MountNode*)NULL, root.get());
  EXPECT_EQ(2, root->ChildCount());

  ScopedMountNode dir;
  EXPECT_EQ(0, mnt_->FindOrCreateDir(Path("/mydir"), &dir));
  ASSERT_NE((MountNode*)NULL, dir.get());
  EXPECT_EQ(1, dir->ChildCount());

  MountNode* node = mnt_->GetMap()["/mydir/foo"].get();
  EXPECT_NE((MountNode*)NULL, node);
  EXPECT_EQ(0, node->GetSize(&result_size));
  EXPECT_EQ(123, result_size);

  // Since these files are cached thanks to the manifest, we can open them
  // without accessing the PPAPI URL API.
  ScopedMountNode foo;
  EXPECT_EQ(0, mnt_->Open(Path("/mydir/foo"), O_RDONLY, &foo));

  ScopedMountNode bar;
  EXPECT_EQ(0, mnt_->Open(Path("/thatdir/bar"), O_RDWR, &bar));

  struct stat sfoo;
  struct stat sbar;

  EXPECT_FALSE(foo->GetStat(&sfoo));
  EXPECT_FALSE(bar->GetStat(&sbar));

  EXPECT_EQ(123, sfoo.st_size);
  EXPECT_EQ(S_IFREG | S_IRALL, sfoo.st_mode);

  EXPECT_EQ(234, sbar.st_size);
  EXPECT_EQ(S_IFREG | S_IRALL | S_IWALL, sbar.st_mode);
}


class MountHttpNodeTest : public MountHttpTest {
 public:
  MountHttpNodeTest();
  virtual void TearDown();

  void SetMountArgs(const StringMap_t& args);
  void ExpectOpen(const char* method);
  void ExpectHeaders(const char* headers);
  void OpenNode();
  void SetResponse(int status_code, const char* headers);
  // Set a response code, but expect the request to fail. Certain function calls
  // expected by SetResponse are not expected here.
  void SetResponseExpectFail(int status_code, const char* headers);
  void SetResponseBody(const char* body);
  void ResetMocks();

 protected:
  MountHttpMock* mnt_;
  ScopedMountNode node_;

  CoreInterfaceMock* core_;
  VarInterfaceMock* var_;
  URLLoaderInterfaceMock* loader_;
  URLRequestInfoInterfaceMock* request_;
  URLResponseInfoInterfaceMock* response_;
  size_t response_body_offset_;

  static const char path_[];
  static const char rel_path_[];
  static const PP_Resource loader_resource_ = 235;
  static const PP_Resource request_resource_ = 236;
  static const PP_Resource response_resource_ = 237;
};

// static
const char MountHttpNodeTest::path_[] = "/foo";
// static
const char MountHttpNodeTest::rel_path_[] = "foo";

MountHttpNodeTest::MountHttpNodeTest()
    : mnt_(NULL),
      node_(NULL) {
}

static PP_Var MakeString(PP_Resource resource) {
  PP_Var result = { PP_VARTYPE_STRING, 0, {PP_FALSE} };
  result.value.as_id = resource;
  return result;
}

void MountHttpNodeTest::SetMountArgs(const StringMap_t& args) {
  mnt_ = new MountHttpMock(args, &ppapi_);
}

void MountHttpNodeTest::ExpectOpen(const char* method) {
  core_ = ppapi_.GetCoreInterface();
  loader_ = ppapi_.GetURLLoaderInterface();
  request_ = ppapi_.GetURLRequestInfoInterface();
  response_ = ppapi_.GetURLResponseInfoInterface();
  var_ = ppapi_.GetVarInterface();

  ON_CALL(*request_, SetProperty(request_resource_, _, _))
      .WillByDefault(Return(PP_TRUE));
  ON_CALL(*var_, VarFromUtf8(_, _)).WillByDefault(Return(PP_MakeUndefined()));

  EXPECT_CALL(*loader_, Create(instance_)).WillOnce(Return(loader_resource_));
  EXPECT_CALL(*request_, Create(instance_)).WillOnce(Return(request_resource_));

  PP_Var var_head = MakeString(345);
  PP_Var var_url = MakeString(346);
  EXPECT_CALL(*var_, VarFromUtf8(StrEq(method), _)).WillOnce(Return(var_head));
  EXPECT_CALL(*var_, VarFromUtf8(StrEq(rel_path_), _))
      .WillOnce(Return(var_url));

#define EXPECT_SET_PROPERTY(NAME, VAR) \
  EXPECT_CALL(*request_, SetProperty(request_resource_, NAME, VAR))

  EXPECT_SET_PROPERTY(PP_URLREQUESTPROPERTY_URL, IsEqualToVar(var_url));
  EXPECT_SET_PROPERTY(PP_URLREQUESTPROPERTY_METHOD, IsEqualToVar(var_head));
  EXPECT_SET_PROPERTY(PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS, _);
  EXPECT_SET_PROPERTY(PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS, _);

#undef EXPECT_SET_PROPERTY

  EXPECT_CALL(*loader_, Open(loader_resource_, request_resource_, _))
      .WillOnce(DoAll(CallCallback<2>(int32_t(PP_OK)),
                      Return(int32_t(PP_OK_COMPLETIONPENDING))));
  EXPECT_CALL(*loader_, GetResponseInfo(loader_resource_))
      .WillOnce(Return(response_resource_));

  EXPECT_CALL(*core_, ReleaseResource(loader_resource_));
  EXPECT_CALL(*core_, ReleaseResource(request_resource_));
  EXPECT_CALL(*core_, ReleaseResource(response_resource_));
}

void MountHttpNodeTest::ExpectHeaders(const char* headers) {
  PP_Var var_headers = MakeString(347);
  var_ = ppapi_.GetVarInterface();
  EXPECT_CALL(*var_, VarFromUtf8(StrEq(headers), _))
      .WillOnce(Return(var_headers));

  EXPECT_CALL(*request_, SetProperty(request_resource_,
                                     PP_URLREQUESTPROPERTY_HEADERS,
                                     IsEqualToVar(var_headers))).Times(1);
}

void MountHttpNodeTest::SetResponse(int status_code, const char* headers) {
  ON_CALL(*response_, GetProperty(response_resource_, _))
      .WillByDefault(Return(PP_MakeUndefined()));

  PP_Var var_headers = MakeString(348);
  EXPECT_CALL(*response_,
              GetProperty(response_resource_,
                          PP_URLRESPONSEPROPERTY_STATUSCODE))
      .WillOnce(Return(PP_MakeInt32(status_code)));
  EXPECT_CALL(*response_,
              GetProperty(response_resource_, PP_URLRESPONSEPROPERTY_HEADERS))
      .WillOnce(Return(var_headers));
  EXPECT_CALL(*var_, VarToUtf8(IsEqualToVar(var_headers), _))
      .WillOnce(DoAll(SetArgPointee<1>(strlen(headers)),
                      Return(headers)));
}

void MountHttpNodeTest::SetResponseExpectFail(int status_code,
                                              const char* headers) {
  ON_CALL(*response_, GetProperty(response_resource_, _))
      .WillByDefault(Return(PP_MakeUndefined()));

  EXPECT_CALL(*response_,
              GetProperty(response_resource_,
                          PP_URLRESPONSEPROPERTY_STATUSCODE))
      .WillOnce(Return(PP_MakeInt32(status_code)));
}

ACTION_P3(ReadResponseBodyAction, offset, body, body_length) {
  char* buf = static_cast<char*>(arg1);
  size_t read_length = arg2;
  PP_CompletionCallback callback = arg3;
  if (*offset >= body_length)
    return 0;

  read_length = std::min(read_length, body_length - *offset);
  memcpy(buf, body + *offset, read_length);
  *offset += read_length;

  // Also call the callback.
  if (callback.func)
    (*callback.func)(callback.user_data, PP_OK);

  return read_length;
}

void MountHttpNodeTest::SetResponseBody(const char* body) {
  response_body_offset_ = 0;
  EXPECT_CALL(*loader_, ReadResponseBody(loader_resource_, _, _, _))
      .WillRepeatedly(ReadResponseBodyAction(
            &response_body_offset_, body, strlen(body)));
}

void MountHttpNodeTest::OpenNode() {
  ASSERT_EQ(0, mnt_->Open(Path(path_), O_RDONLY, &node_));
  ASSERT_NE((MountNode*)NULL, node_.get());
}

void MountHttpNodeTest::ResetMocks() {
  Mock::VerifyAndClearExpectations(&ppapi_);
  Mock::VerifyAndClearExpectations(loader_);
  Mock::VerifyAndClearExpectations(request_);
  Mock::VerifyAndClearExpectations(response_);
  Mock::VerifyAndClearExpectations(var_);
}

void MountHttpNodeTest::TearDown() {
  node_.reset();
  delete mnt_;
}

// TODO(binji): These tests are all broken now. In another CL, I'll reimplement
// these tests using an HTTP fake.
TEST_F(MountHttpNodeTest, DISABLED_OpenAndCloseNoCache) {
  StringMap_t smap;
  smap["cache_content"] = "false";
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "");
  OpenNode();
}

TEST_F(MountHttpNodeTest, DISABLED_OpenAndCloseNotFound) {
  StringMap_t smap;
  smap["cache_content"] = "false";
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponseExpectFail(404, "");
  ASSERT_EQ(ENOENT, mnt_->Open(Path(path_), O_RDONLY, &node_));
}

TEST_F(MountHttpNodeTest, DISABLED_OpenAndCloseServerError) {
  StringMap_t smap;
  smap["cache_content"] = "false";
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponseExpectFail(500, "");
  ASSERT_EQ(EIO, mnt_->Open(Path(path_), O_RDONLY, &node_));
}

TEST_F(MountHttpNodeTest, DISABLED_GetStat) {
  StringMap_t smap;
  smap["cache_content"] = "false";
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 42\n");
  OpenNode();

  struct stat stat;
  EXPECT_EQ(0, node_->GetStat(&stat));
  EXPECT_EQ(42, stat.st_size);
}

TEST_F(MountHttpNodeTest, DISABLED_Access) {
  StringMap_t smap;
  smap["cache_content"] = "false";
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "");
  ASSERT_EQ(0, mnt_->Access(Path(path_), R_OK));
}

TEST_F(MountHttpNodeTest, DISABLED_AccessWrite) {
  StringMap_t smap;
  smap["cache_content"] = "false";
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "");
  ASSERT_EQ(EACCES, mnt_->Access(Path(path_), W_OK));
}

TEST_F(MountHttpNodeTest, DISABLED_AccessNotFound) {
  StringMap_t smap;
  smap["cache_content"] = "false";
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponseExpectFail(404, "");
  ASSERT_EQ(ENOENT, mnt_->Access(Path(path_), R_OK));
}

TEST_F(MountHttpNodeTest, DISABLED_ReadCached) {
  size_t result_size = 0;
  int result_bytes = 0;

  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 42\n");
  OpenNode();
  ResetMocks();

  EXPECT_EQ(0, node_->GetSize(&result_size));
  EXPECT_EQ(42, result_size);

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ExpectOpen("GET");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 42\n");
  SetResponseBody("Here is some response text. And some more.");
  HandleAttr attr;
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_STREQ("Here is s", &buf[0]);
  ResetMocks();

  // Further reads should be cached.
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_STREQ("Here is s", &buf[0]);
  attr.offs = 10;
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_STREQ("me respon", &buf[0]);

  EXPECT_EQ(0, node_->GetSize(&result_size));
  EXPECT_EQ(42, result_size);
}

TEST_F(MountHttpNodeTest, DISABLED_ReadCachedNoContentLength) {
  size_t result_size = 0;
  int result_bytes = 0;

  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "");
  OpenNode();
  ResetMocks();

  ExpectOpen("GET");
  ExpectHeaders("");
  SetResponse(200, "");  // No Content-Length response here.
  SetResponseBody("Here is some response text. And some more.");

  // GetSize will Read() because it didn't get the content length from the HEAD
  // request.
  EXPECT_EQ(0, node_->GetSize(&result_size));
  EXPECT_EQ(42, result_size);

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  HandleAttr attr;
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_STREQ("Here is s", &buf[0]);
  ResetMocks();

  // Further reads should be cached.
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_STREQ("Here is s", &buf[0]);
  attr.offs = 10;
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_STREQ("me respon", &buf[0]);

  EXPECT_EQ(0, node_->GetSize(&result_size));
  EXPECT_EQ(42, result_size);
}

TEST_F(MountHttpNodeTest, DISABLED_ReadCachedUnderrun) {
  size_t result_size = 0;
  int result_bytes = 0;

  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 100\n");
  OpenNode();
  ResetMocks();

  EXPECT_EQ(0, node_->GetSize(&result_size));
  EXPECT_EQ(100, result_size);

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ExpectOpen("GET");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 100\n");
  SetResponseBody("abcdefghijklmnopqrstuvwxyz");
  HandleAttr attr;
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_EQ(sizeof(buf) - 1, result_bytes);
  EXPECT_STREQ("abcdefghi", &buf[0]);
  ResetMocks();

  EXPECT_EQ(0, node_->GetSize(&result_size));
  EXPECT_EQ(26, result_size);
}

TEST_F(MountHttpNodeTest, DISABLED_ReadCachedOverrun) {
  size_t result_size = 0;
  int result_bytes = 0;

  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 15\n");
  OpenNode();
  ResetMocks();

  EXPECT_EQ(0, node_->GetSize(&result_size));
  EXPECT_EQ(15, result_size);

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ExpectOpen("GET");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 15\n");
  SetResponseBody("01234567890123456789");
  HandleAttr attr;
  attr.offs = 10;
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_EQ(5, result_bytes);
  EXPECT_STREQ("01234", &buf[0]);
  ResetMocks();

  EXPECT_EQ(0, node_->GetSize(&result_size));
  EXPECT_EQ(15, result_size);
}

TEST_F(MountHttpNodeTest, DISABLED_ReadPartial) {
  int result_bytes = 0;

  StringMap_t args;
  args["cache_content"] = "false";
  SetMountArgs(args);
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "");
  OpenNode();
  ResetMocks();

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ExpectOpen("GET");
  ExpectHeaders("Range: bytes=0-8\n");
  SetResponse(206, "Content-Length: 9\nContent-Range: bytes=0-8\n");
  SetResponseBody("012345678");
  HandleAttr attr;
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_EQ(sizeof(buf) - 1, result_bytes);
  EXPECT_STREQ("012345678", &buf[0]);
  ResetMocks();

  // Another read is another request.
  ExpectOpen("GET");
  ExpectHeaders("Range: bytes=10-18\n");
  SetResponse(206, "Content-Length: 9\nContent-Range: bytes=10-18\n");
  SetResponseBody("abcdefghi");
  attr.offs = 10;
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_EQ(sizeof(buf) - 1, result_bytes);
  EXPECT_STREQ("abcdefghi", &buf[0]);
}

TEST_F(MountHttpNodeTest, DISABLED_ReadPartialNoServerSupport) {
  int result_bytes = 0;

  StringMap_t args;
  args["cache_content"] = "false";
  SetMountArgs(args);
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "");
  OpenNode();
  ResetMocks();

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ExpectOpen("GET");
  ExpectHeaders("Range: bytes=10-18\n");
  SetResponse(200, "Content-Length: 20\n");
  SetResponseBody("0123456789abcdefghij");
  HandleAttr attr;
  attr.offs = 10;
  EXPECT_EQ(0, node_->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_EQ(sizeof(buf) - 1, result_bytes);
  EXPECT_STREQ("abcdefghi", &buf[0]);
}

