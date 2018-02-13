// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mfapi.h>
#include <mferror.h>
#include <stddef.h>
#include <wincodec.h>

#include "media/capture/video/win/video_capture_device_factory_win.h"
#include "media/capture/video/win/video_capture_device_mf_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::Mock;
using Microsoft::WRL::ComPtr;

namespace media {

namespace {
class MockClient : public VideoCaptureDevice::Client {
 public:
  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              const VideoCaptureFormat& frame_format,
                              int clockwise_rotation,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp,
                              int frame_feedback_id = 0) override {}

  MOCK_METHOD4(
      ReserveOutputBuffer,
      Buffer(const gfx::Size&, VideoPixelFormat, VideoPixelStorage, int));

  void OnIncomingCapturedBuffer(Buffer buffer,
                                const VideoCaptureFormat& format,
                                base::TimeTicks reference_,
                                base::TimeDelta timestamp) override {}

  void OnIncomingCapturedBufferExt(
      Buffer buffer,
      const VideoCaptureFormat& format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      gfx::Rect visible_rect,
      const VideoFrameMetadata& additional_metadata) override {}

  MOCK_METHOD4(
      ResurrectLastOutputBuffer,
      Buffer(const gfx::Size&, VideoPixelFormat, VideoPixelStorage, int));

  MOCK_METHOD2(OnError, void(const base::Location&, const std::string&));

  double GetBufferPoolUtilization() const override { return 0.0; }

  MOCK_METHOD0(OnStarted, void());
};

class MockImageCaptureClient
    : public base::RefCountedThreadSafe<MockImageCaptureClient> {
 public:
  // GMock doesn't support move-only arguments, so we use this forward method.
  void DoOnGetPhotoState(mojom::PhotoStatePtr received_state) {
    state = std::move(received_state);
  }

  MOCK_METHOD1(OnCorrectSetPhotoOptions, void(bool));

  // GMock doesn't support move-only arguments, so we use this forward method.
  void DoOnPhotoTaken(mojom::BlobPtr blob) {
    EXPECT_TRUE(blob);
    OnCorrectPhotoTaken();
  }
  MOCK_METHOD0(OnCorrectPhotoTaken, void(void));

  mojom::PhotoStatePtr state;

 private:
  friend class base::RefCountedThreadSafe<MockImageCaptureClient>;
  virtual ~MockImageCaptureClient() = default;
};

class MockMFMediaSource : public base::RefCountedThreadSafe<MockMFMediaSource>,
                          public IMFMediaSource {
 public:
  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override {
    return E_NOTIMPL;
  }
  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MockMFMediaSource>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MockMFMediaSource>::Release();
    return 1U;
  }
  STDMETHOD(GetEvent)(DWORD dwFlags, IMFMediaEvent** ppEvent) override {
    return E_NOTIMPL;
  }
  STDMETHOD(BeginGetEvent)
  (IMFAsyncCallback* pCallback, IUnknown* punkState) override {
    return E_NOTIMPL;
  }
  STDMETHOD(EndGetEvent)
  (IMFAsyncResult* pResult, IMFMediaEvent** ppEvent) override {
    return E_NOTIMPL;
  }
  STDMETHOD(QueueEvent)
  (MediaEventType met,
   REFGUID guidExtendedType,
   HRESULT hrStatus,
   const PROPVARIANT* pvValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetCharacteristics)(DWORD* pdwCharacteristics) override {
    return E_NOTIMPL;
  }
  STDMETHOD(CreatePresentationDescriptor)
  (IMFPresentationDescriptor** ppPresentationDescriptor) override {
    return E_NOTIMPL;
  }
  STDMETHOD(Start)
  (IMFPresentationDescriptor* pPresentationDescriptor,
   const GUID* pguidTimeFormat,
   const PROPVARIANT* pvarStartPosition) override {
    return E_NOTIMPL;
  }
  STDMETHOD(Stop)(void) override { return E_NOTIMPL; }
  STDMETHOD(Pause)(void) override { return E_NOTIMPL; }
  STDMETHOD(Shutdown)(void) override { return E_NOTIMPL; }

 private:
  friend class base::RefCountedThreadSafe<MockMFMediaSource>;
  virtual ~MockMFMediaSource() = default;
};

