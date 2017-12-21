// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#include "components/cronet/native/generated/cronet.idl_impl_interface.h"

#include "base/logging.h"

// C functions of Cronet_Buffer that forward calls to C++ implementation.
void Cronet_Buffer_Destroy(Cronet_BufferPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_Buffer_SetContext(Cronet_BufferPtr self,
                              Cronet_BufferContext context) {
  DCHECK(self);
  return self->SetContext(context);
}

Cronet_BufferContext Cronet_Buffer_GetContext(Cronet_BufferPtr self) {
  DCHECK(self);
  return self->GetContext();
}

void Cronet_Buffer_InitWithDataAndCallback(Cronet_BufferPtr self,
                                           RawDataPtr data,
                                           uint64_t size,
                                           Cronet_BufferCallbackPtr callback) {
  DCHECK(self);
  self->InitWithDataAndCallback(data, size, callback);
}

void Cronet_Buffer_InitWithAlloc(Cronet_BufferPtr self, uint64_t size) {
  DCHECK(self);
  self->InitWithAlloc(size);
}

uint64_t Cronet_Buffer_GetSize(Cronet_BufferPtr self) {
  DCHECK(self);
  return self->GetSize();
}

RawDataPtr Cronet_Buffer_GetData(Cronet_BufferPtr self) {
  DCHECK(self);
  return self->GetData();
}

// Implementation of Cronet_Buffer that forwards calls to C functions
// implemented by the app.
class Cronet_BufferStub : public Cronet_Buffer {
 public:
  Cronet_BufferStub(
      Cronet_Buffer_InitWithDataAndCallbackFunc InitWithDataAndCallbackFunc,
      Cronet_Buffer_InitWithAllocFunc InitWithAllocFunc,
      Cronet_Buffer_GetSizeFunc GetSizeFunc,
      Cronet_Buffer_GetDataFunc GetDataFunc)
      : InitWithDataAndCallbackFunc_(InitWithDataAndCallbackFunc),
        InitWithAllocFunc_(InitWithAllocFunc),
        GetSizeFunc_(GetSizeFunc),
        GetDataFunc_(GetDataFunc) {}

  ~Cronet_BufferStub() override {}

  void SetContext(Cronet_BufferContext context) override { context_ = context; }

  Cronet_BufferContext GetContext() override { return context_; }

 protected:
  void InitWithDataAndCallback(RawDataPtr data,
                               uint64_t size,
                               Cronet_BufferCallbackPtr callback) override {
    InitWithDataAndCallbackFunc_(this, data, size, callback);
  }

  void InitWithAlloc(uint64_t size) override { InitWithAllocFunc_(this, size); }

  uint64_t GetSize() override { return GetSizeFunc_(this); }

  RawDataPtr GetData() override { return GetDataFunc_(this); }

 private:
  Cronet_BufferContext context_ = nullptr;
  const Cronet_Buffer_InitWithDataAndCallbackFunc InitWithDataAndCallbackFunc_;
  const Cronet_Buffer_InitWithAllocFunc InitWithAllocFunc_;
  const Cronet_Buffer_GetSizeFunc GetSizeFunc_;
  const Cronet_Buffer_GetDataFunc GetDataFunc_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_BufferStub);
};

Cronet_BufferPtr Cronet_Buffer_CreateStub(
    Cronet_Buffer_InitWithDataAndCallbackFunc InitWithDataAndCallbackFunc,
    Cronet_Buffer_InitWithAllocFunc InitWithAllocFunc,
    Cronet_Buffer_GetSizeFunc GetSizeFunc,
    Cronet_Buffer_GetDataFunc GetDataFunc) {
  return new Cronet_BufferStub(InitWithDataAndCallbackFunc, InitWithAllocFunc,
                               GetSizeFunc, GetDataFunc);
}

