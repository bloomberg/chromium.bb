// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#include "components/cronet/native/generated/cronet.idl_c.h"

#include "base/logging.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test of Cronet_Buffer interface.
class Cronet_BufferTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_BufferTest() {}
  ~Cronet_BufferTest() override {}

 public:
  bool InitWithDataAndCallback_called_ = false;
  bool InitWithAlloc_called_ = false;
  bool GetSize_called_ = false;
  bool GetData_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_BufferTest);
};

namespace {
// Implementation of Cronet_Buffer methods for testing.
void TestCronet_Buffer_InitWithDataAndCallback(
    Cronet_BufferPtr self,
    RawDataPtr data,
    uint64_t size,
    Cronet_BufferCallbackPtr callback) {
  CHECK(self);
  Cronet_BufferContext context = Cronet_Buffer_GetContext(self);
  Cronet_BufferTest* test = static_cast<Cronet_BufferTest*>(context);
  CHECK(test);
  test->InitWithDataAndCallback_called_ = true;
}
void TestCronet_Buffer_InitWithAlloc(Cronet_BufferPtr self, uint64_t size) {
  CHECK(self);
  Cronet_BufferContext context = Cronet_Buffer_GetContext(self);
  Cronet_BufferTest* test = static_cast<Cronet_BufferTest*>(context);
  CHECK(test);
  test->InitWithAlloc_called_ = true;
}
uint64_t TestCronet_Buffer_GetSize(Cronet_BufferPtr self) {
  CHECK(self);
  Cronet_BufferContext context = Cronet_Buffer_GetContext(self);
  Cronet_BufferTest* test = static_cast<Cronet_BufferTest*>(context);
  CHECK(test);
  test->GetSize_called_ = true;

  return static_cast<uint64_t>(0);
}
RawDataPtr TestCronet_Buffer_GetData(Cronet_BufferPtr self) {
  CHECK(self);
  Cronet_BufferContext context = Cronet_Buffer_GetContext(self);
  Cronet_BufferTest* test = static_cast<Cronet_BufferTest*>(context);
  CHECK(test);
  test->GetData_called_ = true;

  return static_cast<RawDataPtr>(0);
}
}  // namespace

// Test that Cronet_Buffer stub forwards function calls as expected.
TEST_F(Cronet_BufferTest, TestCreate) {
  Cronet_BufferPtr test = Cronet_Buffer_CreateStub(
      TestCronet_Buffer_InitWithDataAndCallback,
      TestCronet_Buffer_InitWithAlloc, TestCronet_Buffer_GetSize,
      TestCronet_Buffer_GetData);
  CHECK(test);
  Cronet_Buffer_SetContext(test, this);
  CHECK(!InitWithDataAndCallback_called_);
  CHECK(!InitWithAlloc_called_);
  Cronet_Buffer_GetSize(test);
  CHECK(GetSize_called_);
  Cronet_Buffer_GetData(test);
  CHECK(GetData_called_);

  Cronet_Buffer_Destroy(test);
}

// Test of Cronet_BufferCallback interface.
class Cronet_BufferCallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_BufferCallbackTest() {}
  ~Cronet_BufferCallbackTest() override {}

 public:
  bool OnDestroy_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_BufferCallbackTest);
};

namespace {
// Implementation of Cronet_BufferCallback methods for testing.
void TestCronet_BufferCallback_OnDestroy(Cronet_BufferCallbackPtr self,
                                         Cronet_BufferPtr buffer) {
  CHECK(self);
  Cronet_BufferCallbackContext context = Cronet_BufferCallback_GetContext(self);
  Cronet_BufferCallbackTest* test =
      static_cast<Cronet_BufferCallbackTest*>(context);
  CHECK(test);
  test->OnDestroy_called_ = true;
}
}  // namespace

// Test that Cronet_BufferCallback stub forwards function calls as expected.
TEST_F(Cronet_BufferCallbackTest, TestCreate) {
  Cronet_BufferCallbackPtr test =
      Cronet_BufferCallback_CreateStub(TestCronet_BufferCallback_OnDestroy);
  CHECK(test);
  Cronet_BufferCallback_SetContext(test, this);
  CHECK(!OnDestroy_called_);

  Cronet_BufferCallback_Destroy(test);
}

// Test of Cronet_Runnable interface.
class Cronet_RunnableTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_RunnableTest() {}
  ~Cronet_RunnableTest() override {}

 public:
  bool Run_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_RunnableTest);
};

