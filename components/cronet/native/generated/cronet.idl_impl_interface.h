// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#ifndef COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_INTERFACE_H_
#define COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_INTERFACE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/cronet/native/generated/cronet.idl_c.h"

struct Cronet_Buffer {
  Cronet_Buffer() = default;
  virtual ~Cronet_Buffer() = default;

  virtual void SetContext(Cronet_BufferContext context) = 0;
  virtual Cronet_BufferContext GetContext() = 0;

  virtual void InitWithDataAndCallback(RawDataPtr data,
                                       uint64_t size,
                                       Cronet_BufferCallbackPtr callback) = 0;
  virtual void InitWithAlloc(uint64_t size) = 0;
  virtual uint64_t GetSize() = 0;
  virtual RawDataPtr GetData() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_Buffer);
};

struct Cronet_BufferCallback {
  Cronet_BufferCallback() = default;
  virtual ~Cronet_BufferCallback() = default;

  virtual void SetContext(Cronet_BufferCallbackContext context) = 0;
  virtual Cronet_BufferCallbackContext GetContext() = 0;

  virtual void OnDestroy(Cronet_BufferPtr buffer) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_BufferCallback);
};

struct Cronet_Runnable {
  Cronet_Runnable() = default;
  virtual ~Cronet_Runnable() = default;

  virtual void SetContext(Cronet_RunnableContext context) = 0;
  virtual Cronet_RunnableContext GetContext() = 0;

  virtual void Run() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_Runnable);
};

struct Cronet_Executor {
  Cronet_Executor() = default;
  virtual ~Cronet_Executor() = default;

  virtual void SetContext(Cronet_ExecutorContext context) = 0;
  virtual Cronet_ExecutorContext GetContext() = 0;

  virtual void Execute(Cronet_RunnablePtr command) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_Executor);
};

struct Cronet_Engine {
  Cronet_Engine() = default;
  virtual ~Cronet_Engine() = default;

  virtual void SetContext(Cronet_EngineContext context) = 0;
  virtual Cronet_EngineContext GetContext() = 0;

  virtual void StartWithParams(Cronet_EngineParamsPtr params) = 0;
  virtual void StartNetLogToFile(CharString fileName, bool logAll) = 0;
  virtual void StopNetLog() = 0;
  virtual CharString GetVersionString() = 0;
  virtual CharString GetDefaultUserAgent() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_Engine);
};

struct Cronet_UrlRequestStatusListener {
  Cronet_UrlRequestStatusListener() = default;
  virtual ~Cronet_UrlRequestStatusListener() = default;

  virtual void SetContext(Cronet_UrlRequestStatusListenerContext context) = 0;
  virtual Cronet_UrlRequestStatusListenerContext GetContext() = 0;

  virtual void OnStatus(Cronet_UrlRequestStatusListener_Status status) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestStatusListener);
};

struct Cronet_UrlRequestCallback {
  Cronet_UrlRequestCallback() = default;
  virtual ~Cronet_UrlRequestCallback() = default;

  virtual void SetContext(Cronet_UrlRequestCallbackContext context) = 0;
  virtual Cronet_UrlRequestCallbackContext GetContext() = 0;

  virtual void OnRedirectReceived(Cronet_UrlRequestPtr request,
                                  Cronet_UrlResponseInfoPtr info,
                                  CharString newLocationUrl) = 0;
  virtual void OnResponseStarted(Cronet_UrlRequestPtr request,
                                 Cronet_UrlResponseInfoPtr info) = 0;
  virtual void OnReadCompleted(Cronet_UrlRequestPtr request,
                               Cronet_UrlResponseInfoPtr info,
                               Cronet_BufferPtr buffer) = 0;
  virtual void OnSucceeded(Cronet_UrlRequestPtr request,
                           Cronet_UrlResponseInfoPtr info) = 0;
  virtual void OnFailed(Cronet_UrlRequestPtr request,
                        Cronet_UrlResponseInfoPtr info,
                        Cronet_ExceptionPtr error) = 0;
  virtual void OnCanceled(Cronet_UrlRequestPtr request,
                          Cronet_UrlResponseInfoPtr info) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequestCallback);
};

struct Cronet_UploadDataSink {
  Cronet_UploadDataSink() = default;
  virtual ~Cronet_UploadDataSink() = default;

  virtual void SetContext(Cronet_UploadDataSinkContext context) = 0;
  virtual Cronet_UploadDataSinkContext GetContext() = 0;

  virtual void OnReadSucceeded(bool finalChunk) = 0;
  virtual void OnReadError(Cronet_ExceptionPtr error) = 0;
  virtual void OnRewindSucceded() = 0;
  virtual void OnRewindError(Cronet_ExceptionPtr error) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UploadDataSink);
};

struct Cronet_UploadDataProvider {
  Cronet_UploadDataProvider() = default;
  virtual ~Cronet_UploadDataProvider() = default;

  virtual void SetContext(Cronet_UploadDataProviderContext context) = 0;
  virtual Cronet_UploadDataProviderContext GetContext() = 0;

  virtual int64_t GetLength() = 0;
  virtual void Read(Cronet_UploadDataSinkPtr uploadDataSink,
                    Cronet_BufferPtr buffer) = 0;
  virtual void Rewind(Cronet_UploadDataSinkPtr uploadDataSink) = 0;
  virtual void Close() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UploadDataProvider);
};

struct Cronet_UrlRequest {
  Cronet_UrlRequest() = default;
  virtual ~Cronet_UrlRequest() = default;

  virtual void SetContext(Cronet_UrlRequestContext context) = 0;
  virtual Cronet_UrlRequestContext GetContext() = 0;

  virtual void InitWithParams(Cronet_EnginePtr engine,
                              CharString url,
                              Cronet_UrlRequestParamsPtr params,
                              Cronet_UrlRequestCallbackPtr callback,
                              Cronet_ExecutorPtr executor) = 0;
  virtual void Start() = 0;
  virtual void FollowRedirect() = 0;
  virtual void Read(Cronet_BufferPtr buffer) = 0;
  virtual void Cancel() = 0;
  virtual bool IsDone() = 0;
  virtual void GetStatus(Cronet_UrlRequestStatusListenerPtr listener) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cronet_UrlRequest);
};

#endif  // COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_INTERFACE_H_