// C functions of Cronet_BufferCallback that forward calls to C++
// implementation.
void Cronet_BufferCallback_Destroy(Cronet_BufferCallbackPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_BufferCallback_SetContext(Cronet_BufferCallbackPtr self,
                                      Cronet_BufferCallbackContext context) {
  DCHECK(self);
  return self->SetContext(context);
}

Cronet_BufferCallbackContext Cronet_BufferCallback_GetContext(
    Cronet_BufferCallbackPtr self) {
  DCHECK(self);
  return self->GetContext();
}

void Cronet_BufferCallback_OnDestroy(Cronet_BufferCallbackPtr self,
                                     Cronet_BufferPtr buffer) {
  DCHECK(self);
  self->OnDestroy(buffer);
}

// Implementation of Cronet_BufferCallback that forwards calls to C functions
// implemented by the app.
class Cronet_BufferCallbackStub : public Cronet_BufferCallback {
 public:
  explicit Cronet_BufferCallbackStub(
      Cronet_BufferCallback_OnDestroyFunc OnDestroyFunc)
      : OnDestroyFunc_(OnDestroyFunc) {}

  ~Cronet_BufferCallbackStub() override {}

  void SetContext(Cronet_BufferCallbackContext context) override {
    context_ = context;
  }

  Cronet_BufferCallbackContext GetContext() override { return context_; }

 protected:
  void OnDestroy(Cronet_BufferPtr buffer) override {
    OnDestroyFunc_(this, buffer);
  }

 private:
  Cronet_BufferCallbackContext context_ = nullptr;
  const Cronet_BufferCallback_OnDestroyFunc OnDestroyFunc_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_BufferCallbackStub);
};

Cronet_BufferCallbackPtr Cronet_BufferCallback_CreateStub(
    Cronet_BufferCallback_OnDestroyFunc OnDestroyFunc) {
  return new Cronet_BufferCallbackStub(OnDestroyFunc);
}

// C functions of Cronet_Runnable that forward calls to C++ implementation.
void Cronet_Runnable_Destroy(Cronet_RunnablePtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_Runnable_SetContext(Cronet_RunnablePtr self,
                                Cronet_RunnableContext context) {
  DCHECK(self);
  return self->SetContext(context);
}

Cronet_RunnableContext Cronet_Runnable_GetContext(Cronet_RunnablePtr self) {
  DCHECK(self);
  return self->GetContext();
}

void Cronet_Runnable_Run(Cronet_RunnablePtr self) {
  DCHECK(self);
  self->Run();
}

// Implementation of Cronet_Runnable that forwards calls to C functions
// implemented by the app.
class Cronet_RunnableStub : public Cronet_Runnable {
 public:
  explicit Cronet_RunnableStub(Cronet_Runnable_RunFunc RunFunc)
      : RunFunc_(RunFunc) {}

  ~Cronet_RunnableStub() override {}

  void SetContext(Cronet_RunnableContext context) override {
    context_ = context;
  }

  Cronet_RunnableContext GetContext() override { return context_; }

 protected:
  void Run() override { RunFunc_(this); }

 private:
  Cronet_RunnableContext context_ = nullptr;
  const Cronet_Runnable_RunFunc RunFunc_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_RunnableStub);
};

Cronet_RunnablePtr Cronet_Runnable_CreateStub(Cronet_Runnable_RunFunc RunFunc) {
  return new Cronet_RunnableStub(RunFunc);
}

// C functions of Cronet_Executor that forward calls to C++ implementation.
void Cronet_Executor_Destroy(Cronet_ExecutorPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_Executor_SetContext(Cronet_ExecutorPtr self,
                                Cronet_ExecutorContext context) {
  DCHECK(self);
  return self->SetContext(context);
}

Cronet_ExecutorContext Cronet_Executor_GetContext(Cronet_ExecutorPtr self) {
  DCHECK(self);
  return self->GetContext();
}

void Cronet_Executor_Execute(Cronet_ExecutorPtr self,
                             Cronet_RunnablePtr command) {
  DCHECK(self);
  self->Execute(command);
}

// Implementation of Cronet_Executor that forwards calls to C functions
// implemented by the app.
class Cronet_ExecutorStub : public Cronet_Executor {
 public:
  explicit Cronet_ExecutorStub(Cronet_Executor_ExecuteFunc ExecuteFunc)
      : ExecuteFunc_(ExecuteFunc) {}

  ~Cronet_ExecutorStub() override {}

  void SetContext(Cronet_ExecutorContext context) override {
    context_ = context;
  }

  Cronet_ExecutorContext GetContext() override { return context_; }

 protected:
  void Execute(Cronet_RunnablePtr command) override {
    ExecuteFunc_(this, command);
  }

 private:
  Cronet_ExecutorContext context_ = nullptr;
  const Cronet_Executor_ExecuteFunc ExecuteFunc_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_ExecutorStub);
};

Cronet_ExecutorPtr Cronet_Executor_CreateStub(
    Cronet_Executor_ExecuteFunc ExecuteFunc) {
  return new Cronet_ExecutorStub(ExecuteFunc);
}