namespace {
// Implementation of Cronet_Runnable methods for testing.
void TestCronet_Runnable_Run(Cronet_RunnablePtr self) {
  CHECK(self);
  Cronet_RunnableContext context = Cronet_Runnable_GetContext(self);
  Cronet_RunnableTest* test = static_cast<Cronet_RunnableTest*>(context);
  CHECK(test);
  test->Run_called_ = true;
}
}  // namespace

// Test that Cronet_Runnable stub forwards function calls as expected.
TEST_F(Cronet_RunnableTest, TestCreate) {
  Cronet_RunnablePtr test = Cronet_Runnable_CreateStub(TestCronet_Runnable_Run);
  CHECK(test);
  Cronet_Runnable_SetContext(test, this);
  Cronet_Runnable_Run(test);
  CHECK(Run_called_);

  Cronet_Runnable_Destroy(test);
}

// Test of Cronet_Executor interface.
class Cronet_ExecutorTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_ExecutorTest() {}
  ~Cronet_ExecutorTest() override {}

 public:
  bool Execute_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_ExecutorTest);
};

namespace {
// Implementation of Cronet_Executor methods for testing.
void TestCronet_Executor_Execute(Cronet_ExecutorPtr self,
                                 Cronet_RunnablePtr command) {
  CHECK(self);
  Cronet_ExecutorContext context = Cronet_Executor_GetContext(self);
  Cronet_ExecutorTest* test = static_cast<Cronet_ExecutorTest*>(context);
  CHECK(test);
  test->Execute_called_ = true;
}
}  // namespace

// Test that Cronet_Executor stub forwards function calls as expected.
TEST_F(Cronet_ExecutorTest, TestCreate) {
  Cronet_ExecutorPtr test =
      Cronet_Executor_CreateStub(TestCronet_Executor_Execute);
  CHECK(test);
  Cronet_Executor_SetContext(test, this);
  CHECK(!Execute_called_);

  Cronet_Executor_Destroy(test);
}

// Test of Cronet_Engine interface.
class Cronet_EngineTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_EngineTest() {}
  ~Cronet_EngineTest() override {}

 public:
  bool StartWithParams_called_ = false;
  bool StartNetLogToFile_called_ = false;
  bool StopNetLog_called_ = false;
  bool GetVersionString_called_ = false;
  bool GetDefaultUserAgent_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_EngineTest);
};

namespace {
// Implementation of Cronet_Engine methods for testing.
void TestCronet_Engine_StartWithParams(Cronet_EnginePtr self,
                                       Cronet_EngineParamsPtr params) {
  CHECK(self);
  Cronet_EngineContext context = Cronet_Engine_GetContext(self);
  Cronet_EngineTest* test = static_cast<Cronet_EngineTest*>(context);
  CHECK(test);
  test->StartWithParams_called_ = true;
}
void TestCronet_Engine_StartNetLogToFile(Cronet_EnginePtr self,
                                         CharString fileName,
                                         bool logAll) {
  CHECK(self);
  Cronet_EngineContext context = Cronet_Engine_GetContext(self);
  Cronet_EngineTest* test = static_cast<Cronet_EngineTest*>(context);
  CHECK(test);
  test->StartNetLogToFile_called_ = true;
}
void TestCronet_Engine_StopNetLog(Cronet_EnginePtr self) {
  CHECK(self);
  Cronet_EngineContext context = Cronet_Engine_GetContext(self);
  Cronet_EngineTest* test = static_cast<Cronet_EngineTest*>(context);
  CHECK(test);
  test->StopNetLog_called_ = true;
}
CharString TestCronet_Engine_GetVersionString(Cronet_EnginePtr self) {
  CHECK(self);
  Cronet_EngineContext context = Cronet_Engine_GetContext(self);
  Cronet_EngineTest* test = static_cast<Cronet_EngineTest*>(context);
  CHECK(test);
  test->GetVersionString_called_ = true;

  return static_cast<CharString>(0);
}
CharString TestCronet_Engine_GetDefaultUserAgent(Cronet_EnginePtr self) {
  CHECK(self);
  Cronet_EngineContext context = Cronet_Engine_GetContext(self);
  Cronet_EngineTest* test = static_cast<Cronet_EngineTest*>(context);
  CHECK(test);
  test->GetDefaultUserAgent_called_ = true;

  return static_cast<CharString>(0);
}
}  // namespace

