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

#include "mock_util.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/mount_http.h"
#include "nacl_io/mount_node_dir.h"
#include "nacl_io/osdirent.h"
#include "pepper_interface_mock.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;


class MountHttpMock : public MountHttp {
 public:
  MountHttpMock(StringMap_t map, PepperInterfaceMock* ppapi) {
    EXPECT_TRUE(Init(1, map, ppapi));
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

TEST_F(MountHttpTest, ParseManifest) {
  StringMap_t args;
  mnt_ = new MountHttpMock(args, &ppapi_);

  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  EXPECT_TRUE(mnt_->ParseManifest(manifest));

  MountNodeDir* root = mnt_->FindOrCreateDir(Path("/"));
  EXPECT_EQ(2, root->ChildCount());

  MountNodeDir* dir = mnt_->FindOrCreateDir(Path("/mydir"));
  EXPECT_EQ(1, dir->ChildCount());

  MountNode* node = mnt_->GetMap()["/mydir/foo"];
  EXPECT_TRUE(node);
  EXPECT_EQ(123, node->GetSize());

  // Since these files are cached thanks to the manifest, we can open them
  // without accessing the PPAPI URL API.
  MountNode* foo = mnt_->Open(Path("/mydir/foo"), O_RDONLY);
  MountNode* bar = mnt_->Open(Path("/thatdir/bar"), O_RDWR);

  struct stat sfoo;
  struct stat sbar;

  EXPECT_FALSE(foo->GetStat(&sfoo));
  EXPECT_FALSE(bar->GetStat(&sbar));

  EXPECT_EQ(123, sfoo.st_size);
  EXPECT_EQ(S_IFREG | S_IREAD, sfoo.st_mode);

  EXPECT_EQ(234, sbar.st_size);
  EXPECT_EQ(S_IFREG | S_IREAD | S_IWRITE, sbar.st_mode);
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
  void SetResponseBody(const char* body);
  void ResetMocks();

 protected:
  MountHttpMock* mnt_;
  MountNode* node_;

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
      .WillOnce(CallCallback<2>(int32_t(PP_OK)));
  EXPECT_CALL(*loader_, GetResponseInfo(loader_resource_))
      .WillOnce(Return(response_resource_));

  EXPECT_CALL(ppapi_, ReleaseResource(loader_resource_));
  EXPECT_CALL(ppapi_, ReleaseResource(request_resource_));
  EXPECT_CALL(ppapi_, ReleaseResource(response_resource_));
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
  node_ = mnt_->Open(Path(path_), O_RDONLY);
  ASSERT_NE((MountNode*)NULL, node_);
}

void MountHttpNodeTest::ResetMocks() {
  Mock::VerifyAndClearExpectations(&ppapi_);
  Mock::VerifyAndClearExpectations(loader_);
  Mock::VerifyAndClearExpectations(request_);
  Mock::VerifyAndClearExpectations(response_);
  Mock::VerifyAndClearExpectations(var_);
}

void MountHttpNodeTest::TearDown() {
  if (node_)
    mnt_->ReleaseNode(node_);
  delete mnt_;
}

TEST_F(MountHttpNodeTest, OpenAndClose) {
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "");
  OpenNode();
}

TEST_F(MountHttpNodeTest, ReadCached) {
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 42\n");
  OpenNode();
  ResetMocks();

  EXPECT_EQ(42, node_->GetSize());

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ExpectOpen("GET");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 42\n");
  SetResponseBody("Here is some response text. And some more.");
  node_->Read(0, buf, sizeof(buf) - 1);
  EXPECT_STREQ("Here is s", &buf[0]);
  ResetMocks();

  // Further reads should be cached.
  node_->Read(0, buf, sizeof(buf) - 1);
  EXPECT_STREQ("Here is s", &buf[0]);
  node_->Read(10, buf, sizeof(buf) - 1);
  EXPECT_STREQ("me respon", &buf[0]);

  EXPECT_EQ(42, node_->GetSize());
}

TEST_F(MountHttpNodeTest, ReadCachedNoContentLength) {
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "");
  OpenNode();
  ResetMocks();

  // Unknown size.
  EXPECT_EQ(0, node_->GetSize());

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ExpectOpen("GET");
  ExpectHeaders("");
  SetResponse(200, "");  // No Content-Length response here.
  SetResponseBody("Here is some response text. And some more.");
  node_->Read(0, buf, sizeof(buf) - 1);
  EXPECT_STREQ("Here is s", &buf[0]);
  ResetMocks();

  // Further reads should be cached.
  node_->Read(0, buf, sizeof(buf) - 1);
  EXPECT_STREQ("Here is s", &buf[0]);
  node_->Read(10, buf, sizeof(buf) - 1);
  EXPECT_STREQ("me respon", &buf[0]);

  EXPECT_EQ(42, node_->GetSize());
}

TEST_F(MountHttpNodeTest, ReadCachedUnderrun) {
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 100\n");
  OpenNode();
  ResetMocks();

  EXPECT_EQ(100, node_->GetSize());

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ExpectOpen("GET");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 100\n");
  SetResponseBody("abcdefghijklmnopqrstuvwxyz");
  node_->Read(0, buf, sizeof(buf) - 1);
  EXPECT_STREQ("abcdefghi", &buf[0]);
  ResetMocks();

  EXPECT_EQ(26, node_->GetSize());
}

TEST_F(MountHttpNodeTest, ReadCachedOverrun) {
  SetMountArgs(StringMap_t());
  ExpectOpen("HEAD");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 15\n");
  OpenNode();
  ResetMocks();

  EXPECT_EQ(15, node_->GetSize());

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ExpectOpen("GET");
  ExpectHeaders("");
  SetResponse(200, "Content-Length: 15\n");
  SetResponseBody("01234567890123456789");
  node_->Read(10, buf, sizeof(buf) - 1);
  EXPECT_STREQ("01234", &buf[0]);
  ResetMocks();

  EXPECT_EQ(15, node_->GetSize());
}

TEST_F(MountHttpNodeTest, ReadPartial) {
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
  node_->Read(0, buf, sizeof(buf) - 1);
  EXPECT_STREQ("012345678", &buf[0]);
  ResetMocks();

  // Another read is another request.
  ExpectOpen("GET");
  ExpectHeaders("Range: bytes=10-18\n");
  SetResponse(206, "Content-Length: 9\nContent-Range: bytes=10-18\n");
  SetResponseBody("abcdefghi");
  node_->Read(10, buf, sizeof(buf) - 1);
  EXPECT_STREQ("abcdefghi", &buf[0]);
}

TEST_F(MountHttpNodeTest, ReadPartialNoServerSupport) {
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
  node_->Read(10, buf, sizeof(buf) - 1);
  EXPECT_STREQ("abcdefghi", &buf[0]);
}