// C functions of Cronet_Engine that forward calls to C++ implementation.
void Cronet_Engine_Destroy(Cronet_EnginePtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_Engine_SetContext(Cronet_EnginePtr self,
                              Cronet_EngineContext context) {
  DCHECK(self);
  return self->SetContext(context);
}

Cronet_EngineContext Cronet_Engine_GetContext(Cronet_EnginePtr self) {
  DCHECK(self);
  return self->GetContext();
}

void Cronet_Engine_StartWithParams(Cronet_EnginePtr self,
                                   Cronet_EngineParamsPtr params) {
  DCHECK(self);
  self->StartWithParams(params);
}

void Cronet_Engine_StartNetLogToFile(Cronet_EnginePtr self,
                                     CharString fileName,
                                     bool logAll) {
  DCHECK(self);
  self->StartNetLogToFile(fileName, logAll);
}

void Cronet_Engine_StopNetLog(Cronet_EnginePtr self) {
  DCHECK(self);
  self->StopNetLog();
}

CharString Cronet_Engine_GetVersionString(Cronet_EnginePtr self) {
  DCHECK(self);
  return self->GetVersionString();
}

CharString Cronet_Engine_GetDefaultUserAgent(Cronet_EnginePtr self) {
  DCHECK(self);
  return self->GetDefaultUserAgent();
}

// Implementation of Cronet_Engine that forwards calls to C functions
// implemented by the app.
class Cronet_EngineStub : public Cronet_Engine {
 public:
  Cronet_EngineStub(
      Cronet_Engine_StartWithParamsFunc StartWithParamsFunc,
      Cronet_Engine_StartNetLogToFileFunc StartNetLogToFileFunc,
      Cronet_Engine_StopNetLogFunc StopNetLogFunc,
      Cronet_Engine_GetVersionStringFunc GetVersionStringFunc,
      Cronet_Engine_GetDefaultUserAgentFunc GetDefaultUserAgentFunc)
      : StartWithParamsFunc_(StartWithParamsFunc),
        StartNetLogToFileFunc_(StartNetLogToFileFunc),
        StopNetLogFunc_(StopNetLogFunc),
        GetVersionStringFunc_(GetVersionStringFunc),
        GetDefaultUserAgentFunc_(GetDefaultUserAgentFunc) {}

  ~Cronet_EngineStub() override {}

  void SetContext(Cronet_EngineContext context) override { context_ = context; }

  Cronet_EngineContext GetContext() override { return context_; }

 protected:
  void StartWithParams(Cronet_EngineParamsPtr params) override {
    StartWithParamsFunc_(this, params);
  }

  void StartNetLogToFile(CharString fileName, bool logAll) override {
    StartNetLogToFileFunc_(this, fileName, logAll);
  }

  void StopNetLog() override { StopNetLogFunc_(this); }

  CharString GetVersionString() override { return GetVersionStringFunc_(this); }

  CharString GetDefaultUserAgent() override {
    return GetDefaultUserAgentFunc_(this);
  }

 private:
  Cronet_EngineContext context_ = nullptr;
  const Cronet_Engine_StartWithParamsFunc StartWithParamsFunc_;
  const Cronet_Engine_StartNetLogToFileFunc StartNetLogToFileFunc_;
  const Cronet_Engine_StopNetLogFunc StopNetLogFunc_;
  const Cronet_Engine_GetVersionStringFunc GetVersionStringFunc_;
  const Cronet_Engine_GetDefaultUserAgentFunc GetDefaultUserAgentFunc_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_EngineStub);
};

Cronet_EnginePtr Cronet_Engine_CreateStub(
    Cronet_Engine_StartWithParamsFunc StartWithParamsFunc,
    Cronet_Engine_StartNetLogToFileFunc StartNetLogToFileFunc,
    Cronet_Engine_StopNetLogFunc StopNetLogFunc,
    Cronet_Engine_GetVersionStringFunc GetVersionStringFunc,
    Cronet_Engine_GetDefaultUserAgentFunc GetDefaultUserAgentFunc) {
  return new Cronet_EngineStub(StartWithParamsFunc, StartNetLogToFileFunc,
                               StopNetLogFunc, GetVersionStringFunc,
                               GetDefaultUserAgentFunc);
}