// Test that Cronet_Engine stub forwards function calls as expected.
TEST_F(Cronet_EngineTest, TestCreate) {
  Cronet_EnginePtr test = Cronet_Engine_CreateStub(
      TestCronet_Engine_StartWithParams, TestCronet_Engine_StartNetLogToFile,
      TestCronet_Engine_StopNetLog, TestCronet_Engine_GetVersionString,
      TestCronet_Engine_GetDefaultUserAgent);
  CHECK(test);
  Cronet_Engine_SetContext(test, this);
  CHECK(!StartWithParams_called_);
  CHECK(!StartNetLogToFile_called_);
  Cronet_Engine_StopNetLog(test);
  CHECK(StopNetLog_called_);
  Cronet_Engine_GetVersionString(test);
  CHECK(GetVersionString_called_);
  Cronet_Engine_GetDefaultUserAgent(test);
  CHECK(GetDefaultUserAgent_called_);

  Cronet_Engine_Destroy(test);
}

// Test of Cronet_UrlRequestStatusListener interface.
class Cronet_UrlRequestStatusListenerTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_UrlRequestStatusListenerTest() {}
  ~Cronet_UrlRequestStatusListenerTest() override {}

 public:
  bool OnStatus_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestStatusListenerTest);
};

namespace {
// Implementation of Cronet_UrlRequestStatusListener methods for testing.
void TestCronet_UrlRequestStatusListener_OnStatus(
    Cronet_UrlRequestStatusListenerPtr self,
    Cronet_UrlRequestStatusListener_Status status) {
  CHECK(self);
  Cronet_UrlRequestStatusListenerContext context =
      Cronet_UrlRequestStatusListener_GetContext(self);
  Cronet_UrlRequestStatusListenerTest* test =
      static_cast<Cronet_UrlRequestStatusListenerTest*>(context);
  CHECK(test);
  test->OnStatus_called_ = true;
}
}  // namespace

// Test that Cronet_UrlRequestStatusListener stub forwards function calls as
// expected.
TEST_F(Cronet_UrlRequestStatusListenerTest, TestCreate) {
  Cronet_UrlRequestStatusListenerPtr test =
      Cronet_UrlRequestStatusListener_CreateStub(
          TestCronet_UrlRequestStatusListener_OnStatus);
  CHECK(test);
  Cronet_UrlRequestStatusListener_SetContext(test, this);
  CHECK(!OnStatus_called_);

  Cronet_UrlRequestStatusListener_Destroy(test);
}

// Test of Cronet_UrlRequestCallback interface.
class Cronet_UrlRequestCallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_UrlRequestCallbackTest() {}
  ~Cronet_UrlRequestCallbackTest() override {}

 public:
  bool OnRedirectReceived_called_ = false;
  bool OnResponseStarted_called_ = false;
  bool OnReadCompleted_called_ = false;
  bool OnSucceeded_called_ = false;
  bool OnFailed_called_ = false;
  bool OnCanceled_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestCallbackTest);
};

namespace {
// Implementation of Cronet_UrlRequestCallback methods for testing.
void TestCronet_UrlRequestCallback_OnRedirectReceived(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    CharString newLocationUrl) {
  CHECK(self);
  Cronet_UrlRequestCallbackContext context =
      Cronet_UrlRequestCallback_GetContext(self);
  Cronet_UrlRequestCallbackTest* test =
      static_cast<Cronet_UrlRequestCallbackTest*>(context);
  CHECK(test);
  test->OnRedirectReceived_called_ = true;
}
void TestCronet_UrlRequestCallback_OnResponseStarted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info) {
  CHECK(self);
  Cronet_UrlRequestCallbackContext context =
      Cronet_UrlRequestCallback_GetContext(self);
  Cronet_UrlRequestCallbackTest* test =
      static_cast<Cronet_UrlRequestCallbackTest*>(context);
  CHECK(test);
  test->OnResponseStarted_called_ = true;
}
void TestCronet_UrlRequestCallback_OnReadCompleted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_BufferPtr buffer) {
  CHECK(self);
  Cronet_UrlRequestCallbackContext context =
      Cronet_UrlRequestCallback_GetContext(self);
  Cronet_UrlRequestCallbackTest* test =
      static_cast<Cronet_UrlRequestCallbackTest*>(context);
  CHECK(test);
  test->OnReadCompleted_called_ = true;
}
void TestCronet_UrlRequestCallback_OnSucceeded(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info) {
  CHECK(self);
  Cronet_UrlRequestCallbackContext context =
      Cronet_UrlRequestCallback_GetContext(self);
  Cronet_UrlRequestCallbackTest* test =
      static_cast<Cronet_UrlRequestCallbackTest*>(context);
  CHECK(test);
  test->OnSucceeded_called_ = true;
}
void TestCronet_UrlRequestCallback_OnFailed(Cronet_UrlRequestCallbackPtr self,
                                            Cronet_UrlRequestPtr request,
                                            Cronet_UrlResponseInfoPtr info,
                                            Cronet_ExceptionPtr error) {
  CHECK(self);
  Cronet_UrlRequestCallbackContext context =
      Cronet_UrlRequestCallback_GetContext(self);
  Cronet_UrlRequestCallbackTest* test =
      static_cast<Cronet_UrlRequestCallbackTest*>(context);
  CHECK(test);
  test->OnFailed_called_ = true;
}
void TestCronet_UrlRequestCallback_OnCanceled(Cronet_UrlRequestCallbackPtr self,
                                              Cronet_UrlRequestPtr request,
                                              Cronet_UrlResponseInfoPtr info) {
  CHECK(self);
  Cronet_UrlRequestCallbackContext context =
      Cronet_UrlRequestCallback_GetContext(self);
  Cronet_UrlRequestCallbackTest* test =
      static_cast<Cronet_UrlRequestCallbackTest*>(context);
  CHECK(test);
  test->OnCanceled_called_ = true;
}
}  // namespace