class MockMFCaptureSource
    : public base::RefCountedThreadSafe<MockMFCaptureSource>,
      public IMFCaptureSource {
 public:
  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override {
    return E_NOTIMPL;
  }
  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MockMFCaptureSource>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MockMFCaptureSource>::Release();
    return 1U;
  }
  STDMETHOD(GetCaptureDeviceSource)
  (MF_CAPTURE_ENGINE_DEVICE_TYPE mfCaptureEngineDeviceType,
   IMFMediaSource** ppMediaSource) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetCaptureDeviceActivate)
  (MF_CAPTURE_ENGINE_DEVICE_TYPE mfCaptureEngineDeviceType,
   IMFActivate** ppActivate) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetService)
  (REFIID rguidService, REFIID riid, IUnknown** ppUnknown) override {
    return E_NOTIMPL;
  }
  STDMETHOD(AddEffect)(DWORD dwSourceStreamIndex, IUnknown* pUnknown) override {
    return E_NOTIMPL;
  }
  STDMETHOD(RemoveEffect)
  (DWORD dwSourceStreamIndex, IUnknown* pUnknown) override { return E_NOTIMPL; }
  STDMETHOD(RemoveAllEffects)(DWORD dwSourceStreamIndex) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetAvailableDeviceMediaType)
  (DWORD stream_index,
   DWORD media_type_index,
   IMFMediaType** media_type) override {
    return DoGetAvailableDeviceMediaType(stream_index, media_type_index,
                                         media_type);
  }

  MOCK_METHOD3(DoGetAvailableDeviceMediaType,
               HRESULT(DWORD, DWORD, IMFMediaType**));

  STDMETHOD(SetCurrentDeviceMediaType)
  (DWORD dwSourceStreamIndex, IMFMediaType* pMediaType) override {
    return S_OK;
  }

  STDMETHOD(GetCurrentDeviceMediaType)
  (DWORD stream_index, IMFMediaType** media_type) {
    return DoGetCurrentDeviceMediaType(stream_index, media_type);
  }
  MOCK_METHOD2(DoGetCurrentDeviceMediaType, HRESULT(DWORD, IMFMediaType**));

  STDMETHOD(GetDeviceStreamCount)(DWORD* count) {
    return DoGetDeviceStreamCount(count);
  }
  MOCK_METHOD1(DoGetDeviceStreamCount, HRESULT(DWORD*));

  STDMETHOD(GetDeviceStreamCategory)
  (DWORD stream_index, MF_CAPTURE_ENGINE_STREAM_CATEGORY* category) {
    return DoGetDeviceStreamCategory(stream_index, category);
  }
  MOCK_METHOD2(DoGetDeviceStreamCategory,
               HRESULT(DWORD, MF_CAPTURE_ENGINE_STREAM_CATEGORY*));

  STDMETHOD(GetMirrorState)(DWORD dwStreamIndex, BOOL* pfMirrorState) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetMirrorState)(DWORD dwStreamIndex, BOOL fMirrorState) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetStreamIndexFromFriendlyName)
  (UINT32 uifriendlyName, DWORD* pdwActualStreamIndex) override {
    return E_NOTIMPL;
  }

 private:
  friend class base::RefCountedThreadSafe<MockMFCaptureSource>;
  virtual ~MockMFCaptureSource() = default;
};

class MockCapturePreviewSink
    : public base::RefCountedThreadSafe<MockCapturePreviewSink>,
      public IMFCapturePreviewSink {
 public:
  STDMETHOD(QueryInterface)(REFIID riid, void** object) override {
    if (riid == IID_IUnknown || riid == IID_IMFCapturePreviewSink) {
      AddRef();
      *object = this;
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MockCapturePreviewSink>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MockCapturePreviewSink>::Release();
    return 1U;
  }
  STDMETHOD(GetOutputMediaType)
  (DWORD dwSinkStreamIndex, IMFMediaType** ppMediaType) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetService)
  (DWORD dwSinkStreamIndex,
   REFGUID rguidService,
   REFIID riid,
   IUnknown** ppUnknown) override {
    return E_NOTIMPL;
  }
  STDMETHOD(AddStream)
  (DWORD dwSourceStreamIndex,
   IMFMediaType* pMediaType,
   IMFAttributes* pAttributes,
   DWORD* pdwSinkStreamIndex) override {
    return S_OK;
  }
  STDMETHOD(Prepare)(void) override { return E_NOTIMPL; }
  STDMETHOD(RemoveAllStreams)(void) override { return S_OK; }
  STDMETHOD(SetRenderHandle)(HANDLE handle) override { return E_NOTIMPL; }
  STDMETHOD(SetRenderSurface)(IUnknown* pSurface) override { return E_NOTIMPL; }
  STDMETHOD(UpdateVideo)
  (const MFVideoNormalizedRect* pSrc,
   const RECT* pDst,
   const COLORREF* pBorderClr) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetSampleCallback)
  (DWORD dwStreamSinkIndex,
   IMFCaptureEngineOnSampleCallback* pCallback) override {
    sample_callback = pCallback;
    return S_OK;
  }
  STDMETHOD(GetMirrorState)(BOOL* pfMirrorState) override { return E_NOTIMPL; }
  STDMETHOD(SetMirrorState)(BOOL fMirrorState) override { return E_NOTIMPL; }
  STDMETHOD(GetRotation)
  (DWORD dwStreamIndex, DWORD* pdwRotationValue) override { return E_NOTIMPL; }
  STDMETHOD(SetRotation)(DWORD dwStreamIndex, DWORD dwRotationValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetCustomSink)(IMFMediaSink* pMediaSink) override {
    return E_NOTIMPL;
  }

  scoped_refptr<IMFCaptureEngineOnSampleCallback> sample_callback;

 private:
  friend class base::RefCountedThreadSafe<MockCapturePreviewSink>;
  virtual ~MockCapturePreviewSink() = default;
};