// C functions of Cronet_UrlRequestStatusListener that forward calls to C++
// implementation.
void Cronet_UrlRequestStatusListener_Destroy(
    Cronet_UrlRequestStatusListenerPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_UrlRequestStatusListener_SetContext(
    Cronet_UrlRequestStatusListenerPtr self,
    Cronet_UrlRequestStatusListenerContext context) {
  DCHECK(self);
  return self->SetContext(context);
}

Cronet_UrlRequestStatusListenerContext
Cronet_UrlRequestStatusListener_GetContext(
    Cronet_UrlRequestStatusListenerPtr self) {
  DCHECK(self);
  return self->GetContext();
}

void Cronet_UrlRequestStatusListener_OnStatus(
    Cronet_UrlRequestStatusListenerPtr self,
    Cronet_UrlRequestStatusListener_Status status) {
  DCHECK(self);
  self->OnStatus(status);
}

// Implementation of Cronet_UrlRequestStatusListener that forwards calls to C
// functions implemented by the app.
class Cronet_UrlRequestStatusListenerStub
    : public Cronet_UrlRequestStatusListener {
 public:
  explicit Cronet_UrlRequestStatusListenerStub(
      Cronet_UrlRequestStatusListener_OnStatusFunc OnStatusFunc)
      : OnStatusFunc_(OnStatusFunc) {}

  ~Cronet_UrlRequestStatusListenerStub() override {}

  void SetContext(Cronet_UrlRequestStatusListenerContext context) override {
    context_ = context;
  }

  Cronet_UrlRequestStatusListenerContext GetContext() override {
    return context_;
  }

 protected:
  void OnStatus(Cronet_UrlRequestStatusListener_Status status) override {
    OnStatusFunc_(this, status);
  }

 private:
  Cronet_UrlRequestStatusListenerContext context_ = nullptr;
  const Cronet_UrlRequestStatusListener_OnStatusFunc OnStatusFunc_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestStatusListenerStub);
};

Cronet_UrlRequestStatusListenerPtr Cronet_UrlRequestStatusListener_CreateStub(
    Cronet_UrlRequestStatusListener_OnStatusFunc OnStatusFunc) {
  return new Cronet_UrlRequestStatusListenerStub(OnStatusFunc);
}

// C functions of Cronet_UrlRequestCallback that forward calls to C++
// implementation.
void Cronet_UrlRequestCallback_Destroy(Cronet_UrlRequestCallbackPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_UrlRequestCallback_SetContext(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestCallbackContext context) {
  DCHECK(self);
  return self->SetContext(context);
}

Cronet_UrlRequestCallbackContext Cronet_UrlRequestCallback_GetContext(
    Cronet_UrlRequestCallbackPtr self) {
  DCHECK(self);
  return self->GetContext();
}

void Cronet_UrlRequestCallback_OnRedirectReceived(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    CharString newLocationUrl) {
  DCHECK(self);
  self->OnRedirectReceived(request, info, newLocationUrl);
}

void Cronet_UrlRequestCallback_OnResponseStarted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info) {
  DCHECK(self);
  self->OnResponseStarted(request, info);
}

void Cronet_UrlRequestCallback_OnReadCompleted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_BufferPtr buffer) {
  DCHECK(self);
  self->OnReadCompleted(request, info, buffer);
}

void Cronet_UrlRequestCallback_OnSucceeded(Cronet_UrlRequestCallbackPtr self,
                                           Cronet_UrlRequestPtr request,
                                           Cronet_UrlResponseInfoPtr info) {
  DCHECK(self);
  self->OnSucceeded(request, info);
}

void Cronet_UrlRequestCallback_OnFailed(Cronet_UrlRequestCallbackPtr self,
                                        Cronet_UrlRequestPtr request,
                                        Cronet_UrlResponseInfoPtr info,
                                        Cronet_ExceptionPtr error) {
  DCHECK(self);
  self->OnFailed(request, info, error);
}

void Cronet_UrlRequestCallback_OnCanceled(Cronet_UrlRequestCallbackPtr self,
                                          Cronet_UrlRequestPtr request,
                                          Cronet_UrlResponseInfoPtr info) {
  DCHECK(self);
  self->OnCanceled(request, info);
}