// Test that Cronet_UrlRequestCallback stub forwards function calls as expected.
TEST_F(Cronet_UrlRequestCallbackTest, TestCreate) {
  Cronet_UrlRequestCallbackPtr test = Cronet_UrlRequestCallback_CreateStub(
      TestCronet_UrlRequestCallback_OnRedirectReceived,
      TestCronet_UrlRequestCallback_OnResponseStarted,
      TestCronet_UrlRequestCallback_OnReadCompleted,
      TestCronet_UrlRequestCallback_OnSucceeded,
      TestCronet_UrlRequestCallback_OnFailed,
      TestCronet_UrlRequestCallback_OnCanceled);
  CHECK(test);
  Cronet_UrlRequestCallback_SetContext(test, this);
  CHECK(!OnRedirectReceived_called_);
  CHECK(!OnResponseStarted_called_);
  CHECK(!OnReadCompleted_called_);
  CHECK(!OnSucceeded_called_);
  CHECK(!OnFailed_called_);
  CHECK(!OnCanceled_called_);

  Cronet_UrlRequestCallback_Destroy(test);
}

// Test of Cronet_UploadDataSink interface.
class Cronet_UploadDataSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_UploadDataSinkTest() {}
  ~Cronet_UploadDataSinkTest() override {}

 public:
  bool OnReadSucceeded_called_ = false;
  bool OnReadError_called_ = false;
  bool OnRewindSucceded_called_ = false;
  bool OnRewindError_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UploadDataSinkTest);
};

namespace {
// Implementation of Cronet_UploadDataSink methods for testing.
void TestCronet_UploadDataSink_OnReadSucceeded(Cronet_UploadDataSinkPtr self,
                                               bool finalChunk) {
  CHECK(self);
  Cronet_UploadDataSinkContext context = Cronet_UploadDataSink_GetContext(self);
  Cronet_UploadDataSinkTest* test =
      static_cast<Cronet_UploadDataSinkTest*>(context);
  CHECK(test);
  test->OnReadSucceeded_called_ = true;
}
void TestCronet_UploadDataSink_OnReadError(Cronet_UploadDataSinkPtr self,
                                           Cronet_ExceptionPtr error) {
  CHECK(self);
  Cronet_UploadDataSinkContext context = Cronet_UploadDataSink_GetContext(self);
  Cronet_UploadDataSinkTest* test =
      static_cast<Cronet_UploadDataSinkTest*>(context);
  CHECK(test);
  test->OnReadError_called_ = true;
}
void TestCronet_UploadDataSink_OnRewindSucceded(Cronet_UploadDataSinkPtr self) {
  CHECK(self);
  Cronet_UploadDataSinkContext context = Cronet_UploadDataSink_GetContext(self);
  Cronet_UploadDataSinkTest* test =
      static_cast<Cronet_UploadDataSinkTest*>(context);
  CHECK(test);
  test->OnRewindSucceded_called_ = true;
}
void TestCronet_UploadDataSink_OnRewindError(Cronet_UploadDataSinkPtr self,
                                             Cronet_ExceptionPtr error) {
  CHECK(self);
  Cronet_UploadDataSinkContext context = Cronet_UploadDataSink_GetContext(self);
  Cronet_UploadDataSinkTest* test =
      static_cast<Cronet_UploadDataSinkTest*>(context);
  CHECK(test);
  test->OnRewindError_called_ = true;
}
}  // namespace