class MockCapturePhotoSink
    : public base::RefCountedThreadSafe<MockCapturePhotoSink>,
      public IMFCapturePhotoSink {
 public:
  STDMETHOD(QueryInterface)(REFIID riid, void** object) override {
    if (riid == IID_IUnknown || riid == IID_IMFCapturePhotoSink) {
      AddRef();
      *object = this;
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MockCapturePhotoSink>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MockCapturePhotoSink>::Release();
    return 1U;
  }
  STDMETHOD(GetOutputMediaType)
  (DWORD dwSinkStreamIndex, IMFMediaType** ppMediaType) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetService)
  (DWORD dwSinkStreamIndex,
   REFGUID rguidService,
   REFIID riid,
   IUnknown** ppUnknown) override {
    return E_NOTIMPL;
  }
  STDMETHOD(AddStream)
  (DWORD dwSourceStreamIndex,
   IMFMediaType* pMediaType,
   IMFAttributes* pAttributes,
   DWORD* pdwSinkStreamIndex) override {
    return S_OK;
  }
  STDMETHOD(Prepare)(void) override { return E_NOTIMPL; }
  STDMETHOD(RemoveAllStreams)(void) override { return S_OK; }

  STDMETHOD(SetOutputFileName)(LPCWSTR fileName) override { return E_NOTIMPL; }
  STDMETHOD(SetSampleCallback)
  (IMFCaptureEngineOnSampleCallback* pCallback) override {
    sample_callback = pCallback;
    return S_OK;
  }
  STDMETHOD(SetOutputByteStream)(IMFByteStream* pByteStream) override {
    return E_NOTIMPL;
  }

  scoped_refptr<IMFCaptureEngineOnSampleCallback> sample_callback;

 private:
  friend class base::RefCountedThreadSafe<MockCapturePhotoSink>;
  virtual ~MockCapturePhotoSink() = default;
};

class MockMFCaptureEngine
    : public base::RefCountedThreadSafe<MockMFCaptureEngine>,
      public IMFCaptureEngine {
 public:
  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) { return S_OK; }
  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MockMFCaptureEngine>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MockMFCaptureEngine>::Release();
    return 1U;
  }
  STDMETHOD(Initialize)
  (IMFCaptureEngineOnEventCallback* pEventCallback,
   IMFAttributes* pAttributes,
   IUnknown* pAudioSource,
   IUnknown* pVideoSource) override {
    EXPECT_TRUE(pEventCallback);
    EXPECT_TRUE(pAttributes);
    EXPECT_TRUE(pVideoSource);
    event_callback = pEventCallback;
    OnCorrectInitialize();
    return S_OK;
  }

  MOCK_METHOD0(OnCorrectInitialize, void(void));

  STDMETHOD(StartPreview)(void) override {
    OnStartPreview();
    return S_OK;
  }

  MOCK_METHOD0(OnStartPreview, void(void));

  STDMETHOD(StopPreview)(void) override {
    OnStopPreview();
    return S_OK;
  }

  MOCK_METHOD0(OnStopPreview, void(void));

  STDMETHOD(StartRecord)(void) override { return E_NOTIMPL; }
  STDMETHOD(StopRecord)(BOOL bFinalize, BOOL bFlushUnprocessedSamples) {
    return E_NOTIMPL;
  }
  STDMETHOD(TakePhoto)(void) override {
    OnTakePhoto();
    return S_OK;
  }
  MOCK_METHOD0(OnTakePhoto, void(void));

  STDMETHOD(GetSink)(MF_CAPTURE_ENGINE_SINK_TYPE type, IMFCaptureSink** sink) {
    return DoGetSink(type, sink);
  }
  MOCK_METHOD2(DoGetSink,
               HRESULT(MF_CAPTURE_ENGINE_SINK_TYPE, IMFCaptureSink**));

  STDMETHOD(GetSource)(IMFCaptureSource** source) {
    *source = DoGetSource();
    return source ? S_OK : E_FAIL;
  }
  MOCK_METHOD0(DoGetSource, IMFCaptureSource*());

  scoped_refptr<IMFCaptureEngineOnEventCallback> event_callback;

 private:
  friend class base::RefCountedThreadSafe<MockMFCaptureEngine>;
  virtual ~MockMFCaptureEngine() = default;
};