// Implementation of Cronet_UrlRequestCallback that forwards calls to C
// functions implemented by the app.
class Cronet_UrlRequestCallbackStub : public Cronet_UrlRequestCallback {
 public:
  Cronet_UrlRequestCallbackStub(
      Cronet_UrlRequestCallback_OnRedirectReceivedFunc OnRedirectReceivedFunc,
      Cronet_UrlRequestCallback_OnResponseStartedFunc OnResponseStartedFunc,
      Cronet_UrlRequestCallback_OnReadCompletedFunc OnReadCompletedFunc,
      Cronet_UrlRequestCallback_OnSucceededFunc OnSucceededFunc,
      Cronet_UrlRequestCallback_OnFailedFunc OnFailedFunc,
      Cronet_UrlRequestCallback_OnCanceledFunc OnCanceledFunc)
      : OnRedirectReceivedFunc_(OnRedirectReceivedFunc),
        OnResponseStartedFunc_(OnResponseStartedFunc),
        OnReadCompletedFunc_(OnReadCompletedFunc),
        OnSucceededFunc_(OnSucceededFunc),
        OnFailedFunc_(OnFailedFunc),
        OnCanceledFunc_(OnCanceledFunc) {}

  ~Cronet_UrlRequestCallbackStub() override {}

  void SetContext(Cronet_UrlRequestCallbackContext context) override {
    context_ = context;
  }

  Cronet_UrlRequestCallbackContext GetContext() override { return context_; }

 protected:
  void OnRedirectReceived(Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info,
                          CharString newLocationUrl) override {
    OnRedirectReceivedFunc_(this, request, info, newLocationUrl);
  }

  void OnResponseStarted(Cronet_UrlRequestPtr request,
                         Cronet_UrlResponseInfoPtr info) override {
    OnResponseStartedFunc_(this, request, info);
  }

  void OnReadCompleted(Cronet_UrlRequestPtr request,
                       Cronet_UrlResponseInfoPtr info,
                       Cronet_BufferPtr buffer) override {
    OnReadCompletedFunc_(this, request, info, buffer);
  }

  void OnSucceeded(Cronet_UrlRequestPtr request,
                   Cronet_UrlResponseInfoPtr info) override {
    OnSucceededFunc_(this, request, info);
  }

  void OnFailed(Cronet_UrlRequestPtr request,
                Cronet_UrlResponseInfoPtr info,
                Cronet_ExceptionPtr error) override {
    OnFailedFunc_(this, request, info, error);
  }

  void OnCanceled(Cronet_UrlRequestPtr request,
                  Cronet_UrlResponseInfoPtr info) override {
    OnCanceledFunc_(this, request, info);
  }

 private:
  Cronet_UrlRequestCallbackContext context_ = nullptr;
  const Cronet_UrlRequestCallback_OnRedirectReceivedFunc
      OnRedirectReceivedFunc_;
  const Cronet_UrlRequestCallback_OnResponseStartedFunc OnResponseStartedFunc_;
  const Cronet_UrlRequestCallback_OnReadCompletedFunc OnReadCompletedFunc_;
  const Cronet_UrlRequestCallback_OnSucceededFunc OnSucceededFunc_;
  const Cronet_UrlRequestCallback_OnFailedFunc OnFailedFunc_;
  const Cronet_UrlRequestCallback_OnCanceledFunc OnCanceledFunc_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestCallbackStub);
};

Cronet_UrlRequestCallbackPtr Cronet_UrlRequestCallback_CreateStub(
    Cronet_UrlRequestCallback_OnRedirectReceivedFunc OnRedirectReceivedFunc,
    Cronet_UrlRequestCallback_OnResponseStartedFunc OnResponseStartedFunc,
    Cronet_UrlRequestCallback_OnReadCompletedFunc OnReadCompletedFunc,
    Cronet_UrlRequestCallback_OnSucceededFunc OnSucceededFunc,
    Cronet_UrlRequestCallback_OnFailedFunc OnFailedFunc,
    Cronet_UrlRequestCallback_OnCanceledFunc OnCanceledFunc) {
  return new Cronet_UrlRequestCallbackStub(
      OnRedirectReceivedFunc, OnResponseStartedFunc, OnReadCompletedFunc,
      OnSucceededFunc, OnFailedFunc, OnCanceledFunc);
}