// Test that Cronet_UploadDataSink stub forwards function calls as expected.
TEST_F(Cronet_UploadDataSinkTest, TestCreate) {
  Cronet_UploadDataSinkPtr test = Cronet_UploadDataSink_CreateStub(
      TestCronet_UploadDataSink_OnReadSucceeded,
      TestCronet_UploadDataSink_OnReadError,
      TestCronet_UploadDataSink_OnRewindSucceded,
      TestCronet_UploadDataSink_OnRewindError);
  CHECK(test);
  Cronet_UploadDataSink_SetContext(test, this);
  CHECK(!OnReadSucceeded_called_);
  CHECK(!OnReadError_called_);
  Cronet_UploadDataSink_OnRewindSucceded(test);
  CHECK(OnRewindSucceded_called_);
  CHECK(!OnRewindError_called_);

  Cronet_UploadDataSink_Destroy(test);
}

// Test of Cronet_UploadDataProvider interface.
class Cronet_UploadDataProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_UploadDataProviderTest() {}
  ~Cronet_UploadDataProviderTest() override {}

 public:
  bool GetLength_called_ = false;
  bool Read_called_ = false;
  bool Rewind_called_ = false;
  bool Close_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UploadDataProviderTest);
};

namespace {
// Implementation of Cronet_UploadDataProvider methods for testing.
int64_t TestCronet_UploadDataProvider_GetLength(
    Cronet_UploadDataProviderPtr self) {
  CHECK(self);
  Cronet_UploadDataProviderContext context =
      Cronet_UploadDataProvider_GetContext(self);
  Cronet_UploadDataProviderTest* test =
      static_cast<Cronet_UploadDataProviderTest*>(context);
  CHECK(test);
  test->GetLength_called_ = true;

  return static_cast<int64_t>(0);
}
void TestCronet_UploadDataProvider_Read(Cronet_UploadDataProviderPtr self,
                                        Cronet_UploadDataSinkPtr uploadDataSink,
                                        Cronet_BufferPtr buffer) {
  CHECK(self);
  Cronet_UploadDataProviderContext context =
      Cronet_UploadDataProvider_GetContext(self);
  Cronet_UploadDataProviderTest* test =
      static_cast<Cronet_UploadDataProviderTest*>(context);
  CHECK(test);
  test->Read_called_ = true;
}
void TestCronet_UploadDataProvider_Rewind(
    Cronet_UploadDataProviderPtr self,
    Cronet_UploadDataSinkPtr uploadDataSink) {
  CHECK(self);
  Cronet_UploadDataProviderContext context =
      Cronet_UploadDataProvider_GetContext(self);
  Cronet_UploadDataProviderTest* test =
      static_cast<Cronet_UploadDataProviderTest*>(context);
  CHECK(test);
  test->Rewind_called_ = true;
}
void TestCronet_UploadDataProvider_Close(Cronet_UploadDataProviderPtr self) {
  CHECK(self);
  Cronet_UploadDataProviderContext context =
      Cronet_UploadDataProvider_GetContext(self);
  Cronet_UploadDataProviderTest* test =
      static_cast<Cronet_UploadDataProviderTest*>(context);
  CHECK(test);
  test->Close_called_ = true;
}
}  // namespace

// Test that Cronet_UploadDataProvider stub forwards function calls as expected.
TEST_F(Cronet_UploadDataProviderTest, TestCreate) {
  Cronet_UploadDataProviderPtr test = Cronet_UploadDataProvider_CreateStub(
      TestCronet_UploadDataProvider_GetLength,
      TestCronet_UploadDataProvider_Read, TestCronet_UploadDataProvider_Rewind,
      TestCronet_UploadDataProvider_Close);
  CHECK(test);
  Cronet_UploadDataProvider_SetContext(test, this);
  Cronet_UploadDataProvider_GetLength(test);
  CHECK(GetLength_called_);
  CHECK(!Read_called_);
  CHECK(!Rewind_called_);
  Cronet_UploadDataProvider_Close(test);
  CHECK(Close_called_);

  Cronet_UploadDataProvider_Destroy(test);
}

// Test of Cronet_UrlRequest interface.
class Cronet_UrlRequestTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  Cronet_UrlRequestTest() {}
  ~Cronet_UrlRequestTest() override {}

 public:
  bool InitWithParams_called_ = false;
  bool Start_called_ = false;
  bool FollowRedirect_called_ = false;
  bool Read_called_ = false;
  bool Cancel_called_ = false;
  bool IsDone_called_ = false;
  bool GetStatus_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestTest);
};

