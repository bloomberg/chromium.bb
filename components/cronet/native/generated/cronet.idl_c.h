// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#ifndef COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_C_H_
#define COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_C_H_
#include "cronet_export.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef const char* CharString;
typedef void* RawDataPtr;

// Forward declare interfaces.
typedef struct Cronet_Buffer Cronet_Buffer;
typedef struct Cronet_Buffer* Cronet_BufferPtr;
typedef void* Cronet_BufferContext;
typedef struct Cronet_BufferCallback Cronet_BufferCallback;
typedef struct Cronet_BufferCallback* Cronet_BufferCallbackPtr;
typedef void* Cronet_BufferCallbackContext;
typedef struct Cronet_Runnable Cronet_Runnable;
typedef struct Cronet_Runnable* Cronet_RunnablePtr;
typedef void* Cronet_RunnableContext;
typedef struct Cronet_Executor Cronet_Executor;
typedef struct Cronet_Executor* Cronet_ExecutorPtr;
typedef void* Cronet_ExecutorContext;
typedef struct Cronet_Engine Cronet_Engine;
typedef struct Cronet_Engine* Cronet_EnginePtr;
typedef void* Cronet_EngineContext;
typedef struct Cronet_UrlRequestStatusListener Cronet_UrlRequestStatusListener;
typedef struct Cronet_UrlRequestStatusListener*
    Cronet_UrlRequestStatusListenerPtr;
typedef void* Cronet_UrlRequestStatusListenerContext;
typedef struct Cronet_UrlRequestCallback Cronet_UrlRequestCallback;
typedef struct Cronet_UrlRequestCallback* Cronet_UrlRequestCallbackPtr;
typedef void* Cronet_UrlRequestCallbackContext;
typedef struct Cronet_UploadDataSink Cronet_UploadDataSink;
typedef struct Cronet_UploadDataSink* Cronet_UploadDataSinkPtr;
typedef void* Cronet_UploadDataSinkContext;
typedef struct Cronet_UploadDataProvider Cronet_UploadDataProvider;
typedef struct Cronet_UploadDataProvider* Cronet_UploadDataProviderPtr;
typedef void* Cronet_UploadDataProviderContext;
typedef struct Cronet_UrlRequest Cronet_UrlRequest;
typedef struct Cronet_UrlRequest* Cronet_UrlRequestPtr;
typedef void* Cronet_UrlRequestContext;

// Forward declare structs.
typedef struct Cronet_Exception Cronet_Exception;
typedef struct Cronet_Exception* Cronet_ExceptionPtr;
typedef struct Cronet_QuicHint Cronet_QuicHint;
typedef struct Cronet_QuicHint* Cronet_QuicHintPtr;
typedef struct Cronet_PublicKeyPins Cronet_PublicKeyPins;
typedef struct Cronet_PublicKeyPins* Cronet_PublicKeyPinsPtr;
typedef struct Cronet_EngineParams Cronet_EngineParams;
typedef struct Cronet_EngineParams* Cronet_EngineParamsPtr;
typedef struct Cronet_HttpHeader Cronet_HttpHeader;
typedef struct Cronet_HttpHeader* Cronet_HttpHeaderPtr;
typedef struct Cronet_UrlResponseInfo Cronet_UrlResponseInfo;
typedef struct Cronet_UrlResponseInfo* Cronet_UrlResponseInfoPtr;
typedef struct Cronet_UrlRequestParams Cronet_UrlRequestParams;
typedef struct Cronet_UrlRequestParams* Cronet_UrlRequestParamsPtr;

// Declare enums
typedef enum Cronet_Exception_ERROR_CODE {
  Cronet_Exception_ERROR_CODE_ERROR_CALLBACK = 0,
  Cronet_Exception_ERROR_CODE_ERROR_HOSTNAME_NOT_RESOLVED = 1,
  Cronet_Exception_ERROR_CODE_ERROR_INTERNET_DISCONNECTED = 2,
  Cronet_Exception_ERROR_CODE_ERROR_NETWORK_CHANGED = 3,
  Cronet_Exception_ERROR_CODE_ERROR_TIMED_OUT = 4,
  Cronet_Exception_ERROR_CODE_ERROR_CONNECTION_CLOSED = 5,
  Cronet_Exception_ERROR_CODE_ERROR_CONNECTION_TIMED_OUT = 6,
  Cronet_Exception_ERROR_CODE_ERROR_CONNECTION_REFUSED = 7,
  Cronet_Exception_ERROR_CODE_ERROR_CONNECTION_RESET = 8,
  Cronet_Exception_ERROR_CODE_ERROR_ADDRESS_UNREACHABLE = 9,
  Cronet_Exception_ERROR_CODE_ERROR_QUIC_PROTOCOL_FAILED = 10,
  Cronet_Exception_ERROR_CODE_ERROR_OTHER = 11,
} Cronet_Exception_ERROR_CODE;

typedef enum Cronet_EngineParams_HTTP_CACHE_MODE {
  Cronet_EngineParams_HTTP_CACHE_MODE_DISABLED = 0,
  Cronet_EngineParams_HTTP_CACHE_MODE_IN_MEMORY = 1,
  Cronet_EngineParams_HTTP_CACHE_MODE_DISK_NO_HTTP = 2,
  Cronet_EngineParams_HTTP_CACHE_MODE_DISK = 3,
} Cronet_EngineParams_HTTP_CACHE_MODE;

typedef enum Cronet_UrlRequestParams_REQUEST_PRIORITY {
  Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_IDLE = 0,
  Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_LOWEST = 1,
  Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_LOW = 2,
  Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_MEDIUM = 3,
  Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_HIGHEST = 4,
} Cronet_UrlRequestParams_REQUEST_PRIORITY;