// C functions of Cronet_UploadDataSink that forward calls to C++
// implementation.
void Cronet_UploadDataSink_Destroy(Cronet_UploadDataSinkPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_UploadDataSink_SetContext(Cronet_UploadDataSinkPtr self,
                                      Cronet_UploadDataSinkContext context) {
  DCHECK(self);
  return self->SetContext(context);
}

Cronet_UploadDataSinkContext Cronet_UploadDataSink_GetContext(
    Cronet_UploadDataSinkPtr self) {
  DCHECK(self);
  return self->GetContext();
}

void Cronet_UploadDataSink_OnReadSucceeded(Cronet_UploadDataSinkPtr self,
                                           bool finalChunk) {
  DCHECK(self);
  self->OnReadSucceeded(finalChunk);
}

void Cronet_UploadDataSink_OnReadError(Cronet_UploadDataSinkPtr self,
                                       Cronet_ExceptionPtr error) {
  DCHECK(self);
  self->OnReadError(error);
}

void Cronet_UploadDataSink_OnRewindSucceded(Cronet_UploadDataSinkPtr self) {
  DCHECK(self);
  self->OnRewindSucceded();
}

void Cronet_UploadDataSink_OnRewindError(Cronet_UploadDataSinkPtr self,
                                         Cronet_ExceptionPtr error) {
  DCHECK(self);
  self->OnRewindError(error);
}

// Implementation of Cronet_UploadDataSink that forwards calls to C functions
// implemented by the app.
class Cronet_UploadDataSinkStub : public Cronet_UploadDataSink {
 public:
  Cronet_UploadDataSinkStub(
      Cronet_UploadDataSink_OnReadSucceededFunc OnReadSucceededFunc,
      Cronet_UploadDataSink_OnReadErrorFunc OnReadErrorFunc,
      Cronet_UploadDataSink_OnRewindSuccededFunc OnRewindSuccededFunc,
      Cronet_UploadDataSink_OnRewindErrorFunc OnRewindErrorFunc)
      : OnReadSucceededFunc_(OnReadSucceededFunc),
        OnReadErrorFunc_(OnReadErrorFunc),
        OnRewindSuccededFunc_(OnRewindSuccededFunc),
        OnRewindErrorFunc_(OnRewindErrorFunc) {}

  ~Cronet_UploadDataSinkStub() override {}

  void SetContext(Cronet_UploadDataSinkContext context) override {
    context_ = context;
  }

  Cronet_UploadDataSinkContext GetContext() override { return context_; }

 protected:
  void OnReadSucceeded(bool finalChunk) override {
    OnReadSucceededFunc_(this, finalChunk);
  }

  void OnReadError(Cronet_ExceptionPtr error) override {
    OnReadErrorFunc_(this, error);
  }

  void OnRewindSucceded() override { OnRewindSuccededFunc_(this); }

  void OnRewindError(Cronet_ExceptionPtr error) override {
    OnRewindErrorFunc_(this, error);
  }

 private:
  Cronet_UploadDataSinkContext context_ = nullptr;
  const Cronet_UploadDataSink_OnReadSucceededFunc OnReadSucceededFunc_;
  const Cronet_UploadDataSink_OnReadErrorFunc OnReadErrorFunc_;
  const Cronet_UploadDataSink_OnRewindSuccededFunc OnRewindSuccededFunc_;
  const Cronet_UploadDataSink_OnRewindErrorFunc OnRewindErrorFunc_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_UploadDataSinkStub);
};

Cronet_UploadDataSinkPtr Cronet_UploadDataSink_CreateStub(
    Cronet_UploadDataSink_OnReadSucceededFunc OnReadSucceededFunc,
    Cronet_UploadDataSink_OnReadErrorFunc OnReadErrorFunc,
    Cronet_UploadDataSink_OnRewindSuccededFunc OnRewindSuccededFunc,
    Cronet_UploadDataSink_OnRewindErrorFunc OnRewindErrorFunc) {
  return new Cronet_UploadDataSinkStub(OnReadSucceededFunc, OnReadErrorFunc,
                                       OnRewindSuccededFunc, OnRewindErrorFunc);
}