namespace {
// Implementation of Cronet_UrlRequest methods for testing.
void TestCronet_UrlRequest_InitWithParams(Cronet_UrlRequestPtr self,
                                          Cronet_EnginePtr engine,
                                          CharString url,
                                          Cronet_UrlRequestParamsPtr params,
                                          Cronet_UrlRequestCallbackPtr callback,
                                          Cronet_ExecutorPtr executor) {
  CHECK(self);
  Cronet_UrlRequestContext context = Cronet_UrlRequest_GetContext(self);
  Cronet_UrlRequestTest* test = static_cast<Cronet_UrlRequestTest*>(context);
  CHECK(test);
  test->InitWithParams_called_ = true;
}
void TestCronet_UrlRequest_Start(Cronet_UrlRequestPtr self) {
  CHECK(self);
  Cronet_UrlRequestContext context = Cronet_UrlRequest_GetContext(self);
  Cronet_UrlRequestTest* test = static_cast<Cronet_UrlRequestTest*>(context);
  CHECK(test);
  test->Start_called_ = true;
}
void TestCronet_UrlRequest_FollowRedirect(Cronet_UrlRequestPtr self) {
  CHECK(self);
  Cronet_UrlRequestContext context = Cronet_UrlRequest_GetContext(self);
  Cronet_UrlRequestTest* test = static_cast<Cronet_UrlRequestTest*>(context);
  CHECK(test);
  test->FollowRedirect_called_ = true;
}
void TestCronet_UrlRequest_Read(Cronet_UrlRequestPtr self,
                                Cronet_BufferPtr buffer) {
  CHECK(self);
  Cronet_UrlRequestContext context = Cronet_UrlRequest_GetContext(self);
  Cronet_UrlRequestTest* test = static_cast<Cronet_UrlRequestTest*>(context);
  CHECK(test);
  test->Read_called_ = true;
}
void TestCronet_UrlRequest_Cancel(Cronet_UrlRequestPtr self) {
  CHECK(self);
  Cronet_UrlRequestContext context = Cronet_UrlRequest_GetContext(self);
  Cronet_UrlRequestTest* test = static_cast<Cronet_UrlRequestTest*>(context);
  CHECK(test);
  test->Cancel_called_ = true;
}
bool TestCronet_UrlRequest_IsDone(Cronet_UrlRequestPtr self) {
  CHECK(self);
  Cronet_UrlRequestContext context = Cronet_UrlRequest_GetContext(self);
  Cronet_UrlRequestTest* test = static_cast<Cronet_UrlRequestTest*>(context);
  CHECK(test);
  test->IsDone_called_ = true;

  return static_cast<bool>(0);
}
void TestCronet_UrlRequest_GetStatus(
    Cronet_UrlRequestPtr self,
    Cronet_UrlRequestStatusListenerPtr listener) {
  CHECK(self);
  Cronet_UrlRequestContext context = Cronet_UrlRequest_GetContext(self);
  Cronet_UrlRequestTest* test = static_cast<Cronet_UrlRequestTest*>(context);
  CHECK(test);
  test->GetStatus_called_ = true;
}
}  // namespace

// Test that Cronet_UrlRequest stub forwards function calls as expected.
TEST_F(Cronet_UrlRequestTest, TestCreate) {
  Cronet_UrlRequestPtr test = Cronet_UrlRequest_CreateStub(
      TestCronet_UrlRequest_InitWithParams, TestCronet_UrlRequest_Start,
      TestCronet_UrlRequest_FollowRedirect, TestCronet_UrlRequest_Read,
      TestCronet_UrlRequest_Cancel, TestCronet_UrlRequest_IsDone,
      TestCronet_UrlRequest_GetStatus);
  CHECK(test);
  Cronet_UrlRequest_SetContext(test, this);
  CHECK(!InitWithParams_called_);
  Cronet_UrlRequest_Start(test);
  CHECK(Start_called_);
  Cronet_UrlRequest_FollowRedirect(test);
  CHECK(FollowRedirect_called_);
  CHECK(!Read_called_);
  Cronet_UrlRequest_Cancel(test);
  CHECK(Cancel_called_);
  Cronet_UrlRequest_IsDone(test);
  CHECK(IsDone_called_);
  CHECK(!GetStatus_called_);

  Cronet_UrlRequest_Destroy(test);
}