typedef enum Cronet_UrlRequestStatusListener_Status {
  Cronet_UrlRequestStatusListener_Status_INVALID = -1,
  Cronet_UrlRequestStatusListener_Status_IDLE = 0,
  Cronet_UrlRequestStatusListener_Status_WAITING_FOR_STALLED_SOCKET_POOL = 1,
  Cronet_UrlRequestStatusListener_Status_WAITING_FOR_AVAILABLE_SOCKET = 2,
  Cronet_UrlRequestStatusListener_Status_WAITING_FOR_DELEGATE = 3,
  Cronet_UrlRequestStatusListener_Status_WAITING_FOR_CACHE = 4,
  Cronet_UrlRequestStatusListener_Status_DOWNLOADING_PROXY_SCRIPT = 5,
  Cronet_UrlRequestStatusListener_Status_RESOLVING_PROXY_FOR_URL = 6,
  Cronet_UrlRequestStatusListener_Status_RESOLVING_HOST_IN_PROXY_SCRIPT = 7,
  Cronet_UrlRequestStatusListener_Status_ESTABLISHING_PROXY_TUNNEL = 8,
  Cronet_UrlRequestStatusListener_Status_RESOLVING_HOST = 9,
  Cronet_UrlRequestStatusListener_Status_CONNECTING = 10,
  Cronet_UrlRequestStatusListener_Status_SSL_HANDSHAKE = 11,
  Cronet_UrlRequestStatusListener_Status_SENDING_REQUEST = 12,
  Cronet_UrlRequestStatusListener_Status_WAITING_FOR_RESPONSE = 13,
  Cronet_UrlRequestStatusListener_Status_READING_RESPONSE = 14,
} Cronet_UrlRequestStatusListener_Status;

///////////////////////
// Concrete interface Cronet_Buffer.

// Create an instance of Cronet_Buffer.
CRONET_EXPORT Cronet_BufferPtr Cronet_Buffer_Create();
// Destroy an instance of Cronet_Buffer.
CRONET_EXPORT void Cronet_Buffer_Destroy(Cronet_BufferPtr self);
// Set and get app-specific Cronet_BufferContext.
CRONET_EXPORT void Cronet_Buffer_SetContext(Cronet_BufferPtr self,
                                            Cronet_BufferContext);
CRONET_EXPORT Cronet_BufferContext
Cronet_Buffer_GetContext(Cronet_BufferPtr self);
// Concrete methods of Cronet_Buffer implemented by Cronet.
// The app calls them to manipulate Cronet_Buffer.
CRONET_EXPORT
void Cronet_Buffer_InitWithDataAndCallback(Cronet_BufferPtr self,
                                           RawDataPtr data,
                                           uint64_t size,
                                           Cronet_BufferCallbackPtr callback);
CRONET_EXPORT
void Cronet_Buffer_InitWithAlloc(Cronet_BufferPtr self, uint64_t size);
CRONET_EXPORT
uint64_t Cronet_Buffer_GetSize(Cronet_BufferPtr self);
CRONET_EXPORT
RawDataPtr Cronet_Buffer_GetData(Cronet_BufferPtr self);
// Concrete interface Cronet_Buffer is implemented by Cronet.
// The app can implement these for testing / mocking.
typedef void (*Cronet_Buffer_InitWithDataAndCallbackFunc)(
    Cronet_BufferPtr self,
    RawDataPtr data,
    uint64_t size,
    Cronet_BufferCallbackPtr callback);
typedef void (*Cronet_Buffer_InitWithAllocFunc)(Cronet_BufferPtr self,
                                                uint64_t size);
typedef uint64_t (*Cronet_Buffer_GetSizeFunc)(Cronet_BufferPtr self);
typedef RawDataPtr (*Cronet_Buffer_GetDataFunc)(Cronet_BufferPtr self);
// Concrete interface Cronet_Buffer is implemented by Cronet.
// The app can use this for testing / mocking.
CRONET_EXPORT Cronet_BufferPtr Cronet_Buffer_CreateStub(
    Cronet_Buffer_InitWithDataAndCallbackFunc InitWithDataAndCallbackFunc,
    Cronet_Buffer_InitWithAllocFunc InitWithAllocFunc,
    Cronet_Buffer_GetSizeFunc GetSizeFunc,
    Cronet_Buffer_GetDataFunc GetDataFunc);

///////////////////////
// Abstract interface Cronet_BufferCallback is implemented by the app.

// There is no method to create a concrete implementation.

// Destroy an instance of Cronet_BufferCallback.
CRONET_EXPORT void Cronet_BufferCallback_Destroy(Cronet_BufferCallbackPtr self);
// Set and get app-specific Cronet_BufferCallbackContext.
CRONET_EXPORT void Cronet_BufferCallback_SetContext(
    Cronet_BufferCallbackPtr self,
    Cronet_BufferCallbackContext);
CRONET_EXPORT Cronet_BufferCallbackContext
Cronet_BufferCallback_GetContext(Cronet_BufferCallbackPtr self);
// Abstract interface Cronet_BufferCallback is implemented by the app.
// The following concrete methods forward call to app implementation.
// The app doesn't normally call them.
CRONET_EXPORT
void Cronet_BufferCallback_OnDestroy(Cronet_BufferCallbackPtr self,
                                     Cronet_BufferPtr buffer);
// The app implements abstract interface Cronet_BufferCallback by defining
// custom functions for each method.
typedef void (*Cronet_BufferCallback_OnDestroyFunc)(
    Cronet_BufferCallbackPtr self,
    Cronet_BufferPtr buffer);
// The app creates an instance of Cronet_BufferCallback by providing custom
// functions for each method.
CRONET_EXPORT Cronet_BufferCallbackPtr Cronet_BufferCallback_CreateStub(
    Cronet_BufferCallback_OnDestroyFunc OnDestroyFunc);

///////////////////////
// Abstract interface Cronet_Runnable is implemented by the app.

// There is no method to create a concrete implementation.

// Destroy an instance of Cronet_Runnable.
CRONET_EXPORT void Cronet_Runnable_Destroy(Cronet_RunnablePtr self);
// Set and get app-specific Cronet_RunnableContext.
CRONET_EXPORT void Cronet_Runnable_SetContext(Cronet_RunnablePtr self,
                                              Cronet_RunnableContext);