// C functions of Cronet_UploadDataProvider that forward calls to C++
// implementation.
void Cronet_UploadDataProvider_Destroy(Cronet_UploadDataProviderPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_UploadDataProvider_SetContext(
    Cronet_UploadDataProviderPtr self,
    Cronet_UploadDataProviderContext context) {
  DCHECK(self);
  return self->SetContext(context);
}

Cronet_UploadDataProviderContext Cronet_UploadDataProvider_GetContext(
    Cronet_UploadDataProviderPtr self) {
  DCHECK(self);
  return self->GetContext();
}

int64_t Cronet_UploadDataProvider_GetLength(Cronet_UploadDataProviderPtr self) {
  DCHECK(self);
  return self->GetLength();
}

void Cronet_UploadDataProvider_Read(Cronet_UploadDataProviderPtr self,
                                    Cronet_UploadDataSinkPtr uploadDataSink,
                                    Cronet_BufferPtr buffer) {
  DCHECK(self);
  self->Read(uploadDataSink, buffer);
}

void Cronet_UploadDataProvider_Rewind(Cronet_UploadDataProviderPtr self,
                                      Cronet_UploadDataSinkPtr uploadDataSink) {
  DCHECK(self);
  self->Rewind(uploadDataSink);
}

void Cronet_UploadDataProvider_Close(Cronet_UploadDataProviderPtr self) {
  DCHECK(self);
  self->Close();
}

// Implementation of Cronet_UploadDataProvider that forwards calls to C
// functions implemented by the app.
class Cronet_UploadDataProviderStub : public Cronet_UploadDataProvider {
 public:
  Cronet_UploadDataProviderStub(
      Cronet_UploadDataProvider_GetLengthFunc GetLengthFunc,
      Cronet_UploadDataProvider_ReadFunc ReadFunc,
      Cronet_UploadDataProvider_RewindFunc RewindFunc,
      Cronet_UploadDataProvider_CloseFunc CloseFunc)
      : GetLengthFunc_(GetLengthFunc),
        ReadFunc_(ReadFunc),
        RewindFunc_(RewindFunc),
        CloseFunc_(CloseFunc) {}

  ~Cronet_UploadDataProviderStub() override {}

  void SetContext(Cronet_UploadDataProviderContext context) override {
    context_ = context;
  }

  Cronet_UploadDataProviderContext GetContext() override { return context_; }

 protected:
  int64_t GetLength() override { return GetLengthFunc_(this); }

  void Read(Cronet_UploadDataSinkPtr uploadDataSink,
            Cronet_BufferPtr buffer) override {
    ReadFunc_(this, uploadDataSink, buffer);
  }

  void Rewind(Cronet_UploadDataSinkPtr uploadDataSink) override {
    RewindFunc_(this, uploadDataSink);
  }

  void Close() override { CloseFunc_(this); }

 private:
  Cronet_UploadDataProviderContext context_ = nullptr;
  const Cronet_UploadDataProvider_GetLengthFunc GetLengthFunc_;
  const Cronet_UploadDataProvider_ReadFunc ReadFunc_;
  const Cronet_UploadDataProvider_RewindFunc RewindFunc_;
  const Cronet_UploadDataProvider_CloseFunc CloseFunc_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_UploadDataProviderStub);
};

Cronet_UploadDataProviderPtr Cronet_UploadDataProvider_CreateStub(
    Cronet_UploadDataProvider_GetLengthFunc GetLengthFunc,
    Cronet_UploadDataProvider_ReadFunc ReadFunc,
    Cronet_UploadDataProvider_RewindFunc RewindFunc,
    Cronet_UploadDataProvider_CloseFunc CloseFunc) {
  return new Cronet_UploadDataProviderStub(GetLengthFunc, ReadFunc, RewindFunc,
                                           CloseFunc);
}

// C functions of Cronet_UrlRequest that forward calls to C++ implementation.
void Cronet_UrlRequest_Destroy(Cronet_UrlRequestPtr self) {
  DCHECK(self);
  return delete self;
}

void Cronet_UrlRequest_SetContext(Cronet_UrlRequestPtr self,
                                  Cronet_UrlRequestContext context) {
  DCHECK(self);
  return self->SetContext(context);
}

Cronet_UrlRequestContext Cronet_UrlRequest_GetContext(
    Cronet_UrlRequestPtr self) {
  DCHECK(self);
  return self->GetContext();
}

void Cronet_UrlRequest_InitWithParams(Cronet_UrlRequestPtr self,
                                      Cronet_EnginePtr engine,
                                      CharString url,
                                      Cronet_UrlRequestParamsPtr params,
                                      Cronet_UrlRequestCallbackPtr callback,
                                      Cronet_ExecutorPtr executor) {
  DCHECK(self);
  self->InitWithParams(engine, url, params, callback, executor);
}

void Cronet_UrlRequest_Start(Cronet_UrlRequestPtr self) {
  DCHECK(self);
  self->Start();
}

void Cronet_UrlRequest_FollowRedirect(Cronet_UrlRequestPtr self) {
  DCHECK(self);
  self->FollowRedirect();
}