class StubMFMediaType : public base::RefCountedThreadSafe<StubMFMediaType>,
                        public IMFMediaType {
 public:
  StubMFMediaType(GUID major_type,
                  GUID sub_type,
                  int frame_width,
                  int frame_height,
                  int frame_rate)
      : major_type_(major_type),
        sub_type_(sub_type),
        frame_width_(frame_width),
        frame_height_(frame_height),
        frame_rate_(frame_rate) {}

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override {
    return E_NOTIMPL;
  }
  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<StubMFMediaType>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<StubMFMediaType>::Release();
    return 1U;
  }
  STDMETHOD(GetItem)(REFGUID key, PROPVARIANT* value) override {
    if (key == MF_MT_FRAME_SIZE) {
      value->vt = VT_UI8;
      value->uhVal.QuadPart = Pack2UINT32AsUINT64(frame_width_, frame_height_);
      return S_OK;
    }
    if (key == MF_MT_FRAME_RATE) {
      value->vt = VT_UI8;
      value->uhVal.QuadPart = Pack2UINT32AsUINT64(frame_rate_, 1);
      return S_OK;
    }
    if (key == MF_MT_PIXEL_ASPECT_RATIO) {
      value->vt = VT_UI8;
      value->uhVal.QuadPart = Pack2UINT32AsUINT64(1, 1);
      return S_OK;
    }
    if (key == MF_MT_INTERLACE_MODE) {
      value->vt = VT_UI4;
      value->uintVal = MFVideoInterlace_Progressive;
      return S_OK;
    }
    return E_FAIL;
  }
  STDMETHOD(GetItemType)(REFGUID guidKey, MF_ATTRIBUTE_TYPE* pType) override {
    return E_NOTIMPL;
  }
  STDMETHOD(CompareItem)
  (REFGUID guidKey, REFPROPVARIANT Value, BOOL* pbResult) override {
    return E_NOTIMPL;
  }
  STDMETHOD(Compare)
  (IMFAttributes* pTheirs,
   MF_ATTRIBUTES_MATCH_TYPE MatchType,
   BOOL* pbResult) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetUINT32)(REFGUID key, UINT32* value) override {
    if (key == MF_MT_INTERLACE_MODE) {
      *value = MFVideoInterlace_Progressive;
      return S_OK;
    }
    return E_NOTIMPL;
  }
  STDMETHOD(GetUINT64)(REFGUID key, UINT64* value) override {
    if (key == MF_MT_FRAME_SIZE) {
      *value = (long long)frame_width_ << 32 | frame_height_;
      return S_OK;
    }
    if (key == MF_MT_FRAME_RATE) {
      *value = (long long)frame_rate_ << 32 | 1;
      return S_OK;
    }
    if (key == MF_MT_PIXEL_ASPECT_RATIO) {
      *value = (long long)1 << 32 | 1;
      return S_OK;
    }
    return E_FAIL;
  }
  STDMETHOD(GetDouble)(REFGUID guidKey, double* pfValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetGUID)(REFGUID key, GUID* value) override {
    if (key == MF_MT_MAJOR_TYPE) {
      *value = major_type_;
      return S_OK;
    }
    if (key == MF_MT_SUBTYPE) {
      *value = sub_type_;
      return S_OK;
    }
    return E_FAIL;
  }
  STDMETHOD(GetStringLength)(REFGUID guidKey, UINT32* pcchLength) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetString)
  (REFGUID guidKey,
   LPWSTR pwszValue,
   UINT32 cchBufSize,
   UINT32* pcchLength) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetAllocatedString)
  (REFGUID guidKey, LPWSTR* ppwszValue, UINT32* pcchLength) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetBlobSize)(REFGUID guidKey, UINT32* pcbBlobSize) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetBlob)
  (REFGUID guidKey,
   UINT8* pBuf,
   UINT32 cbBufSize,
   UINT32* pcbBlobSize) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetAllocatedBlob)
  (REFGUID guidKey, UINT8** ppBuf, UINT32* pcbSize) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetUnknown)(REFGUID guidKey, REFIID riid, LPVOID* ppv) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetItem)(REFGUID guidKey, REFPROPVARIANT Value) override {
    return E_NOTIMPL;
  }
  STDMETHOD(DeleteItem)(REFGUID guidKey) override { return E_NOTIMPL; }
  STDMETHOD(DeleteAllItems)(void) override { return E_NOTIMPL; }
  STDMETHOD(SetUINT32)(REFGUID guidKey, UINT32 unValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetUINT64)(REFGUID guidKey, UINT64 unValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetDouble)(REFGUID guidKey, double fValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetGUID)(REFGUID guidKey, REFGUID guidValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetString)(REFGUID guidKey, LPCWSTR wszValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetBlob)
  (REFGUID guidKey, const UINT8* pBuf, UINT32 cbBufSize) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetUnknown)(REFGUID guidKey, IUnknown* pUnknown) override {
    return E_NOTIMPL;
  }
  STDMETHOD(LockStore)(void) override { return E_NOTIMPL; }
  STDMETHOD(UnlockStore)(void) override { return E_NOTIMPL; }
  STDMETHOD(GetCount)(UINT32* pcItems) override { return E_NOTIMPL; }
  STDMETHOD(GetItemByIndex)
  (UINT32 unIndex, GUID* pguidKey, PROPVARIANT* pValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(CopyAllItems)(IMFAttributes* pDest) override { return E_NOTIMPL; }
  STDMETHOD(GetMajorType)(GUID* pguidMajorType) override { return E_NOTIMPL; }
  STDMETHOD(IsCompressedFormat)(BOOL* pfCompressed) override {
    return E_NOTIMPL;
  }
  STDMETHOD(IsEqual)(IMFMediaType* pIMediaType, DWORD* pdwFlags) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetRepresentation)
  (GUID guidRepresentation, LPVOID* ppvRepresentation) override {
    return E_NOTIMPL;
  }
  STDMETHOD(FreeRepresentation)
  (GUID guidRepresentation, LPVOID pvRepresentation) override {
    return E_NOTIMPL;
  }

 private:
  friend class base::RefCountedThreadSafe<StubMFMediaType>;
  virtual ~StubMFMediaType() = default;

  const GUID major_type_;
  const GUID sub_type_;
  const int frame_width_;
  const int frame_height_;
  const int frame_rate_;
};