CRONET_EXPORT Cronet_RunnableContext
Cronet_Runnable_GetContext(Cronet_RunnablePtr self);
// Abstract interface Cronet_Runnable is implemented by the app.
// The following concrete methods forward call to app implementation.
// The app doesn't normally call them.
CRONET_EXPORT
void Cronet_Runnable_Run(Cronet_RunnablePtr self);
// The app implements abstract interface Cronet_Runnable by defining custom
// functions for each method.
typedef void (*Cronet_Runnable_RunFunc)(Cronet_RunnablePtr self);
// The app creates an instance of Cronet_Runnable by providing custom functions
// for each method.
CRONET_EXPORT Cronet_RunnablePtr
Cronet_Runnable_CreateStub(Cronet_Runnable_RunFunc RunFunc);

///////////////////////
// Abstract interface Cronet_Executor is implemented by the app.

// There is no method to create a concrete implementation.

// Destroy an instance of Cronet_Executor.
CRONET_EXPORT void Cronet_Executor_Destroy(Cronet_ExecutorPtr self);
// Set and get app-specific Cronet_ExecutorContext.
CRONET_EXPORT void Cronet_Executor_SetContext(Cronet_ExecutorPtr self,
                                              Cronet_ExecutorContext);
CRONET_EXPORT Cronet_ExecutorContext
Cronet_Executor_GetContext(Cronet_ExecutorPtr self);
// Abstract interface Cronet_Executor is implemented by the app.
// The following concrete methods forward call to app implementation.
// The app doesn't normally call them.
CRONET_EXPORT
void Cronet_Executor_Execute(Cronet_ExecutorPtr self,
                             Cronet_RunnablePtr command);
// The app implements abstract interface Cronet_Executor by defining custom
// functions for each method.
typedef void (*Cronet_Executor_ExecuteFunc)(Cronet_ExecutorPtr self,
                                            Cronet_RunnablePtr command);
// The app creates an instance of Cronet_Executor by providing custom functions
// for each method.
CRONET_EXPORT Cronet_ExecutorPtr
Cronet_Executor_CreateStub(Cronet_Executor_ExecuteFunc ExecuteFunc);

///////////////////////
// Concrete interface Cronet_Engine.

// Create an instance of Cronet_Engine.
CRONET_EXPORT Cronet_EnginePtr Cronet_Engine_Create();
// Destroy an instance of Cronet_Engine.
CRONET_EXPORT void Cronet_Engine_Destroy(Cronet_EnginePtr self);
// Set and get app-specific Cronet_EngineContext.
CRONET_EXPORT void Cronet_Engine_SetContext(Cronet_EnginePtr self,
                                            Cronet_EngineContext);
CRONET_EXPORT Cronet_EngineContext
Cronet_Engine_GetContext(Cronet_EnginePtr self);
// Concrete methods of Cronet_Engine implemented by Cronet.
// The app calls them to manipulate Cronet_Engine.
CRONET_EXPORT
void Cronet_Engine_StartWithParams(Cronet_EnginePtr self,
                                   Cronet_EngineParamsPtr params);
CRONET_EXPORT
void Cronet_Engine_StartNetLogToFile(Cronet_EnginePtr self,
                                     CharString fileName,
                                     bool logAll);
CRONET_EXPORT
void Cronet_Engine_StopNetLog(Cronet_EnginePtr self);
CRONET_EXPORT
CharString Cronet_Engine_GetVersionString(Cronet_EnginePtr self);
CRONET_EXPORT
CharString Cronet_Engine_GetDefaultUserAgent(Cronet_EnginePtr self);
// Concrete interface Cronet_Engine is implemented by Cronet.
// The app can implement these for testing / mocking.
typedef void (*Cronet_Engine_StartWithParamsFunc)(
    Cronet_EnginePtr self,
    Cronet_EngineParamsPtr params);
typedef void (*Cronet_Engine_StartNetLogToFileFunc)(Cronet_EnginePtr self,
                                                    CharString fileName,
                                                    bool logAll);
typedef void (*Cronet_Engine_StopNetLogFunc)(Cronet_EnginePtr self);
typedef CharString (*Cronet_Engine_GetVersionStringFunc)(Cronet_EnginePtr self);
typedef CharString (*Cronet_Engine_GetDefaultUserAgentFunc)(
    Cronet_EnginePtr self);
// Concrete interface Cronet_Engine is implemented by Cronet.
// The app can use this for testing / mocking.
CRONET_EXPORT Cronet_EnginePtr Cronet_Engine_CreateStub(
    Cronet_Engine_StartWithParamsFunc StartWithParamsFunc,
    Cronet_Engine_StartNetLogToFileFunc StartNetLogToFileFunc,
    Cronet_Engine_StopNetLogFunc StopNetLogFunc,
    Cronet_Engine_GetVersionStringFunc GetVersionStringFunc,
    Cronet_Engine_GetDefaultUserAgentFunc GetDefaultUserAgentFunc);

///////////////////////
// Abstract interface Cronet_UrlRequestStatusListener is implemented by the app.

// There is no method to create a concrete implementation.

// Destroy an instance of Cronet_UrlRequestStatusListener.
CRONET_EXPORT void Cronet_UrlRequestStatusListener_Destroy(
    Cronet_UrlRequestStatusListenerPtr self);
// Set and get app-specific Cronet_UrlRequestStatusListenerContext.
CRONET_EXPORT void Cronet_UrlRequestStatusListener_SetContext(
    Cronet_UrlRequestStatusListenerPtr self,
    Cronet_UrlRequestStatusListenerContext);
CRONET_EXPORT Cronet_UrlRequestStatusListenerContext
Cronet_UrlRequestStatusListener_GetContext(
    Cronet_UrlRequestStatusListenerPtr self);
// Abstract interface Cronet_UrlRequestStatusListener is implemented by the app.
// The following concrete methods forward call to app implementation.
// The app doesn't normally call them.
CRONET_EXPORT
void Cronet_UrlRequestStatusListener_OnStatus(
    Cronet_UrlRequestStatusListenerPtr self,
    Cronet_UrlRequestStatusListener_Status status);