void Cronet_UrlRequest_Read(Cronet_UrlRequestPtr self,
                            Cronet_BufferPtr buffer) {
  DCHECK(self);
  self->Read(buffer);
}

void Cronet_UrlRequest_Cancel(Cronet_UrlRequestPtr self) {
  DCHECK(self);
  self->Cancel();
}

bool Cronet_UrlRequest_IsDone(Cronet_UrlRequestPtr self) {
  DCHECK(self);
  return self->IsDone();
}

void Cronet_UrlRequest_GetStatus(Cronet_UrlRequestPtr self,
                                 Cronet_UrlRequestStatusListenerPtr listener) {
  DCHECK(self);
  self->GetStatus(listener);
}

// Implementation of Cronet_UrlRequest that forwards calls to C functions
// implemented by the app.
class Cronet_UrlRequestStub : public Cronet_UrlRequest {
 public:
  Cronet_UrlRequestStub(Cronet_UrlRequest_InitWithParamsFunc InitWithParamsFunc,
                        Cronet_UrlRequest_StartFunc StartFunc,
                        Cronet_UrlRequest_FollowRedirectFunc FollowRedirectFunc,
                        Cronet_UrlRequest_ReadFunc ReadFunc,
                        Cronet_UrlRequest_CancelFunc CancelFunc,
                        Cronet_UrlRequest_IsDoneFunc IsDoneFunc,
                        Cronet_UrlRequest_GetStatusFunc GetStatusFunc)
      : InitWithParamsFunc_(InitWithParamsFunc),
        StartFunc_(StartFunc),
        FollowRedirectFunc_(FollowRedirectFunc),
        ReadFunc_(ReadFunc),
        CancelFunc_(CancelFunc),
        IsDoneFunc_(IsDoneFunc),
        GetStatusFunc_(GetStatusFunc) {}

  ~Cronet_UrlRequestStub() override {}

  void SetContext(Cronet_UrlRequestContext context) override {
    context_ = context;
  }

  Cronet_UrlRequestContext GetContext() override { return context_; }

 protected:
  void InitWithParams(Cronet_EnginePtr engine,
                      CharString url,
                      Cronet_UrlRequestParamsPtr params,
                      Cronet_UrlRequestCallbackPtr callback,
                      Cronet_ExecutorPtr executor) override {
    InitWithParamsFunc_(this, engine, url, params, callback, executor);
  }

  void Start() override { StartFunc_(this); }

  void FollowRedirect() override { FollowRedirectFunc_(this); }

  void Read(Cronet_BufferPtr buffer) override { ReadFunc_(this, buffer); }

  void Cancel() override { CancelFunc_(this); }

  bool IsDone() override { return IsDoneFunc_(this); }

  void GetStatus(Cronet_UrlRequestStatusListenerPtr listener) override {
    GetStatusFunc_(this, listener);
  }

 private:
  Cronet_UrlRequestContext context_ = nullptr;
  const Cronet_UrlRequest_InitWithParamsFunc InitWithParamsFunc_;
  const Cronet_UrlRequest_StartFunc StartFunc_;
  const Cronet_UrlRequest_FollowRedirectFunc FollowRedirectFunc_;
  const Cronet_UrlRequest_ReadFunc ReadFunc_;
  const Cronet_UrlRequest_CancelFunc CancelFunc_;
  const Cronet_UrlRequest_IsDoneFunc IsDoneFunc_;
  const Cronet_UrlRequest_GetStatusFunc GetStatusFunc_;

  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestStub);
};

Cronet_UrlRequestPtr Cronet_UrlRequest_CreateStub(
    Cronet_UrlRequest_InitWithParamsFunc InitWithParamsFunc,
    Cronet_UrlRequest_StartFunc StartFunc,
    Cronet_UrlRequest_FollowRedirectFunc FollowRedirectFunc,
    Cronet_UrlRequest_ReadFunc ReadFunc,
    Cronet_UrlRequest_CancelFunc CancelFunc,
    Cronet_UrlRequest_IsDoneFunc IsDoneFunc,
    Cronet_UrlRequest_GetStatusFunc GetStatusFunc) {
  return new Cronet_UrlRequestStub(InitWithParamsFunc, StartFunc,
                                   FollowRedirectFunc, ReadFunc, CancelFunc,
                                   IsDoneFunc, GetStatusFunc);
}