class MockMFMediaEvent : public base::RefCountedThreadSafe<MockMFMediaEvent>,
                         public IMFMediaEvent {
 public:
  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override {
    return E_NOTIMPL;
  }

  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MockMFMediaEvent>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MockMFMediaEvent>::Release();
    return 1U;
  }

  STDMETHOD(GetItem)(REFGUID guidKey, PROPVARIANT* pValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetItemType)(REFGUID guidKey, MF_ATTRIBUTE_TYPE* pType) override {
    return E_NOTIMPL;
  }

  STDMETHOD(CompareItem)
  (REFGUID guidKey, REFPROPVARIANT Value, BOOL* pbResult) override {
    return E_NOTIMPL;
  }

  STDMETHOD(Compare)
  (IMFAttributes* pTheirs,
   MF_ATTRIBUTES_MATCH_TYPE MatchType,
   BOOL* pbResult) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetUINT32)(REFGUID guidKey, UINT32* punValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetUINT64)(REFGUID guidKey, UINT64* punValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetDouble)(REFGUID guidKey, double* pfValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetGUID)(REFGUID guidKey, GUID* pguidValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetStringLength)(REFGUID guidKey, UINT32* pcchLength) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetString)
  (REFGUID guidKey,
   LPWSTR pwszValue,
   UINT32 cchBufSize,
   UINT32* pcchLength) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetAllocatedString)
  (REFGUID guidKey, LPWSTR* ppwszValue, UINT32* pcchLength) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetBlobSize)(REFGUID guidKey, UINT32* pcbBlobSize) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetBlob)
  (REFGUID guidKey,
   UINT8* pBuf,
   UINT32 cbBufSize,
   UINT32* pcbBlobSize) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetAllocatedBlob)
  (REFGUID guidKey, UINT8** ppBuf, UINT32* pcbSize) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetUnknown)(REFGUID guidKey, REFIID riid, LPVOID* ppv) override {
    return E_NOTIMPL;
  }

  STDMETHOD(SetItem)(REFGUID guidKey, REFPROPVARIANT Value) override {
    return E_NOTIMPL;
  }

  STDMETHOD(DeleteItem)(REFGUID guidKey) override { return E_NOTIMPL; }

  STDMETHOD(DeleteAllItems)(void) override { return E_NOTIMPL; }

  STDMETHOD(SetUINT32)(REFGUID guidKey, UINT32 unValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(SetUINT64)(REFGUID guidKey, UINT64 unValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(SetDouble)(REFGUID guidKey, double fValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(SetGUID)(REFGUID guidKey, REFGUID guidValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(SetString)(REFGUID guidKey, LPCWSTR wszValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(SetBlob)
  (REFGUID guidKey, const UINT8* pBuf, UINT32 cbBufSize) override {
    return E_NOTIMPL;
  }

  STDMETHOD(SetUnknown)(REFGUID guidKey, IUnknown* pUnknown) override {
    return E_NOTIMPL;
  }

  STDMETHOD(LockStore)(void) override { return E_NOTIMPL; }

  STDMETHOD(UnlockStore)(void) override { return E_NOTIMPL; }

  STDMETHOD(GetCount)(UINT32* pcItems) override { return E_NOTIMPL; }

  STDMETHOD(GetItemByIndex)
  (UINT32 unIndex, GUID* pguidKey, PROPVARIANT* pValue) override {
    return E_NOTIMPL;
  }

  STDMETHOD(CopyAllItems)(IMFAttributes* pDest) override { return E_NOTIMPL; }

  STDMETHOD(GetType)(MediaEventType* pmet) override { return E_NOTIMPL; }

  STDMETHOD(GetExtendedType)(GUID* pguidExtendedType) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetStatus)(HRESULT* status) override {
    *status = DoGetStatus();
    return S_OK;
  }
  MOCK_METHOD0(DoGetStatus, HRESULT());

  STDMETHOD(GetValue)(PROPVARIANT* pvValue) override { return E_NOTIMPL; }

 private:
  friend class base::RefCountedThreadSafe<MockMFMediaEvent>;
  virtual ~MockMFMediaEvent() = default;
};

}  // namespace

const int kArbitraryValidVideoWidth = 1920;
const int kArbitraryValidVideoHeight = 1080;

const int kArbitraryValidPhotoWidth = 3264;
const int kArbitraryValidPhotoHeight = 2448;

class VideoCaptureDeviceMFWinTest : public ::testing::Test {
 protected:
  VideoCaptureDeviceMFWinTest()
      : media_source_(new MockMFMediaSource()),
        engine_(new MockMFCaptureEngine()),
        client_(new MockClient()),
        image_capture_client_(new MockImageCaptureClient()),
        device_(new VideoCaptureDeviceMFWin(media_source_, engine_)),
        capture_source_(new MockMFCaptureSource()),
        media_foundation_supported_(
            VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation()) {}

  void SetUp() override {
    if (!media_foundation_supported_)
      return;
    device_->set_max_retry_count_for_testing(3);
    device_->set_retry_delay_in_ms_for_testing(1);

    EXPECT_CALL(*(engine_.Get()), OnCorrectInitialize());
    EXPECT_TRUE(device_->Init());
    EXPECT_CALL(*(engine_.Get()), DoGetSource())
        .WillRepeatedly(Invoke([this]() {
          this->capture_source_->AddRef();
          return this->capture_source_.get();
        }));
  }

  bool ShouldSkipTest() {
    if (media_foundation_supported_)
      return false;
    DVLOG(1) << "Media foundation is not supported by the current platform. "
                "Skipping test.";
    return true;
  }

  void PrepareMFDeviceWithOneVideoStream() {
    EXPECT_CALL(*capture_source_, DoGetDeviceStreamCount(_))
        .WillRepeatedly(Invoke([](DWORD* stream_count) {
          *stream_count = 1;
          return S_OK;
        }));
    EXPECT_CALL(*capture_source_, DoGetDeviceStreamCategory(0, _))
        .WillRepeatedly(Invoke([](DWORD stream_index,
                                  MF_CAPTURE_ENGINE_STREAM_CATEGORY* category) {
          *category = MF_CAPTURE_ENGINE_STREAM_CATEGORY_VIDEO_PREVIEW;
          return S_OK;
        }));

    EXPECT_CALL(*capture_source_, DoGetAvailableDeviceMediaType(0, _, _))
        .WillRepeatedly(Invoke([](DWORD stream_index, DWORD media_type_index,
                                  IMFMediaType** media_type) {
          if (media_type_index != 0)
            return MF_E_NO_MORE_TYPES;

          *media_type = new StubMFMediaType(
              MFMediaType_Video, MFVideoFormat_MJPG, kArbitraryValidVideoWidth,
              kArbitraryValidVideoHeight, 30);
          (*media_type)->AddRef();

          return S_OK;
        }));

    EXPECT_CALL(*(engine_.Get()),
                DoGetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PREVIEW, _))
        .WillRepeatedly(Invoke(
            [](MF_CAPTURE_ENGINE_SINK_TYPE sink_type, IMFCaptureSink** sink) {
              *sink = new MockCapturePreviewSink();
              (*sink)->AddRef();
              return S_OK;
            }));

    EXPECT_CALL(*capture_source_, DoGetCurrentDeviceMediaType(_, _))
        .WillRepeatedly(
            Invoke([](DWORD stream_index, IMFMediaType** media_type) {
              *media_type = new StubMFMediaType(
                  MFMediaType_Video, MFVideoFormat_MJPG,
                  kArbitraryValidVideoWidth, kArbitraryValidVideoHeight, 30);
              (*media_type)->AddRef();
              return S_OK;
            }));
  }

  void PrepareMFDeviceWithOneVideoStreamAndOnePhotoStream() {
    EXPECT_CALL(*capture_source_, DoGetDeviceStreamCount(_))
        .WillRepeatedly(Invoke([](DWORD* stream_count) {
          *stream_count = 2;
          return S_OK;
        }));
    EXPECT_CALL(*capture_source_, DoGetDeviceStreamCategory(_, _))
        .WillRepeatedly(Invoke([](DWORD stream_index,
                                  MF_CAPTURE_ENGINE_STREAM_CATEGORY* category) {
          if (stream_index == 0) {
            *category = MF_CAPTURE_ENGINE_STREAM_CATEGORY_VIDEO_PREVIEW;
            return S_OK;
          } else if (stream_index == 1) {
            *category = MF_CAPTURE_ENGINE_STREAM_CATEGORY_PHOTO_INDEPENDENT;
            return S_OK;
          }
          return E_FAIL;
        }));

    auto get_device_media_type = [](DWORD stream_index,
                                    IMFMediaType** media_type) {
      if (stream_index == 0) {
        *media_type = new StubMFMediaType(MFMediaType_Video, MFVideoFormat_MJPG,
                                          kArbitraryValidVideoWidth,
                                          kArbitraryValidVideoHeight, 30);
        (*media_type)->AddRef();
        return S_OK;
      } else if (stream_index == 1) {
        *media_type = new StubMFMediaType(
            MFMediaType_Image, GUID_ContainerFormatJpeg,
            kArbitraryValidPhotoWidth, kArbitraryValidPhotoHeight, 0);
        (*media_type)->AddRef();
        return S_OK;
      }
      return E_FAIL;
    };

    EXPECT_CALL(*capture_source_, DoGetAvailableDeviceMediaType(_, _, _))
        .WillRepeatedly(Invoke(
            [get_device_media_type](DWORD stream_index, DWORD media_type_index,
                                    IMFMediaType** media_type) {
              if (media_type_index != 0)
                return MF_E_NO_MORE_TYPES;
              return get_device_media_type(stream_index, media_type);
            }));

    EXPECT_CALL(*(engine_.Get()), DoGetSink(_, _))
        .WillRepeatedly(Invoke(
            [](MF_CAPTURE_ENGINE_SINK_TYPE sink_type, IMFCaptureSink** sink) {
              if (sink_type == MF_CAPTURE_ENGINE_SINK_TYPE_PREVIEW) {
                *sink = new MockCapturePreviewSink();
                (*sink)->AddRef();
                return S_OK;
              } else if (sink_type == MF_CAPTURE_ENGINE_SINK_TYPE_PHOTO) {
                *sink = new MockCapturePhotoSink();
                (*sink)->AddRef();
                return S_OK;
              }
              return E_FAIL;
            }));

    EXPECT_CALL(*capture_source_, DoGetCurrentDeviceMediaType(_, _))
        .WillRepeatedly(Invoke(get_device_media_type));
  }

  Microsoft::WRL::ComPtr<MockMFMediaSource> media_source_;
  Microsoft::WRL::ComPtr<MockMFCaptureEngine> engine_;
  std::unique_ptr<MockClient> client_;
  scoped_refptr<MockImageCaptureClient> image_capture_client_;
  std::unique_ptr<VideoCaptureDeviceMFWin> device_;
  VideoCaptureFormat last_format_;

  scoped_refptr<MockMFCaptureSource> capture_source_;

 private:
  const bool media_foundation_supported_;
};

// Expects StartPreview() to be called on AllocateAndStart()
TEST_F(VideoCaptureDeviceMFWinTest, StartPreviewOnAllocateAndStart) {
  if (ShouldSkipTest())
    return;

  PrepareMFDeviceWithOneVideoStream();

  EXPECT_CALL(*(engine_.Get()), OnStartPreview());
  EXPECT_CALL(*client_, OnStarted());
  EXPECT_CALL(*(engine_.Get()), OnStopPreview());

  device_->AllocateAndStart(VideoCaptureParams(), std::move(client_));

  device_->StopAndDeAllocate();
}

// Expects OnError() to be called on an errored IMFMediaEvent
TEST_F(VideoCaptureDeviceMFWinTest, CallClientOnErrorMediaEvent) {
  if (ShouldSkipTest())
    return;

  PrepareMFDeviceWithOneVideoStream();

  EXPECT_CALL(*(engine_.Get()), OnStartPreview());
  EXPECT_CALL(*client_, OnStarted());
  EXPECT_CALL(*client_, OnError(_, _));
  scoped_refptr<MockMFMediaEvent> media_event_error = new MockMFMediaEvent();
  EXPECT_CALL(*media_event_error, DoGetStatus()).WillRepeatedly(Return(E_FAIL));

  device_->AllocateAndStart(VideoCaptureParams(), std::move(client_));
  engine_->event_callback->OnEvent(media_event_error.get());
}

// Allocates device with flaky methods failing with MF_E_INVALIDREQUEST and
// expects the device to retry and start correctly
TEST_F(VideoCaptureDeviceMFWinTest, AllocateAndStartWithFlakyInvalidRequest) {
  if (ShouldSkipTest())
    return;

  EXPECT_CALL(*capture_source_, DoGetDeviceStreamCount(_))
      .Times(AtLeast(2))
      .WillOnce(Return(MF_E_INVALIDREQUEST))
      .WillRepeatedly(Invoke([](DWORD* stream_count) {
        *stream_count = 1;
        return S_OK;
      }));
  EXPECT_CALL(*capture_source_, DoGetDeviceStreamCategory(0, _))
      .Times(AtLeast(2))
      .WillOnce(Return(MF_E_INVALIDREQUEST))
      .WillRepeatedly(Invoke(
          [](DWORD stream_index, MF_CAPTURE_ENGINE_STREAM_CATEGORY* category) {
            *category = MF_CAPTURE_ENGINE_STREAM_CATEGORY_VIDEO_PREVIEW;
            return S_OK;
          }));

  EXPECT_CALL(*capture_source_, DoGetAvailableDeviceMediaType(0, _, _))
      .Times(AtLeast(2))
      .WillOnce(Return(MF_E_INVALIDREQUEST))
      .WillRepeatedly(Invoke([](DWORD stream_index, DWORD media_type_index,
                                IMFMediaType** media_type) {
        if (media_type_index != 0)
          return MF_E_NO_MORE_TYPES;

        *media_type = new StubMFMediaType(MFMediaType_Video, MFVideoFormat_MJPG,
                                          kArbitraryValidVideoWidth,
                                          kArbitraryValidVideoHeight, 30);
        (*media_type)->AddRef();

        return S_OK;
      }));

  EXPECT_CALL(*(engine_.Get()),
              DoGetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PREVIEW, _))
      .WillRepeatedly(Invoke(
          [](MF_CAPTURE_ENGINE_SINK_TYPE sink_type, IMFCaptureSink** sink) {
            *sink = new MockCapturePreviewSink();
            (*sink)->AddRef();
            return S_OK;
          }));

  EXPECT_CALL(*(engine_.Get()), OnStartPreview());
  EXPECT_CALL(*client_, OnStarted());
  device_->AllocateAndStart(VideoCaptureParams(), std::move(client_));
}