// The app implements abstract interface Cronet_UrlRequestStatusListener by
// defining custom functions for each method.
typedef void (*Cronet_UrlRequestStatusListener_OnStatusFunc)(
    Cronet_UrlRequestStatusListenerPtr self,
    Cronet_UrlRequestStatusListener_Status status);
// The app creates an instance of Cronet_UrlRequestStatusListener by providing
// custom functions for each method.
CRONET_EXPORT Cronet_UrlRequestStatusListenerPtr
Cronet_UrlRequestStatusListener_CreateStub(
    Cronet_UrlRequestStatusListener_OnStatusFunc OnStatusFunc);

///////////////////////
// Abstract interface Cronet_UrlRequestCallback is implemented by the app.

// There is no method to create a concrete implementation.

// Destroy an instance of Cronet_UrlRequestCallback.
CRONET_EXPORT void Cronet_UrlRequestCallback_Destroy(
    Cronet_UrlRequestCallbackPtr self);
// Set and get app-specific Cronet_UrlRequestCallbackContext.
CRONET_EXPORT void Cronet_UrlRequestCallback_SetContext(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestCallbackContext);
CRONET_EXPORT Cronet_UrlRequestCallbackContext
Cronet_UrlRequestCallback_GetContext(Cronet_UrlRequestCallbackPtr self);
// Abstract interface Cronet_UrlRequestCallback is implemented by the app.
// The following concrete methods forward call to app implementation.
// The app doesn't normally call them.
CRONET_EXPORT
void Cronet_UrlRequestCallback_OnRedirectReceived(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    CharString newLocationUrl);
CRONET_EXPORT
void Cronet_UrlRequestCallback_OnResponseStarted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info);
CRONET_EXPORT
void Cronet_UrlRequestCallback_OnReadCompleted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_BufferPtr buffer);
CRONET_EXPORT
void Cronet_UrlRequestCallback_OnSucceeded(Cronet_UrlRequestCallbackPtr self,
                                           Cronet_UrlRequestPtr request,
                                           Cronet_UrlResponseInfoPtr info);
CRONET_EXPORT
void Cronet_UrlRequestCallback_OnFailed(Cronet_UrlRequestCallbackPtr self,
                                        Cronet_UrlRequestPtr request,
                                        Cronet_UrlResponseInfoPtr info,
                                        Cronet_ExceptionPtr error);
CRONET_EXPORT
void Cronet_UrlRequestCallback_OnCanceled(Cronet_UrlRequestCallbackPtr self,
                                          Cronet_UrlRequestPtr request,
                                          Cronet_UrlResponseInfoPtr info);
// The app implements abstract interface Cronet_UrlRequestCallback by defining
// custom functions for each method.
typedef void (*Cronet_UrlRequestCallback_OnRedirectReceivedFunc)(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    CharString newLocationUrl);
typedef void (*Cronet_UrlRequestCallback_OnResponseStartedFunc)(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info);
typedef void (*Cronet_UrlRequestCallback_OnReadCompletedFunc)(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_BufferPtr buffer);
typedef void (*Cronet_UrlRequestCallback_OnSucceededFunc)(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info);
typedef void (*Cronet_UrlRequestCallback_OnFailedFunc)(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_ExceptionPtr error);
typedef void (*Cronet_UrlRequestCallback_OnCanceledFunc)(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info);
// The app creates an instance of Cronet_UrlRequestCallback by providing custom
// functions for each method.
CRONET_EXPORT Cronet_UrlRequestCallbackPtr Cronet_UrlRequestCallback_CreateStub(
    Cronet_UrlRequestCallback_OnRedirectReceivedFunc OnRedirectReceivedFunc,
    Cronet_UrlRequestCallback_OnResponseStartedFunc OnResponseStartedFunc,
    Cronet_UrlRequestCallback_OnReadCompletedFunc OnReadCompletedFunc,
    Cronet_UrlRequestCallback_OnSucceededFunc OnSucceededFunc,
    Cronet_UrlRequestCallback_OnFailedFunc OnFailedFunc,
    Cronet_UrlRequestCallback_OnCanceledFunc OnCanceledFunc);

///////////////////////
// Concrete interface Cronet_UploadDataSink.

// Create an instance of Cronet_UploadDataSink.
CRONET_EXPORT Cronet_UploadDataSinkPtr Cronet_UploadDataSink_Create();
// Destroy an instance of Cronet_UploadDataSink.
CRONET_EXPORT void Cronet_UploadDataSink_Destroy(Cronet_UploadDataSinkPtr self);
// Set and get app-specific Cronet_UploadDataSinkContext.
CRONET_EXPORT void Cronet_UploadDataSink_SetContext(
    Cronet_UploadDataSinkPtr self,
    Cronet_UploadDataSinkContext);
CRONET_EXPORT Cronet_UploadDataSinkContext
Cronet_UploadDataSink_GetContext(Cronet_UploadDataSinkPtr self);
// Concrete methods of Cronet_UploadDataSink implemented by Cronet.
// The app calls them to manipulate Cronet_UploadDataSink.
CRONET_EXPORT
void Cronet_UploadDataSink_OnReadSucceeded(Cronet_UploadDataSinkPtr self,
                                           bool finalChunk);
CRONET_EXPORT
void Cronet_UploadDataSink_OnReadError(Cronet_UploadDataSinkPtr self,
                                       Cronet_ExceptionPtr error);
CRONET_EXPORT
void Cronet_UploadDataSink_OnRewindSucceded(Cronet_UploadDataSinkPtr self);
CRONET_EXPORT
void Cronet_UploadDataSink_OnRewindError(Cronet_UploadDataSinkPtr self,
                                         Cronet_ExceptionPtr error);
// Concrete interface Cronet_UploadDataSink is implemented by Cronet.
// The app can implement these for testing / mocking.
typedef void (*Cronet_UploadDataSink_OnReadSucceededFunc)(
    Cronet_UploadDataSinkPtr self,
    bool finalChunk);
typedef void (*Cronet_UploadDataSink_OnReadErrorFunc)(
    Cronet_UploadDataSinkPtr self,
    Cronet_ExceptionPtr error);
typedef void (*Cronet_UploadDataSink_OnRewindSuccededFunc)(
    Cronet_UploadDataSinkPtr self);
typedef void (*Cronet_UploadDataSink_OnRewindErrorFunc)(
    Cronet_UploadDataSinkPtr self,
    Cronet_ExceptionPtr error);
// Concrete interface Cronet_UploadDataSink is implemented by Cronet.
// The app can use this for testing / mocking.
CRONET_EXPORT Cronet_UploadDataSinkPtr Cronet_UploadDataSink_CreateStub(
    Cronet_UploadDataSink_OnReadSucceededFunc OnReadSucceededFunc,
    Cronet_UploadDataSink_OnReadErrorFunc OnReadErrorFunc,
    Cronet_UploadDataSink_OnRewindSuccededFunc OnRewindSuccededFunc,
    Cronet_UploadDataSink_OnRewindErrorFunc OnRewindErrorFunc);

///////////////////////
// Abstract interface Cronet_UploadDataProvider is implemented by the app.

// There is no method to create a concrete implementation.

// Destroy an instance of Cronet_UploadDataProvider.
CRONET_EXPORT void Cronet_UploadDataProvider_Destroy(
    Cronet_UploadDataProviderPtr self);
// Set and get app-specific Cronet_UploadDataProviderContext.
CRONET_EXPORT void Cronet_UploadDataProvider_SetContext(
    Cronet_UploadDataProviderPtr self,
    Cronet_UploadDataProviderContext);
CRONET_EXPORT Cronet_UploadDataProviderContext
Cronet_UploadDataProvider_GetContext(Cronet_UploadDataProviderPtr self);
// Abstract interface Cronet_UploadDataProvider is implemented by the app.
// The following concrete methods forward call to app implementation.
// The app doesn't normally call them.
CRONET_EXPORT
int64_t Cronet_UploadDataProvider_GetLength(Cronet_UploadDataProviderPtr self);
CRONET_EXPORT
void Cronet_UploadDataProvider_Read(Cronet_UploadDataProviderPtr self,
                                    Cronet_UploadDataSinkPtr uploadDataSink,
                                    Cronet_BufferPtr buffer);
CRONET_EXPORT
void Cronet_UploadDataProvider_Rewind(Cronet_UploadDataProviderPtr self,
                                      Cronet_UploadDataSinkPtr uploadDataSink);
CRONET_EXPORT
void Cronet_UploadDataProvider_Close(Cronet_UploadDataProviderPtr self);
// The app implements abstract interface Cronet_UploadDataProvider by defining
// custom functions for each method.
typedef int64_t (*Cronet_UploadDataProvider_GetLengthFunc)(
    Cronet_UploadDataProviderPtr self);
typedef void (*Cronet_UploadDataProvider_ReadFunc)(
    Cronet_UploadDataProviderPtr self,
    Cronet_UploadDataSinkPtr uploadDataSink,
    Cronet_BufferPtr buffer);
typedef void (*Cronet_UploadDataProvider_RewindFunc)(
    Cronet_UploadDataProviderPtr self,
    Cronet_UploadDataSinkPtr uploadDataSink);
typedef void (*Cronet_UploadDataProvider_CloseFunc)(
    Cronet_UploadDataProviderPtr self);
// The app creates an instance of Cronet_UploadDataProvider by providing custom
// functions for each method.
CRONET_EXPORT Cronet_UploadDataProviderPtr Cronet_UploadDataProvider_CreateStub(
    Cronet_UploadDataProvider_GetLengthFunc GetLengthFunc,
    Cronet_UploadDataProvider_ReadFunc ReadFunc,
    Cronet_UploadDataProvider_RewindFunc RewindFunc,
    Cronet_UploadDataProvider_CloseFunc CloseFunc);

///////////////////////
// Concrete interface Cronet_UrlRequest.

// Create an instance of Cronet_UrlRequest.
CRONET_EXPORT Cronet_UrlRequestPtr Cronet_UrlRequest_Create();
// Destroy an instance of Cronet_UrlRequest.
CRONET_EXPORT void Cronet_UrlRequest_Destroy(Cronet_UrlRequestPtr self);
// Set and get app-specific Cronet_UrlRequestContext.
CRONET_EXPORT void Cronet_UrlRequest_SetContext(Cronet_UrlRequestPtr self,
                                                Cronet_UrlRequestContext);
CRONET_EXPORT Cronet_UrlRequestContext
Cronet_UrlRequest_GetContext(Cronet_UrlRequestPtr self);
// Concrete methods of Cronet_UrlRequest implemented by Cronet.
// The app calls them to manipulate Cronet_UrlRequest.
CRONET_EXPORT
void Cronet_UrlRequest_InitWithParams(Cronet_UrlRequestPtr self,
                                      Cronet_EnginePtr engine,
                                      CharString url,
                                      Cronet_UrlRequestParamsPtr params,
                                      Cronet_UrlRequestCallbackPtr callback,
                                      Cronet_ExecutorPtr executor);
CRONET_EXPORT
void Cronet_UrlRequest_Start(Cronet_UrlRequestPtr self);
CRONET_EXPORT
void Cronet_UrlRequest_FollowRedirect(Cronet_UrlRequestPtr self);
CRONET_EXPORT
void Cronet_UrlRequest_Read(Cronet_UrlRequestPtr self, Cronet_BufferPtr buffer);
CRONET_EXPORT
void Cronet_UrlRequest_Cancel(Cronet_UrlRequestPtr self);
CRONET_EXPORT
bool Cronet_UrlRequest_IsDone(Cronet_UrlRequestPtr self);
CRONET_EXPORT
void Cronet_UrlRequest_GetStatus(Cronet_UrlRequestPtr self,
                                 Cronet_UrlRequestStatusListenerPtr listener);