// Allocates device with methods always failing with MF_E_INVALIDREQUEST and
// expects the device to give up and call OnError()
TEST_F(VideoCaptureDeviceMFWinTest, AllocateAndStartWithFailingInvalidRequest) {
  if (ShouldSkipTest())
    return;

  EXPECT_CALL(*capture_source_, DoGetDeviceStreamCount(_))
      .WillRepeatedly(Return(MF_E_INVALIDREQUEST));

  EXPECT_CALL(*client_, OnError(_, _));
  device_->AllocateAndStart(VideoCaptureParams(), std::move(client_));
}

// Given an |IMFCaptureSource| offering a video stream without photo stream to
// |VideoCaptureDevice|, when asking the photo state from |VideoCaptureDevice|
// then expect the returned state to match the video resolution
TEST_F(VideoCaptureDeviceMFWinTest, GetPhotoStateViaVideoStream) {
  if (ShouldSkipTest())
    return;

  PrepareMFDeviceWithOneVideoStream();

  EXPECT_CALL(*(engine_.Get()), OnStartPreview());
  EXPECT_CALL(*client_, OnStarted());

  device_->AllocateAndStart(VideoCaptureParams(), std::move(client_));

  VideoCaptureDevice::GetPhotoStateCallback get_photo_state_callback =
      base::BindOnce(&MockImageCaptureClient::DoOnGetPhotoState,
                     image_capture_client_);
  device_->GetPhotoState(std::move(get_photo_state_callback));

  mojom::PhotoState* state = image_capture_client_->state.get();
  ASSERT_EQ(state->width->min, kArbitraryValidVideoWidth);
  ASSERT_EQ(state->width->current, kArbitraryValidVideoWidth);
  ASSERT_EQ(state->width->max, kArbitraryValidVideoWidth);

  ASSERT_EQ(state->height->min, kArbitraryValidVideoHeight);
  ASSERT_EQ(state->height->current, kArbitraryValidVideoHeight);
  ASSERT_EQ(state->height->max, kArbitraryValidVideoHeight);
}