// Concrete interface Cronet_UrlRequest is implemented by Cronet.
// The app can implement these for testing / mocking.
typedef void (*Cronet_UrlRequest_InitWithParamsFunc)(
    Cronet_UrlRequestPtr self,
    Cronet_EnginePtr engine,
    CharString url,
    Cronet_UrlRequestParamsPtr params,
    Cronet_UrlRequestCallbackPtr callback,
    Cronet_ExecutorPtr executor);
typedef void (*Cronet_UrlRequest_StartFunc)(Cronet_UrlRequestPtr self);
typedef void (*Cronet_UrlRequest_FollowRedirectFunc)(Cronet_UrlRequestPtr self);
typedef void (*Cronet_UrlRequest_ReadFunc)(Cronet_UrlRequestPtr self,
                                           Cronet_BufferPtr buffer);
typedef void (*Cronet_UrlRequest_CancelFunc)(Cronet_UrlRequestPtr self);
typedef bool (*Cronet_UrlRequest_IsDoneFunc)(Cronet_UrlRequestPtr self);
typedef void (*Cronet_UrlRequest_GetStatusFunc)(
    Cronet_UrlRequestPtr self,
    Cronet_UrlRequestStatusListenerPtr listener);
// Concrete interface Cronet_UrlRequest is implemented by Cronet.
// The app can use this for testing / mocking.
CRONET_EXPORT Cronet_UrlRequestPtr Cronet_UrlRequest_CreateStub(
    Cronet_UrlRequest_InitWithParamsFunc InitWithParamsFunc,
    Cronet_UrlRequest_StartFunc StartFunc,
    Cronet_UrlRequest_FollowRedirectFunc FollowRedirectFunc,
    Cronet_UrlRequest_ReadFunc ReadFunc,
    Cronet_UrlRequest_CancelFunc CancelFunc,
    Cronet_UrlRequest_IsDoneFunc IsDoneFunc,
    Cronet_UrlRequest_GetStatusFunc GetStatusFunc);

///////////////////////
// Struct Cronet_Exception.
CRONET_EXPORT Cronet_ExceptionPtr Cronet_Exception_Create();
CRONET_EXPORT void Cronet_Exception_Destroy(Cronet_ExceptionPtr self);
// Cronet_Exception setters.
CRONET_EXPORT
void Cronet_Exception_set_error_code(Cronet_ExceptionPtr self,
                                     Cronet_Exception_ERROR_CODE error_code);
CRONET_EXPORT
void Cronet_Exception_set_internal_error_code(Cronet_ExceptionPtr self,
                                              int32_t internal_error_code);
CRONET_EXPORT
void Cronet_Exception_set_immediately_retryable(Cronet_ExceptionPtr self,
                                                bool immediately_retryable);
CRONET_EXPORT
void Cronet_Exception_set_quic_detailed_error_code(
    Cronet_ExceptionPtr self,
    int32_t quic_detailed_error_code);
// Cronet_Exception getters.
CRONET_EXPORT
Cronet_Exception_ERROR_CODE Cronet_Exception_get_error_code(
    Cronet_ExceptionPtr self);
CRONET_EXPORT
int32_t Cronet_Exception_get_internal_error_code(Cronet_ExceptionPtr self);
CRONET_EXPORT
bool Cronet_Exception_get_immediately_retryable(Cronet_ExceptionPtr self);
CRONET_EXPORT
int32_t Cronet_Exception_get_quic_detailed_error_code(Cronet_ExceptionPtr self);

///////////////////////
// Struct Cronet_QuicHint.
CRONET_EXPORT Cronet_QuicHintPtr Cronet_QuicHint_Create();
CRONET_EXPORT void Cronet_QuicHint_Destroy(Cronet_QuicHintPtr self);
// Cronet_QuicHint setters.
CRONET_EXPORT
void Cronet_QuicHint_set_host(Cronet_QuicHintPtr self, CharString host);
CRONET_EXPORT
void Cronet_QuicHint_set_port(Cronet_QuicHintPtr self, int32_t port);
CRONET_EXPORT
void Cronet_QuicHint_set_alternatePort(Cronet_QuicHintPtr self,
                                       int32_t alternatePort);
// Cronet_QuicHint getters.
CRONET_EXPORT
CharString Cronet_QuicHint_get_host(Cronet_QuicHintPtr self);
CRONET_EXPORT
int32_t Cronet_QuicHint_get_port(Cronet_QuicHintPtr self);
CRONET_EXPORT
int32_t Cronet_QuicHint_get_alternatePort(Cronet_QuicHintPtr self);

///////////////////////
// Struct Cronet_PublicKeyPins.
CRONET_EXPORT Cronet_PublicKeyPinsPtr Cronet_PublicKeyPins_Create();
CRONET_EXPORT void Cronet_PublicKeyPins_Destroy(Cronet_PublicKeyPinsPtr self);
// Cronet_PublicKeyPins setters.
CRONET_EXPORT
void Cronet_PublicKeyPins_set_host(Cronet_PublicKeyPinsPtr self,
                                   CharString host);
CRONET_EXPORT
void Cronet_PublicKeyPins_add_pinsSha256(Cronet_PublicKeyPinsPtr self,
                                         RawDataPtr pinsSha256);
CRONET_EXPORT
void Cronet_PublicKeyPins_set_includeSubdomains(Cronet_PublicKeyPinsPtr self,
                                                bool includeSubdomains);
// Cronet_PublicKeyPins getters.
CRONET_EXPORT
CharString Cronet_PublicKeyPins_get_host(Cronet_PublicKeyPinsPtr self);
CRONET_EXPORT
uint32_t Cronet_PublicKeyPins_get_pinsSha256Size(Cronet_PublicKeyPinsPtr self);
RawDataPtr Cronet_PublicKeyPins_get_pinsSha256AtIndex(
    Cronet_PublicKeyPinsPtr self,
    uint32_t index);
CRONET_EXPORT
bool Cronet_PublicKeyPins_get_includeSubdomains(Cronet_PublicKeyPinsPtr self);

///////////////////////
// Struct Cronet_EngineParams.
CRONET_EXPORT Cronet_EngineParamsPtr Cronet_EngineParams_Create();
CRONET_EXPORT void Cronet_EngineParams_Destroy(Cronet_EngineParamsPtr self);
// Cronet_EngineParams setters.
CRONET_EXPORT
void Cronet_EngineParams_set_userAgent(Cronet_EngineParamsPtr self,
                                       CharString userAgent);
CRONET_EXPORT
void Cronet_EngineParams_set_storagePath(Cronet_EngineParamsPtr self,
                                         CharString storagePath);
CRONET_EXPORT
void Cronet_EngineParams_set_enableQuic(Cronet_EngineParamsPtr self,
                                        bool enableQuic);
CRONET_EXPORT
void Cronet_EngineParams_set_enableHttp2(Cronet_EngineParamsPtr self,
                                         bool enableHttp2);
CRONET_EXPORT
void Cronet_EngineParams_set_enableBrotli(Cronet_EngineParamsPtr self,
                                          bool enableBrotli);
CRONET_EXPORT
void Cronet_EngineParams_set_httpCacheMode(
    Cronet_EngineParamsPtr self,
    Cronet_EngineParams_HTTP_CACHE_MODE httpCacheMode);
CRONET_EXPORT
void Cronet_EngineParams_set_httpCacheMaxSize(Cronet_EngineParamsPtr self,
                                              int64_t httpCacheMaxSize);
CRONET_EXPORT
void Cronet_EngineParams_add_quicHints(Cronet_EngineParamsPtr self,
                                       Cronet_QuicHintPtr quicHints);
CRONET_EXPORT
void Cronet_EngineParams_add_publicKeyPins(
    Cronet_EngineParamsPtr self,
    Cronet_PublicKeyPinsPtr publicKeyPins);
CRONET_EXPORT
void Cronet_EngineParams_set_enablePublicKeyPinningBypassForLocalTrustAnchors(
    Cronet_EngineParamsPtr self,
    bool enablePublicKeyPinningBypassForLocalTrustAnchors);
// Cronet_EngineParams getters.
CRONET_EXPORT
CharString Cronet_EngineParams_get_userAgent(Cronet_EngineParamsPtr self);
CRONET_EXPORT
CharString Cronet_EngineParams_get_storagePath(Cronet_EngineParamsPtr self);
CRONET_EXPORT
bool Cronet_EngineParams_get_enableQuic(Cronet_EngineParamsPtr self);
CRONET_EXPORT
bool Cronet_EngineParams_get_enableHttp2(Cronet_EngineParamsPtr self);
CRONET_EXPORT
bool Cronet_EngineParams_get_enableBrotli(Cronet_EngineParamsPtr self);
CRONET_EXPORT
Cronet_EngineParams_HTTP_CACHE_MODE Cronet_EngineParams_get_httpCacheMode(
    Cronet_EngineParamsPtr self);
CRONET_EXPORT
int64_t Cronet_EngineParams_get_httpCacheMaxSize(Cronet_EngineParamsPtr self);
CRONET_EXPORT
uint32_t Cronet_EngineParams_get_quicHintsSize(Cronet_EngineParamsPtr self);
Cronet_QuicHintPtr Cronet_EngineParams_get_quicHintsAtIndex(
    Cronet_EngineParamsPtr self,
    uint32_t index);
CRONET_EXPORT
uint32_t Cronet_EngineParams_get_publicKeyPinsSize(Cronet_EngineParamsPtr self);
Cronet_PublicKeyPinsPtr Cronet_EngineParams_get_publicKeyPinsAtIndex(
    Cronet_EngineParamsPtr self,
    uint32_t index);
CRONET_EXPORT
bool Cronet_EngineParams_get_enablePublicKeyPinningBypassForLocalTrustAnchors(
    Cronet_EngineParamsPtr self);

///////////////////////
// Struct Cronet_HttpHeader.
CRONET_EXPORT Cronet_HttpHeaderPtr Cronet_HttpHeader_Create();
CRONET_EXPORT void Cronet_HttpHeader_Destroy(Cronet_HttpHeaderPtr self);
// Cronet_HttpHeader setters.
CRONET_EXPORT
void Cronet_HttpHeader_set_name(Cronet_HttpHeaderPtr self, CharString name);
CRONET_EXPORT
void Cronet_HttpHeader_set_value(Cronet_HttpHeaderPtr self, CharString value);
// Cronet_HttpHeader getters.
CRONET_EXPORT
CharString Cronet_HttpHeader_get_name(Cronet_HttpHeaderPtr self);
CRONET_EXPORT
CharString Cronet_HttpHeader_get_value(Cronet_HttpHeaderPtr self);

///////////////////////
// Struct Cronet_UrlResponseInfo.
CRONET_EXPORT Cronet_UrlResponseInfoPtr Cronet_UrlResponseInfo_Create();
CRONET_EXPORT void Cronet_UrlResponseInfo_Destroy(
    Cronet_UrlResponseInfoPtr self);
// Cronet_UrlResponseInfo setters.
CRONET_EXPORT
void Cronet_UrlResponseInfo_set_url(Cronet_UrlResponseInfoPtr self,
                                    CharString url);
CRONET_EXPORT
void Cronet_UrlResponseInfo_add_urlChain(Cronet_UrlResponseInfoPtr self,
                                         CharString urlChain);
CRONET_EXPORT
void Cronet_UrlResponseInfo_set_httpStatusCode(Cronet_UrlResponseInfoPtr self,
                                               int32_t httpStatusCode);
CRONET_EXPORT
void Cronet_UrlResponseInfo_set_httpStatusText(Cronet_UrlResponseInfoPtr self,
                                               CharString httpStatusText);
CRONET_EXPORT
void Cronet_UrlResponseInfo_add_allHeadersList(
    Cronet_UrlResponseInfoPtr self,
    Cronet_HttpHeaderPtr allHeadersList);
CRONET_EXPORT
void Cronet_UrlResponseInfo_set_wasCached(Cronet_UrlResponseInfoPtr self,
                                          bool wasCached);