// Given an |IMFCaptureSource| offering a video stream and a photo stream to
// |VideoCaptureDevice|, when asking the photo state from |VideoCaptureDevice|
// then expect the returned state to match the available photo resolution
TEST_F(VideoCaptureDeviceMFWinTest, GetPhotoStateViaPhotoStream) {
  if (ShouldSkipTest())
    return;

  PrepareMFDeviceWithOneVideoStreamAndOnePhotoStream();

  EXPECT_CALL(*(engine_.Get()), OnStartPreview());
  EXPECT_CALL(*client_, OnStarted());

  device_->AllocateAndStart(VideoCaptureParams(), std::move(client_));
  VideoCaptureDevice::GetPhotoStateCallback get_photo_state_callback =
      base::BindOnce(&MockImageCaptureClient::DoOnGetPhotoState,
                     image_capture_client_);
  device_->GetPhotoState(std::move(get_photo_state_callback));

  mojom::PhotoState* state = image_capture_client_->state.get();
  ASSERT_EQ(state->width->min, kArbitraryValidPhotoWidth);
  ASSERT_EQ(state->width->current, kArbitraryValidPhotoWidth);
  ASSERT_EQ(state->width->max, kArbitraryValidPhotoWidth);

  ASSERT_EQ(state->height->min, kArbitraryValidPhotoHeight);
  ASSERT_EQ(state->height->current, kArbitraryValidPhotoHeight);
  ASSERT_EQ(state->height->max, kArbitraryValidPhotoHeight);
}

// Given an |IMFCaptureSource| offering a video stream and a photo stream to
// |VideoCaptureDevice|, when taking photo from |VideoCaptureDevice| then
// expect IMFCaptureEngine::TakePhoto() to be called
TEST_F(VideoCaptureDeviceMFWinTest, TakePhotoViaPhotoStream) {
  if (ShouldSkipTest())
    return;

  PrepareMFDeviceWithOneVideoStreamAndOnePhotoStream();

  EXPECT_CALL(*(engine_.Get()), OnStartPreview());
  EXPECT_CALL(*client_, OnStarted());

  EXPECT_CALL(*(engine_.Get()), OnTakePhoto());

  device_->AllocateAndStart(VideoCaptureParams(), std::move(client_));
  VideoCaptureDevice::TakePhotoCallback take_photo_callback = base::BindOnce(
      &MockImageCaptureClient::DoOnPhotoTaken, image_capture_client_);
  device_->TakePhoto(std::move(take_photo_callback));
}

}  // namespace media