CRONET_EXPORT
void Cronet_UrlResponseInfo_set_negotiatedProtocol(
    Cronet_UrlResponseInfoPtr self,
    CharString negotiatedProtocol);
CRONET_EXPORT
void Cronet_UrlResponseInfo_set_proxyServer(Cronet_UrlResponseInfoPtr self,
                                            CharString proxyServer);
CRONET_EXPORT
void Cronet_UrlResponseInfo_set_receivedByteCount(
    Cronet_UrlResponseInfoPtr self,
    int64_t receivedByteCount);
// Cronet_UrlResponseInfo getters.
CRONET_EXPORT
CharString Cronet_UrlResponseInfo_get_url(Cronet_UrlResponseInfoPtr self);
CRONET_EXPORT
uint32_t Cronet_UrlResponseInfo_get_urlChainSize(
    Cronet_UrlResponseInfoPtr self);
CharString Cronet_UrlResponseInfo_get_urlChainAtIndex(
    Cronet_UrlResponseInfoPtr self,
    uint32_t index);
CRONET_EXPORT
int32_t Cronet_UrlResponseInfo_get_httpStatusCode(
    Cronet_UrlResponseInfoPtr self);
CRONET_EXPORT
CharString Cronet_UrlResponseInfo_get_httpStatusText(
    Cronet_UrlResponseInfoPtr self);
CRONET_EXPORT
uint32_t Cronet_UrlResponseInfo_get_allHeadersListSize(
    Cronet_UrlResponseInfoPtr self);
Cronet_HttpHeaderPtr Cronet_UrlResponseInfo_get_allHeadersListAtIndex(
    Cronet_UrlResponseInfoPtr self,
    uint32_t index);
CRONET_EXPORT
bool Cronet_UrlResponseInfo_get_wasCached(Cronet_UrlResponseInfoPtr self);
CRONET_EXPORT
CharString Cronet_UrlResponseInfo_get_negotiatedProtocol(
    Cronet_UrlResponseInfoPtr self);
CRONET_EXPORT
CharString Cronet_UrlResponseInfo_get_proxyServer(
    Cronet_UrlResponseInfoPtr self);
CRONET_EXPORT
int64_t Cronet_UrlResponseInfo_get_receivedByteCount(
    Cronet_UrlResponseInfoPtr self);

///////////////////////
// Struct Cronet_UrlRequestParams.
CRONET_EXPORT Cronet_UrlRequestParamsPtr Cronet_UrlRequestParams_Create();
CRONET_EXPORT void Cronet_UrlRequestParams_Destroy(
    Cronet_UrlRequestParamsPtr self);
// Cronet_UrlRequestParams setters.
CRONET_EXPORT
void Cronet_UrlRequestParams_set_httpMethod(Cronet_UrlRequestParamsPtr self,
                                            CharString httpMethod);
CRONET_EXPORT
void Cronet_UrlRequestParams_add_requestHeaders(
    Cronet_UrlRequestParamsPtr self,
    Cronet_HttpHeaderPtr requestHeaders);
CRONET_EXPORT
void Cronet_UrlRequestParams_set_disableCache(Cronet_UrlRequestParamsPtr self,
                                              bool disableCache);
CRONET_EXPORT
void Cronet_UrlRequestParams_set_priority(
    Cronet_UrlRequestParamsPtr self,
    Cronet_UrlRequestParams_REQUEST_PRIORITY priority);
CRONET_EXPORT
void Cronet_UrlRequestParams_set_uploadDataProvider(
    Cronet_UrlRequestParamsPtr self,
    Cronet_UploadDataProviderPtr uploadDataProvider);
CRONET_EXPORT
void Cronet_UrlRequestParams_set_uploadDataProviderExecutor(
    Cronet_UrlRequestParamsPtr self,
    Cronet_ExecutorPtr uploadDataProviderExecutor);
CRONET_EXPORT
void Cronet_UrlRequestParams_set_allowDirectExecutor(
    Cronet_UrlRequestParamsPtr self,
    bool allowDirectExecutor);
CRONET_EXPORT
void Cronet_UrlRequestParams_add_annotations(Cronet_UrlRequestParamsPtr self,
                                             RawDataPtr annotations);
// Cronet_UrlRequestParams getters.
CRONET_EXPORT
CharString Cronet_UrlRequestParams_get_httpMethod(
    Cronet_UrlRequestParamsPtr self);
CRONET_EXPORT
uint32_t Cronet_UrlRequestParams_get_requestHeadersSize(
    Cronet_UrlRequestParamsPtr self);
Cronet_HttpHeaderPtr Cronet_UrlRequestParams_get_requestHeadersAtIndex(
    Cronet_UrlRequestParamsPtr self,
    uint32_t index);
CRONET_EXPORT
bool Cronet_UrlRequestParams_get_disableCache(Cronet_UrlRequestParamsPtr self);
CRONET_EXPORT
Cronet_UrlRequestParams_REQUEST_PRIORITY Cronet_UrlRequestParams_get_priority(
    Cronet_UrlRequestParamsPtr self);
CRONET_EXPORT
Cronet_UploadDataProviderPtr Cronet_UrlRequestParams_get_uploadDataProvider(
    Cronet_UrlRequestParamsPtr self);
CRONET_EXPORT
Cronet_ExecutorPtr Cronet_UrlRequestParams_get_uploadDataProviderExecutor(
    Cronet_UrlRequestParamsPtr self);
CRONET_EXPORT
bool Cronet_UrlRequestParams_get_allowDirectExecutor(
    Cronet_UrlRequestParamsPtr self);
CRONET_EXPORT
uint32_t Cronet_UrlRequestParams_get_annotationsSize(
    Cronet_UrlRequestParamsPtr self);
RawDataPtr Cronet_UrlRequestParams_get_annotationsAtIndex(
    Cronet_UrlRequestParamsPtr self,
    uint32_t index);

#ifdef __cplusplus
}
#endif

#endif  // COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_C_H_
