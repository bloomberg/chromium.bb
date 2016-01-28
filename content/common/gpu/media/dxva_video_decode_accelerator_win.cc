// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/dxva_video_decode_accelerator_win.h"

#if !defined(OS_WIN)
#error This file should only be built on Windows.
#endif   // !defined(OS_WIN)

#include <codecapi.h>
#include <dxgi1_2.h>
#include <ks.h>
#include <mfapi.h>
#include <mferror.h>
#include <ntverp.h>
#include <stddef.h>
#include <string.h>
#include <wmcodecdsp.h>

#include "base/base_paths_win.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "media/base/win/mf_initializer.h"
#include "media/video/video_decode_accelerator.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"

namespace {

// Path is appended on to the PROGRAM_FILES base path.
const wchar_t kVPXDecoderDLLPath[] = L"Intel\\Media SDK\\";

const wchar_t kVP8DecoderDLLName[] =
#if defined(ARCH_CPU_X86)
  L"mfx_mft_vp8vd_32.dll";
#elif defined(ARCH_CPU_X86_64)
  L"mfx_mft_vp8vd_64.dll";
#else
#error Unsupported Windows CPU Architecture
#endif

const wchar_t kVP9DecoderDLLName[] =
#if defined(ARCH_CPU_X86)
  L"mfx_mft_vp9vd_32.dll";
#elif defined(ARCH_CPU_X86_64)
  L"mfx_mft_vp9vd_64.dll";
#else
#error Unsupported Windows CPU Architecture
#endif

const CLSID CLSID_WebmMfVp8Dec = {
  0x451e3cb7,
  0x2622,
  0x4ba5,
  { 0x8e, 0x1d, 0x44, 0xb3, 0xc4, 0x1d, 0x09, 0x24 }
};

const CLSID CLSID_WebmMfVp9Dec = {
  0x07ab4bd2,
  0x1979,
  0x4fcd,
  { 0xa6, 0x97, 0xdf, 0x9a, 0xd1, 0x5b, 0x34, 0xfe }
};

const CLSID MEDIASUBTYPE_VP80 = {
  0x30385056,
  0x0000,
  0x0010,
  { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

const CLSID MEDIASUBTYPE_VP90 = {
  0x30395056,
  0x0000,
  0x0010,
  { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

// The CLSID of the video processor media foundation transform which we use for
// texture color conversion in DX11.
// Defined in mfidl.h in the Windows 10 SDK. ntverp.h provides VER_PRODUCTBUILD
// to detect which SDK we are compiling with.
#if VER_PRODUCTBUILD < 10011  // VER_PRODUCTBUILD for 10.0.10158.0 SDK.
DEFINE_GUID(CLSID_VideoProcessorMFT,
            0x88753b26, 0x5b24, 0x49bd, 0xb2, 0xe7, 0xc, 0x44, 0x5c, 0x78,
            0xc9, 0x82);
#endif

// MF_XVP_PLAYBACK_MODE
// Data type: UINT32 (treat as BOOL)
// If this attribute is TRUE, the video processor will run in playback mode
// where it allows callers to allocate output samples and allows last frame
// regeneration (repaint).
DEFINE_GUID(MF_XVP_PLAYBACK_MODE, 0x3c5d293f, 0xad67, 0x4e29, 0xaf, 0x12,
            0xcf, 0x3e, 0x23, 0x8a, 0xcc, 0xe9);

}  // namespace

namespace content {

static const media::VideoCodecProfile kSupportedProfiles[] = {
  media::H264PROFILE_BASELINE,
  media::H264PROFILE_MAIN,
  media::H264PROFILE_HIGH,
  media::VP8PROFILE_ANY,
  media::VP9PROFILE_ANY
};

CreateDXGIDeviceManager DXVAVideoDecodeAccelerator::create_dxgi_device_manager_
    = NULL;

#define RETURN_ON_FAILURE(result, log, ret)  \
  do {                                       \
    if (!(result)) {                         \
      DLOG(ERROR) << log;                    \
      return ret;                            \
    }                                        \
  } while (0)

#define RETURN_ON_HR_FAILURE(result, log, ret)                    \
  RETURN_ON_FAILURE(SUCCEEDED(result),                            \
                    log << ", HRESULT: 0x" << std::hex << result, \
                    ret);

#define RETURN_AND_NOTIFY_ON_FAILURE(result, log, error_code, ret)  \
  do {                                                              \
    if (!(result)) {                                                \
      DVLOG(1) << log;                                              \
      StopOnError(error_code);                                      \
      return ret;                                                   \
    }                                                               \
  } while (0)

#define RETURN_AND_NOTIFY_ON_HR_FAILURE(result, log, error_code, ret)  \
  RETURN_AND_NOTIFY_ON_FAILURE(SUCCEEDED(result),                      \
                               log << ", HRESULT: 0x" << std::hex << result, \
                               error_code, ret);

enum {
  // Maximum number of iterations we allow before aborting the attempt to flush
  // the batched queries to the driver and allow torn/corrupt frames to be
  // rendered.
  kFlushDecoderSurfaceTimeoutMs = 1,
  // Maximum iterations where we try to flush the d3d device.
  kMaxIterationsForD3DFlush = 4,
  // We only request 5 picture buffers from the client which are used to hold
  // the decoded samples. These buffers are then reused when the client tells
  // us that it is done with the buffer.
  kNumPictureBuffers = 5,
  // The keyed mutex should always be released before the other thread
  // attempts to acquire it, so AcquireSync should always return immediately.
  kAcquireSyncWaitMs = 0,
};

static IMFSample* CreateEmptySample() {
  base::win::ScopedComPtr<IMFSample> sample;
  HRESULT hr = MFCreateSample(sample.Receive());
  RETURN_ON_HR_FAILURE(hr, "MFCreateSample failed", NULL);
  return sample.Detach();
}

// Creates a Media Foundation sample with one buffer of length |buffer_length|
// on a |align|-byte boundary. Alignment must be a perfect power of 2 or 0.
static IMFSample* CreateEmptySampleWithBuffer(int buffer_length, int align) {
  CHECK_GT(buffer_length, 0);

  base::win::ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateEmptySample());

  base::win::ScopedComPtr<IMFMediaBuffer> buffer;
  HRESULT hr = E_FAIL;
  if (align == 0) {
    // Note that MFCreateMemoryBuffer is same as MFCreateAlignedMemoryBuffer
    // with the align argument being 0.
    hr = MFCreateMemoryBuffer(buffer_length, buffer.Receive());
  } else {
    hr = MFCreateAlignedMemoryBuffer(buffer_length,
                                     align - 1,
                                     buffer.Receive());
  }
  RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer for sample", NULL);

  hr = sample->AddBuffer(buffer.get());
  RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", NULL);

  buffer->SetCurrentLength(0);
  return sample.Detach();
}

// Creates a Media Foundation sample with one buffer containing a copy of the
// given Annex B stream data.
// If duration and sample time are not known, provide 0.
// |min_size| specifies the minimum size of the buffer (might be required by
// the decoder for input). If no alignment is required, provide 0.
static IMFSample* CreateInputSample(const uint8_t* stream,
                                    int size,
                                    int min_size,
                                    int alignment) {
  CHECK(stream);
  CHECK_GT(size, 0);
  base::win::ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateEmptySampleWithBuffer(std::max(min_size, size),
                                            alignment));
  RETURN_ON_FAILURE(sample.get(), "Failed to create empty sample", NULL);

  base::win::ScopedComPtr<IMFMediaBuffer> buffer;
  HRESULT hr = sample->GetBufferByIndex(0, buffer.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from sample", NULL);

  DWORD max_length = 0;
  DWORD current_length = 0;
  uint8_t* destination = NULL;
  hr = buffer->Lock(&destination, &max_length, &current_length);
  RETURN_ON_HR_FAILURE(hr, "Failed to lock buffer", NULL);

  CHECK_EQ(current_length, 0u);
  CHECK_GE(static_cast<int>(max_length), size);
  memcpy(destination, stream, size);

  hr = buffer->Unlock();
  RETURN_ON_HR_FAILURE(hr, "Failed to unlock buffer", NULL);

  hr = buffer->SetCurrentLength(size);
  RETURN_ON_HR_FAILURE(hr, "Failed to set buffer length", NULL);

  return sample.Detach();
}

static IMFSample* CreateSampleFromInputBuffer(
    const media::BitstreamBuffer& bitstream_buffer,
    DWORD stream_size,
    DWORD alignment) {
  base::SharedMemory shm(bitstream_buffer.handle(), true);
  RETURN_ON_FAILURE(shm.Map(bitstream_buffer.size()),
                    "Failed in base::SharedMemory::Map", NULL);

  return CreateInputSample(reinterpret_cast<const uint8_t*>(shm.memory()),
                           bitstream_buffer.size(), stream_size, alignment);
}

// Helper function to create a COM object instance from a DLL. The alternative
// is to use the CoCreateInstance API which requires the COM apartment to be
// initialized which is not the case on the GPU main thread. We want to avoid
// initializing COM as it may have sideeffects.
HRESULT CreateCOMObjectFromDll(HMODULE dll, const CLSID& clsid, const IID& iid,
                               void** object) {
  if (!dll || !object)
    return E_INVALIDARG;

  using GetClassObject = HRESULT (WINAPI*)(
      const CLSID& clsid, const IID& iid, void** object);

  GetClassObject get_class_object = reinterpret_cast<GetClassObject>(
      GetProcAddress(dll, "DllGetClassObject"));
  RETURN_ON_FAILURE(
      get_class_object, "Failed to get DllGetClassObject pointer", E_FAIL);

  base::win::ScopedComPtr<IClassFactory> factory;
  HRESULT hr = get_class_object(
      clsid,
      __uuidof(IClassFactory),
      factory.ReceiveVoid());
  RETURN_ON_HR_FAILURE(hr, "DllGetClassObject failed", hr);

  hr = factory->CreateInstance(NULL, iid, object);
  return hr;
}

// Helper function to query the ANGLE device object. The template argument T
// identifies the device interface being queried. IDirect3DDevice9Ex for d3d9
// and ID3D11Device for dx11.
template<class T>
base::win::ScopedComPtr<T> QueryDeviceObjectFromANGLE(int object_type) {
  base::win::ScopedComPtr<T> device_object;

  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();
  intptr_t egl_device = 0;
  intptr_t device = 0;

  RETURN_ON_FAILURE(
      gfx::GLSurfaceEGL::HasEGLExtension("EGL_EXT_device_query"),
      "EGL_EXT_device_query missing",
      device_object);

  PFNEGLQUERYDISPLAYATTRIBEXTPROC QueryDisplayAttribEXT =
      reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress(
        "eglQueryDisplayAttribEXT"));

  RETURN_ON_FAILURE(
      QueryDisplayAttribEXT,
      "Failed to get the eglQueryDisplayAttribEXT function from ANGLE",
      device_object);

  PFNEGLQUERYDEVICEATTRIBEXTPROC QueryDeviceAttribEXT =
      reinterpret_cast<PFNEGLQUERYDEVICEATTRIBEXTPROC>(eglGetProcAddress(
          "eglQueryDeviceAttribEXT"));

  RETURN_ON_FAILURE(
      QueryDeviceAttribEXT,
      "Failed to get the eglQueryDeviceAttribEXT function from ANGLE",
      device_object);

  RETURN_ON_FAILURE(
      QueryDisplayAttribEXT(egl_display, EGL_DEVICE_EXT, &egl_device),
      "The eglQueryDisplayAttribEXT function failed to get the EGL device",
      device_object);

  RETURN_ON_FAILURE(
      egl_device,
      "Failed to get the EGL device",
      device_object);

  RETURN_ON_FAILURE(
      QueryDeviceAttribEXT(
      reinterpret_cast<EGLDeviceEXT>(egl_device), object_type, &device),
      "The eglQueryDeviceAttribEXT function failed to get the device",
      device_object);

  RETURN_ON_FAILURE(device, "Failed to get the ANGLE device", device_object);

  device_object = reinterpret_cast<T*>(device);
  return device_object;
}


// Maintains information about a DXVA picture buffer, i.e. whether it is
// available for rendering, the texture information, etc.
struct DXVAVideoDecodeAccelerator::DXVAPictureBuffer {
 public:
  static linked_ptr<DXVAPictureBuffer> Create(
      const DXVAVideoDecodeAccelerator& decoder,
      const media::PictureBuffer& buffer,
      EGLConfig egl_config);
  ~DXVAPictureBuffer();

  bool InitializeTexture(const DXVAVideoDecodeAccelerator& decoder,
                         bool use_rgb);

  bool ReusePictureBuffer();
  // Copies the output sample data to the picture buffer provided by the
  // client.
  // The dest_surface parameter contains the decoded bits.
  bool CopyOutputSampleDataToPictureBuffer(
      DXVAVideoDecodeAccelerator* decoder,
      IDirect3DSurface9* dest_surface,
      ID3D11Texture2D* dx11_texture,
      int input_buffer_id);

  bool available() const {
    return available_;
  }

  void set_available(bool available) {
    available_ = available;
  }

  int id() const {
    return picture_buffer_.id();
  }

  gfx::Size size() const {
    return picture_buffer_.size();
  }

  // Called when the source surface |src_surface| is copied to the destination
  // |dest_surface|
  bool CopySurfaceComplete(IDirect3DSurface9* src_surface,
                           IDirect3DSurface9* dest_surface);

 private:
  explicit DXVAPictureBuffer(const media::PictureBuffer& buffer);

  bool available_;
  media::PictureBuffer picture_buffer_;
  EGLSurface decoding_surface_;

  HANDLE texture_share_handle_;
  base::win::ScopedComPtr<IDirect3DTexture9> decoding_texture_;
  base::win::ScopedComPtr<ID3D11Texture2D> dx11_decoding_texture_;

  base::win::ScopedComPtr<IDXGIKeyedMutex> egl_keyed_mutex_;
  base::win::ScopedComPtr<IDXGIKeyedMutex> dx11_keyed_mutex_;

  // This is the last value that was used to release the keyed mutex.
  uint64_t keyed_mutex_value_;

  // The following |IDirect3DSurface9| interface pointers are used to hold
  // references on the surfaces during the course of a StretchRect operation
  // to copy the source surface to the target. The references are released
  // when the StretchRect operation i.e. the copy completes.
  base::win::ScopedComPtr<IDirect3DSurface9> decoder_surface_;
  base::win::ScopedComPtr<IDirect3DSurface9> target_surface_;

  // This ID3D11Texture2D interface pointer is used to hold a reference to the
  // decoder texture during the course of a copy operation. This reference is
  // released when the copy completes.
  base::win::ScopedComPtr<ID3D11Texture2D> decoder_dx11_texture_;

  // Set to true if RGB is supported by the texture.
  // Defaults to true.
  bool use_rgb_;

  DISALLOW_COPY_AND_ASSIGN(DXVAPictureBuffer);
};

// static
linked_ptr<DXVAVideoDecodeAccelerator::DXVAPictureBuffer>
DXVAVideoDecodeAccelerator::DXVAPictureBuffer::Create(
    const DXVAVideoDecodeAccelerator& decoder,
    const media::PictureBuffer& buffer,
    EGLConfig egl_config) {
  linked_ptr<DXVAPictureBuffer> picture_buffer(new DXVAPictureBuffer(buffer));

  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();

  EGLint use_rgb = 1;
  eglGetConfigAttrib(egl_display, egl_config, EGL_BIND_TO_TEXTURE_RGB,
                     &use_rgb);

  if (!picture_buffer->InitializeTexture(decoder, !!use_rgb))
    return linked_ptr<DXVAPictureBuffer>(nullptr);

  EGLint attrib_list[] = {
    EGL_WIDTH, buffer.size().width(),
    EGL_HEIGHT, buffer.size().height(),
    EGL_TEXTURE_FORMAT, use_rgb ? EGL_TEXTURE_RGB : EGL_TEXTURE_RGBA,
    EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
    EGL_NONE
  };

  picture_buffer->decoding_surface_ = eglCreatePbufferFromClientBuffer(
      egl_display, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
      picture_buffer->texture_share_handle_, egl_config, attrib_list);
  RETURN_ON_FAILURE(picture_buffer->decoding_surface_,
                    "Failed to create surface",
                    linked_ptr<DXVAPictureBuffer>(NULL));
  if (decoder.d3d11_device_ && decoder.use_keyed_mutex_) {
    void* keyed_mutex = nullptr;
    EGLBoolean ret = eglQuerySurfacePointerANGLE(
        egl_display, picture_buffer->decoding_surface_,
        EGL_DXGI_KEYED_MUTEX_ANGLE, &keyed_mutex);
    RETURN_ON_FAILURE(keyed_mutex && ret == EGL_TRUE,
                      "Failed to query ANGLE keyed mutex",
                      linked_ptr<DXVAPictureBuffer>(nullptr));
    picture_buffer->egl_keyed_mutex_ = base::win::ScopedComPtr<IDXGIKeyedMutex>(
        static_cast<IDXGIKeyedMutex*>(keyed_mutex));
  }
  picture_buffer->use_rgb_ = !!use_rgb;
  return picture_buffer;
}

bool DXVAVideoDecodeAccelerator::DXVAPictureBuffer::InitializeTexture(
    const DXVAVideoDecodeAccelerator& decoder,
    bool use_rgb) {
  DCHECK(!texture_share_handle_);
  if (decoder.d3d11_device_) {
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = picture_buffer_.size().width();
    desc.Height = picture_buffer_.size().height();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = decoder.use_keyed_mutex_
                         ? D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX
                         : D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = decoder.d3d11_device_->CreateTexture2D(
        &desc, nullptr, dx11_decoding_texture_.Receive());
    RETURN_ON_HR_FAILURE(hr, "Failed to create texture", false);
    if (decoder.use_keyed_mutex_) {
      hr = dx11_keyed_mutex_.QueryFrom(dx11_decoding_texture_.get());
      RETURN_ON_HR_FAILURE(hr, "Failed to get keyed mutex", false);
    }

    base::win::ScopedComPtr<IDXGIResource> resource;
    hr = resource.QueryFrom(dx11_decoding_texture_.get());
    DCHECK(SUCCEEDED(hr));
    hr = resource->GetSharedHandle(&texture_share_handle_);
    RETURN_ON_FAILURE(SUCCEEDED(hr) && texture_share_handle_,
                      "Failed to query shared handle", false);

  } else {
    HRESULT hr = E_FAIL;
    hr = decoder.d3d9_device_ex_->CreateTexture(
        picture_buffer_.size().width(), picture_buffer_.size().height(), 1,
        D3DUSAGE_RENDERTARGET, use_rgb ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, decoding_texture_.Receive(), &texture_share_handle_);
    RETURN_ON_HR_FAILURE(hr, "Failed to create texture", false);
    RETURN_ON_FAILURE(texture_share_handle_, "Failed to query shared handle",
                      false);
  }
  return true;
}

DXVAVideoDecodeAccelerator::DXVAPictureBuffer::DXVAPictureBuffer(
    const media::PictureBuffer& buffer)
    : available_(true),
      picture_buffer_(buffer),
      decoding_surface_(NULL),
      texture_share_handle_(nullptr),
      keyed_mutex_value_(0),
      use_rgb_(true) {}

DXVAVideoDecodeAccelerator::DXVAPictureBuffer::~DXVAPictureBuffer() {
  if (decoding_surface_) {
    EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();

    eglReleaseTexImage(
        egl_display,
        decoding_surface_,
        EGL_BACK_BUFFER);

    eglDestroySurface(
        egl_display,
        decoding_surface_);
    decoding_surface_ = NULL;
  }
}

bool DXVAVideoDecodeAccelerator::DXVAPictureBuffer::ReusePictureBuffer() {
  DCHECK(decoding_surface_);
  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();
  eglReleaseTexImage(
    egl_display,
    decoding_surface_,
    EGL_BACK_BUFFER);
  decoder_surface_.Release();
  target_surface_.Release();
  decoder_dx11_texture_.Release();
  set_available(true);
  if (egl_keyed_mutex_) {
    HRESULT hr = egl_keyed_mutex_->ReleaseSync(++keyed_mutex_value_);
    RETURN_ON_FAILURE(hr == S_OK, "Could not release sync mutex", false);
  }
  return true;
}

bool DXVAVideoDecodeAccelerator::DXVAPictureBuffer::
    CopyOutputSampleDataToPictureBuffer(
        DXVAVideoDecodeAccelerator* decoder,
        IDirect3DSurface9* dest_surface,
        ID3D11Texture2D* dx11_texture,
        int input_buffer_id) {
  DCHECK(dest_surface || dx11_texture);
  if (dx11_texture) {
    // Grab a reference on the decoder texture. This reference will be released
    // when we receive a notification that the copy was completed or when the
    // DXVAPictureBuffer instance is destroyed.
    decoder_dx11_texture_ = dx11_texture;
    decoder->CopyTexture(dx11_texture, dx11_decoding_texture_.get(),
                         dx11_keyed_mutex_, keyed_mutex_value_, NULL, id(),
                         input_buffer_id);
    return true;
  }
  D3DSURFACE_DESC surface_desc;
  HRESULT hr = dest_surface->GetDesc(&surface_desc);
  RETURN_ON_HR_FAILURE(hr, "Failed to get surface description", false);

  D3DSURFACE_DESC texture_desc;
  decoding_texture_->GetLevelDesc(0, &texture_desc);

  if (texture_desc.Width != surface_desc.Width ||
      texture_desc.Height != surface_desc.Height) {
    NOTREACHED() << "Decode surface of different dimension than texture";
    return false;
  }

  hr = decoder->d3d9_->CheckDeviceFormatConversion(
      D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, surface_desc.Format,
      use_rgb_ ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8);
  RETURN_ON_HR_FAILURE(hr, "Device does not support format converision", false);

  // The same picture buffer can be reused for a different frame. Release the
  // target surface and the decoder references here.
  target_surface_.Release();
  decoder_surface_.Release();

  // Grab a reference on the decoder surface and the target surface. These
  // references will be released when we receive a notification that the
  // copy was completed or when the DXVAPictureBuffer instance is destroyed.
  // We hold references here as it is easier to manage their lifetimes.
  hr = decoding_texture_->GetSurfaceLevel(0, target_surface_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get surface from texture", false);

  decoder_surface_ = dest_surface;

  decoder->CopySurface(decoder_surface_.get(), target_surface_.get(), id(),
                       input_buffer_id);
  return true;
}

bool DXVAVideoDecodeAccelerator::DXVAPictureBuffer::CopySurfaceComplete(
    IDirect3DSurface9* src_surface,
    IDirect3DSurface9* dest_surface) {
  DCHECK(!available());

  GLint current_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

  glBindTexture(GL_TEXTURE_2D, picture_buffer_.texture_id());

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  if (src_surface && dest_surface) {
    DCHECK_EQ(src_surface, decoder_surface_.get());
    DCHECK_EQ(dest_surface, target_surface_.get());
    decoder_surface_.Release();
    target_surface_.Release();
  } else {
    DCHECK(decoder_dx11_texture_.get());
    decoder_dx11_texture_.Release();
  }
  if (egl_keyed_mutex_) {
    keyed_mutex_value_++;
    HRESULT result =
        egl_keyed_mutex_->AcquireSync(keyed_mutex_value_, kAcquireSyncWaitMs);
    RETURN_ON_FAILURE(result == S_OK, "Could not acquire sync mutex", false);
  }

  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();
  eglBindTexImage(
      egl_display,
      decoding_surface_,
      EGL_BACK_BUFFER);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, current_texture);
  return true;
}

DXVAVideoDecodeAccelerator::PendingSampleInfo::PendingSampleInfo(
    int32_t buffer_id,
    IMFSample* sample)
    : input_buffer_id(buffer_id), picture_buffer_id(-1) {
  output_sample.Attach(sample);
}

DXVAVideoDecodeAccelerator::PendingSampleInfo::~PendingSampleInfo() {}

DXVAVideoDecodeAccelerator::DXVAVideoDecodeAccelerator(
    const base::Callback<bool(void)>& make_context_current,
    gfx::GLContext* gl_context)
    : client_(NULL),
      dev_manager_reset_token_(0),
      dx11_dev_manager_reset_token_(0),
      egl_config_(NULL),
      state_(kUninitialized),
      pictures_requested_(false),
      inputs_before_decode_(0),
      sent_drain_message_(false),
      make_context_current_(make_context_current),
      codec_(media::kUnknownVideoCodec),
      decoder_thread_("DXVAVideoDecoderThread"),
      pending_flush_(false),
      use_dx11_(false),
      use_keyed_mutex_(false),
      dx11_video_format_converter_media_type_needs_init_(true),
      gl_context_(gl_context),
      using_angle_device_(false),
      weak_this_factory_(this) {
  weak_ptr_ = weak_this_factory_.GetWeakPtr();
  memset(&input_stream_info_, 0, sizeof(input_stream_info_));
  memset(&output_stream_info_, 0, sizeof(output_stream_info_));
}

DXVAVideoDecodeAccelerator::~DXVAVideoDecodeAccelerator() {
  client_ = NULL;
}

bool DXVAVideoDecodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
  if (config.is_encrypted) {
    NOTREACHED() << "Encrypted streams are not supported for this VDA";
    return false;
  }

  client_ = client;

  main_thread_task_runner_ = base::MessageLoop::current()->task_runner();

  bool profile_supported = false;
  for (const auto& supported_profile : kSupportedProfiles) {
    if (config.profile == supported_profile) {
      profile_supported = true;
      break;
    }
  }
  if (!profile_supported) {
    RETURN_AND_NOTIFY_ON_FAILURE(false,
        "Unsupported h.264, vp8, or vp9 profile", PLATFORM_FAILURE, false);
  }

  // Not all versions of Windows 7 and later include Media Foundation DLLs.
  // Instead of crashing while delay loading the DLL when calling MFStartup()
  // below, probe whether we can successfully load the DLL now.
  // See http://crbug.com/339678 for details.
  HMODULE dxgi_manager_dll = ::GetModuleHandle(L"MFPlat.dll");
  RETURN_ON_FAILURE(dxgi_manager_dll, "MFPlat.dll is required for decoding",
                    false);

  // On Windows 8+ mfplat.dll provides the MFCreateDXGIDeviceManager API.
  // On Windows 7 mshtmlmedia.dll provides it.

  // TODO(ananta)
  // The code below works, as in we can create the DX11 device manager for
  // Windows 7. However the IMFTransform we use for texture conversion and
  // copy does not exist on Windows 7. Look into an alternate approach
  // and enable the code below.
#if defined(ENABLE_DX11_FOR_WIN7)
  if (base::win::GetVersion() == base::win::VERSION_WIN7) {
    dxgi_manager_dll = ::GetModuleHandle(L"mshtmlmedia.dll");
    RETURN_ON_FAILURE(dxgi_manager_dll,
        "mshtmlmedia.dll is required for decoding", false);
  }
#endif
  // If we don't find the MFCreateDXGIDeviceManager API we fallback to D3D9
  // decoding.
  if (dxgi_manager_dll && !create_dxgi_device_manager_) {
    create_dxgi_device_manager_ = reinterpret_cast<CreateDXGIDeviceManager>(
            ::GetProcAddress(dxgi_manager_dll, "MFCreateDXGIDeviceManager"));
  }

  RETURN_AND_NOTIFY_ON_FAILURE(
      gfx::g_driver_egl.ext.b_EGL_ANGLE_surface_d3d_texture_2d_share_handle,
      "EGL_ANGLE_surface_d3d_texture_2d_share_handle unavailable",
      PLATFORM_FAILURE,
      false);

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state == kUninitialized),
      "Initialize: invalid state: " << state, ILLEGAL_STATE, false);

  media::InitializeMediaFoundation();

  RETURN_AND_NOTIFY_ON_FAILURE(InitDecoder(config.profile),
      "Failed to initialize decoder", PLATFORM_FAILURE, false);

  RETURN_AND_NOTIFY_ON_FAILURE(GetStreamsInfoAndBufferReqs(),
      "Failed to get input/output stream info.", PLATFORM_FAILURE, false);

  RETURN_AND_NOTIFY_ON_FAILURE(
      SendMFTMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0),
      "Send MFT_MESSAGE_NOTIFY_BEGIN_STREAMING notification failed",
      PLATFORM_FAILURE, false);

  RETURN_AND_NOTIFY_ON_FAILURE(
      SendMFTMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0),
      "Send MFT_MESSAGE_NOTIFY_START_OF_STREAM notification failed",
      PLATFORM_FAILURE, false);

  SetState(kNormal);

  StartDecoderThread();
  return true;
}

bool DXVAVideoDecodeAccelerator::CreateD3DDevManager() {
  TRACE_EVENT0("gpu", "DXVAVideoDecodeAccelerator_CreateD3DDevManager");

  HRESULT hr = E_FAIL;

  hr = Direct3DCreate9Ex(D3D_SDK_VERSION, d3d9_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Direct3DCreate9Ex failed", false);

  base::win::ScopedComPtr<IDirect3DDevice9> angle_device =
      QueryDeviceObjectFromANGLE<IDirect3DDevice9>(EGL_D3D9_DEVICE_ANGLE);
  if (angle_device.get())
    using_angle_device_ = true;

  if (using_angle_device_) {
    hr = d3d9_device_ex_.QueryFrom(angle_device.get());
    RETURN_ON_HR_FAILURE(hr,
        "QueryInterface for IDirect3DDevice9Ex from angle device failed",
        false);
  } else {
    D3DPRESENT_PARAMETERS present_params = {0};
    present_params.BackBufferWidth = 1;
    present_params.BackBufferHeight = 1;
    present_params.BackBufferFormat = D3DFMT_UNKNOWN;
    present_params.BackBufferCount = 1;
    present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present_params.hDeviceWindow = NULL;
    present_params.Windowed = TRUE;
    present_params.Flags = D3DPRESENTFLAG_VIDEO;
    present_params.FullScreen_RefreshRateInHz = 0;
    present_params.PresentationInterval = 0;

    hr = d3d9_->CreateDeviceEx(D3DADAPTER_DEFAULT,
                               D3DDEVTYPE_HAL,
                               NULL,
                               D3DCREATE_FPU_PRESERVE |
                               D3DCREATE_HARDWARE_VERTEXPROCESSING |
                               D3DCREATE_DISABLE_PSGP_THREADING |
                               D3DCREATE_MULTITHREADED,
                               &present_params,
                               NULL,
                               d3d9_device_ex_.Receive());
    RETURN_ON_HR_FAILURE(hr, "Failed to create D3D device", false);
  }

  hr = DXVA2CreateDirect3DDeviceManager9(&dev_manager_reset_token_,
                                         device_manager_.Receive());
  RETURN_ON_HR_FAILURE(hr, "DXVA2CreateDirect3DDeviceManager9 failed", false);

  hr = device_manager_->ResetDevice(d3d9_device_ex_.get(),
                                    dev_manager_reset_token_);
  RETURN_ON_HR_FAILURE(hr, "Failed to reset device", false);

  hr = d3d9_device_ex_->CreateQuery(D3DQUERYTYPE_EVENT, query_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to create D3D device query", false);
  // Ensure query_ API works (to avoid an infinite loop later in
  // CopyOutputSampleDataToPictureBuffer).
  hr = query_->Issue(D3DISSUE_END);
  RETURN_ON_HR_FAILURE(hr, "Failed to issue END test query", false);
  return true;
}

bool DXVAVideoDecodeAccelerator::CreateDX11DevManager() {
  HRESULT hr = create_dxgi_device_manager_(&dx11_dev_manager_reset_token_,
                                           d3d11_device_manager_.Receive());
  RETURN_ON_HR_FAILURE(hr, "MFCreateDXGIDeviceManager failed", false);

  // This array defines the set of DirectX hardware feature levels we support.
  // The ordering MUST be preserved. All applications are assumed to support
  // 9.1 unless otherwise stated by the application.
  D3D_FEATURE_LEVEL feature_levels[] = {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_9_2,
    D3D_FEATURE_LEVEL_9_1
  };

  UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

#if defined _DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  D3D_FEATURE_LEVEL feature_level_out = D3D_FEATURE_LEVEL_11_0;
  hr = D3D11CreateDevice(NULL,
                         D3D_DRIVER_TYPE_HARDWARE,
                         NULL,
                         flags,
                         feature_levels,
                         arraysize(feature_levels),
                         D3D11_SDK_VERSION,
                         d3d11_device_.Receive(),
                         &feature_level_out,
                         d3d11_device_context_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to create DX11 device", false);

  // Enable multithreaded mode on the device. This ensures that accesses to
  // context are synchronized across threads. We have multiple threads
  // accessing the context, the media foundation decoder threads and the
  // decoder thread via the video format conversion transform.
  hr = multi_threaded_.QueryFrom(d3d11_device_.get());
  RETURN_ON_HR_FAILURE(hr, "Failed to query ID3D10Multithread", false);
  multi_threaded_->SetMultithreadProtected(TRUE);

  hr = d3d11_device_manager_->ResetDevice(d3d11_device_.get(),
                                          dx11_dev_manager_reset_token_);
  RETURN_ON_HR_FAILURE(hr, "Failed to reset device", false);

  D3D11_QUERY_DESC query_desc;
  query_desc.Query = D3D11_QUERY_EVENT;
  query_desc.MiscFlags = 0;
  hr = d3d11_device_->CreateQuery(
      &query_desc,
      d3d11_query_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to create DX11 device query", false);

  HMODULE video_processor_dll = ::GetModuleHandle(L"msvproc.dll");
  RETURN_ON_FAILURE(video_processor_dll, "Failed to load video processor",
                    false);

  hr = CreateCOMObjectFromDll(
      video_processor_dll,
      CLSID_VideoProcessorMFT,
      __uuidof(IMFTransform),
      video_format_converter_mft_.ReceiveVoid());
  if (FAILED(hr)) {
    base::debug::Alias(&hr);
    // TODO(ananta)
    // Remove this CHECK when the change to use DX11 for H/W decoding
    // stablizes.
    CHECK(false);
  }

  RETURN_ON_HR_FAILURE(hr, "Failed to create video format converter", false);

  base::win::ScopedComPtr<IMFAttributes> converter_attributes;
  hr = video_format_converter_mft_->GetAttributes(
      converter_attributes.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get converter attributes", false);

  hr = converter_attributes->SetUINT32(MF_XVP_PLAYBACK_MODE, TRUE);
  RETURN_ON_HR_FAILURE(
      hr,
      "Failed to set MF_XVP_PLAYBACK_MODE attribute on converter",
      false);

  hr = converter_attributes->SetUINT32(MF_LOW_LATENCY, FALSE);
  RETURN_ON_HR_FAILURE(
      hr,
      "Failed to set MF_LOW_LATENCY attribute on converter",
      false);
  return true;
}

void DXVAVideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state == kNormal || state == kStopped ||
                                state == kFlushing),
           "Invalid state: " << state, ILLEGAL_STATE,);

  base::win::ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateSampleFromInputBuffer(bitstream_buffer,
                                            input_stream_info_.cbSize,
                                            input_stream_info_.cbAlignment));
  RETURN_AND_NOTIFY_ON_FAILURE(sample.get(), "Failed to create input sample",
                               PLATFORM_FAILURE, );

  RETURN_AND_NOTIFY_ON_HR_FAILURE(sample->SetSampleTime(bitstream_buffer.id()),
      "Failed to associate input buffer id with sample", PLATFORM_FAILURE,);

  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::DecodeInternal,
                 base::Unretained(this), sample));
}

void DXVAVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state != kUninitialized),
      "Invalid state: " << state, ILLEGAL_STATE,);
  RETURN_AND_NOTIFY_ON_FAILURE((kNumPictureBuffers >= buffers.size()),
      "Failed to provide requested picture buffers. (Got " << buffers.size() <<
      ", requested " << kNumPictureBuffers << ")", INVALID_ARGUMENT,);

  // Copy the picture buffers provided by the client to the available list,
  // and mark these buffers as available for use.
  for (size_t buffer_index = 0; buffer_index < buffers.size();
       ++buffer_index) {
    linked_ptr<DXVAPictureBuffer> picture_buffer =
        DXVAPictureBuffer::Create(*this, buffers[buffer_index], egl_config_);
    RETURN_AND_NOTIFY_ON_FAILURE(picture_buffer.get(),
        "Failed to allocate picture buffer", PLATFORM_FAILURE,);

    bool inserted = output_picture_buffers_.insert(std::make_pair(
        buffers[buffer_index].id(), picture_buffer)).second;
    DCHECK(inserted);
  }

  ProcessPendingSamples();
  if (pending_flush_) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::FlushInternal,
                   base::Unretained(this)));
  }
}

void DXVAVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_buffer_id) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state != kUninitialized),
      "Invalid state: " << state, ILLEGAL_STATE,);

  if (output_picture_buffers_.empty() && stale_output_picture_buffers_.empty())
    return;

  OutputBuffers::iterator it = output_picture_buffers_.find(picture_buffer_id);
  // If we didn't find the picture id in the |output_picture_buffers_| map we
  // try the |stale_output_picture_buffers_| map, as this may have been an
  // output picture buffer from before a resolution change, that at resolution
  // change time had yet to be displayed. The client is calling us back to tell
  // us that we can now recycle this picture buffer, so if we were waiting to
  // dispose of it we now can.
  if (it == output_picture_buffers_.end()) {
    it = stale_output_picture_buffers_.find(picture_buffer_id);
    RETURN_AND_NOTIFY_ON_FAILURE(it != stale_output_picture_buffers_.end(),
        "Invalid picture id: " << picture_buffer_id, INVALID_ARGUMENT,);
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::DeferredDismissStaleBuffer,
                   weak_this_factory_.GetWeakPtr(), picture_buffer_id));
    return;
  }

  RETURN_AND_NOTIFY_ON_FAILURE(it->second->ReusePictureBuffer(),
                               "Failed to reuse picture buffer",
                               PLATFORM_FAILURE, );

  ProcessPendingSamples();
  if (pending_flush_) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::FlushInternal,
                   base::Unretained(this)));
  }
}

void DXVAVideoDecodeAccelerator::Flush() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "DXVAVideoDecodeAccelerator::Flush";

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state == kNormal || state == kStopped),
      "Unexpected decoder state: " << state, ILLEGAL_STATE,);

  SetState(kFlushing);

  pending_flush_ = true;

  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::FlushInternal,
                  base::Unretained(this)));
}

void DXVAVideoDecodeAccelerator::Reset() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "DXVAVideoDecodeAccelerator::Reset";

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state == kNormal || state == kStopped),
      "Reset: invalid state: " << state, ILLEGAL_STATE,);

  decoder_thread_.Stop();

  SetState(kResetting);

  // If we have pending output frames waiting for display then we drop those
  // frames and set the corresponding picture buffer as available.
  PendingOutputSamples::iterator index;
  for (index = pending_output_samples_.begin();
       index != pending_output_samples_.end();
       ++index) {
    if (index->picture_buffer_id != -1) {
      OutputBuffers::iterator it = output_picture_buffers_.find(
          index->picture_buffer_id);
      if (it != output_picture_buffers_.end()) {
        DXVAPictureBuffer* picture_buffer = it->second.get();
        picture_buffer->ReusePictureBuffer();
      }
    }
  }

  pending_output_samples_.clear();

  NotifyInputBuffersDropped();

  RETURN_AND_NOTIFY_ON_FAILURE(SendMFTMessage(MFT_MESSAGE_COMMAND_FLUSH, 0),
      "Reset: Failed to send message.", PLATFORM_FAILURE,);

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::NotifyResetDone,
                 weak_this_factory_.GetWeakPtr()));

  StartDecoderThread();
  SetState(kNormal);
}

void DXVAVideoDecodeAccelerator::Destroy() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  Invalidate();
  delete this;
}

bool DXVAVideoDecodeAccelerator::CanDecodeOnIOThread() {
  return false;
}

GLenum DXVAVideoDecodeAccelerator::GetSurfaceInternalFormat() const {
  return GL_BGRA_EXT;
}

// static
media::VideoDecodeAccelerator::SupportedProfiles
DXVAVideoDecodeAccelerator::GetSupportedProfiles() {
  // TODO(henryhsu): Need to ensure the profiles are actually supported.
  SupportedProfiles profiles;
  for (const auto& supported_profile : kSupportedProfiles) {
    SupportedProfile profile;
    profile.profile = supported_profile;
    // Windows Media Foundation H.264 decoding does not support decoding videos
    // with any dimension smaller than 48 pixels:
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd797815
    profile.min_resolution.SetSize(48, 48);
    // Use 1088 to account for 16x16 macroblocks.
    profile.max_resolution.SetSize(1920, 1088);
    profiles.push_back(profile);
  }
  return profiles;
}

// static
void DXVAVideoDecodeAccelerator::PreSandboxInitialization() {
  ::LoadLibrary(L"MFPlat.dll");
  ::LoadLibrary(L"msmpeg2vdec.dll");

  if (base::win::GetVersion() > base::win::VERSION_WIN7) {
    LoadLibrary(L"msvproc.dll");
  } else {
    LoadLibrary(L"dxva2.dll");
#if defined(ENABLE_DX11_FOR_WIN7)
    LoadLibrary(L"mshtmlmedia.dll");
#endif
  }
}

bool DXVAVideoDecodeAccelerator::InitDecoder(media::VideoCodecProfile profile) {
  HMODULE decoder_dll = NULL;

  CLSID clsid = {};

  // Profile must fall within the valid range for one of the supported codecs.
  if (profile >= media::H264PROFILE_MIN && profile <= media::H264PROFILE_MAX) {
    // We mimic the steps CoCreateInstance uses to instantiate the object. This
    // was previously done because it failed inside the sandbox, and now is done
    // as a more minimal approach to avoid other side-effects CCI might have (as
    // we are still in a reduced sandbox).
    decoder_dll = ::GetModuleHandle(L"msmpeg2vdec.dll");
    RETURN_ON_FAILURE(decoder_dll,
                      "msmpeg2vdec.dll required for decoding is not loaded",
                      false);

    // Check version of DLL, version 6.7.7140 is blacklisted due to high crash
    // rates in browsers loading that DLL. If that is the version installed we
    // fall back to software decoding. See crbug/403440.
    FileVersionInfo* version_info =
        FileVersionInfo::CreateFileVersionInfoForModule(decoder_dll);
    RETURN_ON_FAILURE(version_info,
                      "unable to get version of msmpeg2vdec.dll",
                      false);
    base::string16 file_version = version_info->file_version();
    RETURN_ON_FAILURE(file_version.find(L"6.1.7140") == base::string16::npos,
                      "blacklisted version of msmpeg2vdec.dll 6.7.7140",
                      false);
    codec_ = media::kCodecH264;
    clsid = __uuidof(CMSH264DecoderMFT);
  } else if ((profile == media::VP8PROFILE_ANY ||
              profile == media::VP9PROFILE_ANY) &&
             base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kEnableAcceleratedVpxDecode)) {
    int program_files_key = base::DIR_PROGRAM_FILES;
    if (base::win::OSInfo::GetInstance()->wow64_status() ==
        base::win::OSInfo::WOW64_ENABLED) {
      program_files_key = base::DIR_PROGRAM_FILES6432;
    }

    base::FilePath dll_path;
    RETURN_ON_FAILURE(PathService::Get(program_files_key, &dll_path),
        "failed to get path for Program Files", false);

    dll_path = dll_path.Append(kVPXDecoderDLLPath);
    if (profile == media::VP8PROFILE_ANY) {
      codec_ = media::kCodecVP8;
      dll_path = dll_path.Append(kVP8DecoderDLLName);
      clsid = CLSID_WebmMfVp8Dec;
    } else {
      codec_ = media::kCodecVP9;
      dll_path = dll_path.Append(kVP9DecoderDLLName);
      clsid = CLSID_WebmMfVp9Dec;
    }
    decoder_dll = ::LoadLibraryEx(dll_path.value().data(), NULL,
        LOAD_WITH_ALTERED_SEARCH_PATH);
    RETURN_ON_FAILURE(decoder_dll, "vpx decoder dll is not loaded", false);
  } else {
    RETURN_ON_FAILURE(false, "Unsupported codec.", false);
  }

  HRESULT hr = CreateCOMObjectFromDll(decoder_dll,
                                      clsid,
                                      __uuidof(IMFTransform),
                                      decoder_.ReceiveVoid());
  RETURN_ON_HR_FAILURE(hr, "Failed to create decoder instance", false);

  RETURN_ON_FAILURE(CheckDecoderDxvaSupport(),
                    "Failed to check decoder DXVA support", false);

  ULONG_PTR device_manager_to_use = NULL;
  if (use_dx11_) {
    CHECK(create_dxgi_device_manager_);
    RETURN_AND_NOTIFY_ON_FAILURE(CreateDX11DevManager(),
                                 "Failed to initialize DX11 device and manager",
                                 PLATFORM_FAILURE,
                                 false);
    device_manager_to_use = reinterpret_cast<ULONG_PTR>(
        d3d11_device_manager_.get());
  } else {
    RETURN_AND_NOTIFY_ON_FAILURE(CreateD3DDevManager(),
                                 "Failed to initialize D3D device and manager",
                                 PLATFORM_FAILURE,
                                 false);
    device_manager_to_use = reinterpret_cast<ULONG_PTR>(device_manager_.get());
  }

  hr = decoder_->ProcessMessage(
            MFT_MESSAGE_SET_D3D_MANAGER,
            device_manager_to_use);
  if (use_dx11_) {
    RETURN_ON_HR_FAILURE(hr, "Failed to pass DX11 manager to decoder", false);
  } else {
    RETURN_ON_HR_FAILURE(hr, "Failed to pass D3D manager to decoder", false);
  }

  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();

  EGLint config_attribs[] = {
    EGL_BUFFER_SIZE, 32,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_ALPHA_SIZE, 0,
    EGL_NONE
  };

  EGLint num_configs;

  if (!eglChooseConfig(
      egl_display,
      config_attribs,
      &egl_config_,
      1,
      &num_configs))
    return false;

  return SetDecoderMediaTypes();
}

bool DXVAVideoDecodeAccelerator::CheckDecoderDxvaSupport() {
  base::win::ScopedComPtr<IMFAttributes> attributes;
  HRESULT hr = decoder_->GetAttributes(attributes.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get decoder attributes", false);

  UINT32 dxva = 0;
  hr = attributes->GetUINT32(MF_SA_D3D_AWARE, &dxva);
  RETURN_ON_HR_FAILURE(hr, "Failed to check if decoder supports DXVA", false);

  if (codec_ == media::kCodecH264) {
    hr = attributes->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, TRUE);
    RETURN_ON_HR_FAILURE(hr, "Failed to enable DXVA H/W decoding", false);
  }

  hr = attributes->SetUINT32(CODECAPI_AVLowLatencyMode, TRUE);
  if (SUCCEEDED(hr)) {
    DVLOG(1) << "Successfully set Low latency mode on decoder.";
  } else {
    DVLOG(1) << "Failed to set Low latency mode on decoder. Error: " << hr;
  }

  // The decoder should use DX11 iff
  // 1. The underlying H/W decoder supports it.
  // 2. We have a pointer to the MFCreateDXGIDeviceManager function needed for
  //    this. This should always be true for Windows 8+.
  // 3. ANGLE is using DX11.
  DCHECK(gl_context_);
  if (create_dxgi_device_manager_ &&
      (gl_context_->GetGLRenderer().find("Direct3D11") !=
           std::string::npos)) {
    UINT32 dx11_aware = 0;
    attributes->GetUINT32(MF_SA_D3D11_AWARE, &dx11_aware);
    use_dx11_ = !!dx11_aware;
  }

  use_keyed_mutex_ =
      use_dx11_ && gfx::GLSurfaceEGL::HasEGLExtension("EGL_ANGLE_keyed_mutex");

  return true;
}

bool DXVAVideoDecodeAccelerator::SetDecoderMediaTypes() {
  RETURN_ON_FAILURE(SetDecoderInputMediaType(),
                    "Failed to set decoder input media type", false);
  return SetDecoderOutputMediaType(MFVideoFormat_NV12);
}

bool DXVAVideoDecodeAccelerator::SetDecoderInputMediaType() {
  base::win::ScopedComPtr<IMFMediaType> media_type;
  HRESULT hr = MFCreateMediaType(media_type.Receive());
  RETURN_ON_HR_FAILURE(hr, "MFCreateMediaType failed", false);

  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Failed to set major input type", false);

  if (codec_ == media::kCodecH264) {
    hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  } else if (codec_ == media::kCodecVP8) {
    hr = media_type->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_VP80);
  } else if (codec_ == media::kCodecVP9) {
    hr = media_type->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_VP90);
  } else {
    NOTREACHED();
    RETURN_ON_FAILURE(false, "Unsupported codec on input media type.", false);
  }
  RETURN_ON_HR_FAILURE(hr, "Failed to set subtype", false);

  // Not sure about this. msdn recommends setting this value on the input
  // media type.
  hr = media_type->SetUINT32(MF_MT_INTERLACE_MODE,
                             MFVideoInterlace_MixedInterlaceOrProgressive);
  RETURN_ON_HR_FAILURE(hr, "Failed to set interlace mode", false);

  hr = decoder_->SetInputType(0, media_type.get(), 0);  // No flags
  RETURN_ON_HR_FAILURE(hr, "Failed to set decoder input type", false);
  return true;
}

bool DXVAVideoDecodeAccelerator::SetDecoderOutputMediaType(
    const GUID& subtype) {
  return SetTransformOutputType(decoder_.get(), subtype, 0, 0);
}

bool DXVAVideoDecodeAccelerator::SendMFTMessage(MFT_MESSAGE_TYPE msg,
                                                int32_t param) {
  HRESULT hr = decoder_->ProcessMessage(msg, param);
  return SUCCEEDED(hr);
}

// Gets the minimum buffer sizes for input and output samples. The MFT will not
// allocate buffer for input nor output, so we have to do it ourselves and make
// sure they're the correct size. We only provide decoding if DXVA is enabled.
bool DXVAVideoDecodeAccelerator::GetStreamsInfoAndBufferReqs() {
  HRESULT hr = decoder_->GetInputStreamInfo(0, &input_stream_info_);
  RETURN_ON_HR_FAILURE(hr, "Failed to get input stream info", false);

  hr = decoder_->GetOutputStreamInfo(0, &output_stream_info_);
  RETURN_ON_HR_FAILURE(hr, "Failed to get decoder output stream info", false);

  DVLOG(1) << "Input stream info: ";
  DVLOG(1) << "Max latency: " << input_stream_info_.hnsMaxLatency;
  if (codec_ == media::kCodecH264) {
    // There should be three flags, one for requiring a whole frame be in a
    // single sample, one for requiring there be one buffer only in a single
    // sample, and one that specifies a fixed sample size. (as in cbSize)
    CHECK_EQ(input_stream_info_.dwFlags, 0x7u);
  }

  DVLOG(1) << "Min buffer size: " << input_stream_info_.cbSize;
  DVLOG(1) << "Max lookahead: " << input_stream_info_.cbMaxLookahead;
  DVLOG(1) << "Alignment: " << input_stream_info_.cbAlignment;

  DVLOG(1) << "Output stream info: ";
  // The flags here should be the same and mean the same thing, except when
  // DXVA is enabled, there is an extra 0x100 flag meaning decoder will
  // allocate its own sample.
  DVLOG(1) << "Flags: "
          << std::hex << std::showbase << output_stream_info_.dwFlags;
  if (codec_ == media::kCodecH264) {
    CHECK_EQ(output_stream_info_.dwFlags, 0x107u);
  }
  DVLOG(1) << "Min buffer size: " << output_stream_info_.cbSize;
  DVLOG(1) << "Alignment: " << output_stream_info_.cbAlignment;
  return true;
}

void DXVAVideoDecodeAccelerator::DoDecode() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  // This function is also called from FlushInternal in a loop which could
  // result in the state transitioning to kStopped due to no decoded output.
  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE(
      (state == kNormal || state == kFlushing || state == kStopped),
          "DoDecode: not in normal/flushing/stopped state", ILLEGAL_STATE,);

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  DWORD status = 0;

  HRESULT hr = decoder_->ProcessOutput(0,  // No flags
                                       1,  // # of out streams to pull from
                                       &output_data_buffer,
                                       &status);
  IMFCollection* events = output_data_buffer.pEvents;
  if (events != NULL) {
    DVLOG(1) << "Got events from ProcessOuput, but discarding";
    events->Release();
  }
  if (FAILED(hr)) {
    // A stream change needs further ProcessInput calls to get back decoder
    // output which is why we need to set the state to stopped.
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      if (!SetDecoderOutputMediaType(MFVideoFormat_NV12)) {
        // Decoder didn't let us set NV12 output format. Not sure as to why
        // this can happen. Give up in disgust.
        NOTREACHED() << "Failed to set decoder output media type to NV12";
        SetState(kStopped);
      } else {
        DVLOG(1) << "Received output format change from the decoder."
                    " Recursively invoking DoDecode";
        DoDecode();
      }
      return;
    } else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      // No more output from the decoder. Stop playback.
      SetState(kStopped);
      return;
    } else {
      NOTREACHED() << "Unhandled error in DoDecode()";
      return;
    }
  }
  TRACE_EVENT_ASYNC_END0("gpu", "DXVAVideoDecodeAccelerator.Decoding", this);

  TRACE_COUNTER1("DXVA Decoding", "TotalPacketsBeforeDecode",
                 inputs_before_decode_);

  inputs_before_decode_ = 0;

  RETURN_AND_NOTIFY_ON_FAILURE(ProcessOutputSample(output_data_buffer.pSample),
      "Failed to process output sample.", PLATFORM_FAILURE,);
}

bool DXVAVideoDecodeAccelerator::ProcessOutputSample(IMFSample* sample) {
  RETURN_ON_FAILURE(sample, "Decode succeeded with NULL output sample", false);

  LONGLONG input_buffer_id = 0;
  RETURN_ON_HR_FAILURE(sample->GetSampleTime(&input_buffer_id),
                       "Failed to get input buffer id associated with sample",
                       false);

  {
    base::AutoLock lock(decoder_lock_);
    DCHECK(pending_output_samples_.empty());
    pending_output_samples_.push_back(
        PendingSampleInfo(input_buffer_id, sample));
  }

  if (pictures_requested_) {
    DVLOG(1) << "Waiting for picture slots from the client.";
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::ProcessPendingSamples,
                   weak_this_factory_.GetWeakPtr()));
    return true;
  }

  int width = 0;
  int height = 0;
  if (!GetVideoFrameDimensions(sample, &width, &height)) {
    RETURN_ON_FAILURE(false, "Failed to get D3D surface from output sample",
                      false);
  }

  // Go ahead and request picture buffers.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::RequestPictureBuffers,
                 weak_this_factory_.GetWeakPtr(),
                 width,
                 height));

  pictures_requested_ = true;
  return true;
}

void DXVAVideoDecodeAccelerator::ProcessPendingSamples() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  if (!output_picture_buffers_.size())
    return;

  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_.Run(),
      "Failed to make context current", PLATFORM_FAILURE,);

  OutputBuffers::iterator index;

  for (index = output_picture_buffers_.begin();
       index != output_picture_buffers_.end() &&
       OutputSamplesPresent();
       ++index) {
    if (index->second->available()) {
      PendingSampleInfo* pending_sample = NULL;
      {
        base::AutoLock lock(decoder_lock_);

        PendingSampleInfo& sample_info = pending_output_samples_.front();
        if (sample_info.picture_buffer_id != -1)
          continue;
        pending_sample = &sample_info;
      }

      int width = 0;
      int height = 0;
      if (!GetVideoFrameDimensions(pending_sample->output_sample.get(),
          &width, &height)) {
        RETURN_AND_NOTIFY_ON_FAILURE(false,
            "Failed to get D3D surface from output sample", PLATFORM_FAILURE,);
      }

      if (width != index->second->size().width() ||
          height != index->second->size().height()) {
        HandleResolutionChanged(width, height);
        return;
      }

      base::win::ScopedComPtr<IMFMediaBuffer> output_buffer;
      HRESULT hr = pending_sample->output_sample->GetBufferByIndex(
          0, output_buffer.Receive());
      RETURN_AND_NOTIFY_ON_HR_FAILURE(hr,
          "Failed to get buffer from output sample", PLATFORM_FAILURE,);

      base::win::ScopedComPtr<IDirect3DSurface9> surface;
      base::win::ScopedComPtr<ID3D11Texture2D> d3d11_texture;

      if (use_dx11_) {
        base::win::ScopedComPtr<IMFDXGIBuffer> dxgi_buffer;
        hr = dxgi_buffer.QueryFrom(output_buffer.get());
        RETURN_AND_NOTIFY_ON_HR_FAILURE(hr,
            "Failed to get DXGIBuffer from output sample", PLATFORM_FAILURE,);
        hr = dxgi_buffer->GetResource(
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(d3d11_texture.Receive()));
      } else {
        hr = MFGetService(output_buffer.get(), MR_BUFFER_SERVICE,
                          IID_PPV_ARGS(surface.Receive()));
      }
      RETURN_AND_NOTIFY_ON_HR_FAILURE(hr,
          "Failed to get surface from output sample", PLATFORM_FAILURE,);

      pending_sample->picture_buffer_id = index->second->id();

      RETURN_AND_NOTIFY_ON_FAILURE(
          index->second->CopyOutputSampleDataToPictureBuffer(
              this,
              surface.get(),
              d3d11_texture.get(),
              pending_sample->input_buffer_id),
          "Failed to copy output sample", PLATFORM_FAILURE,);

      index->second->set_available(false);
    }
  }
}

void DXVAVideoDecodeAccelerator::StopOnError(
  media::VideoDecodeAccelerator::Error error) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::StopOnError,
                   weak_this_factory_.GetWeakPtr(),
                   error));
    return;
  }

  if (client_)
    client_->NotifyError(error);
  client_ = NULL;

  if (GetState() != kUninitialized) {
    Invalidate();
  }
}

void DXVAVideoDecodeAccelerator::Invalidate() {
  if (GetState() == kUninitialized)
    return;

  decoder_thread_.Stop();
  weak_this_factory_.InvalidateWeakPtrs();
  output_picture_buffers_.clear();
  stale_output_picture_buffers_.clear();
  pending_output_samples_.clear();
  pending_input_buffers_.clear();
  decoder_.Release();

  if (use_dx11_) {
    if (video_format_converter_mft_.get()) {
      video_format_converter_mft_->ProcessMessage(
          MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
      video_format_converter_mft_.Release();
    }
    d3d11_device_context_.Release();
    d3d11_device_.Release();
    d3d11_device_manager_.Release();
    d3d11_query_.Release();
    dx11_video_format_converter_media_type_needs_init_ = true;
  } else {
    d3d9_.Release();
    d3d9_device_ex_.Release();
    device_manager_.Release();
    query_.Release();
  }

  SetState(kUninitialized);
}

void DXVAVideoDecodeAccelerator::NotifyInputBufferRead(int input_buffer_id) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (client_)
    client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
}

void DXVAVideoDecodeAccelerator::NotifyFlushDone() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (client_ && pending_flush_) {
    pending_flush_ = false;
    {
      base::AutoLock lock(decoder_lock_);
      sent_drain_message_ = false;
    }

    client_->NotifyFlushDone();
  }
}

void DXVAVideoDecodeAccelerator::NotifyResetDone() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (client_)
    client_->NotifyResetDone();
}

void DXVAVideoDecodeAccelerator::RequestPictureBuffers(int width, int height) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  // This task could execute after the decoder has been torn down.
  if (GetState() != kUninitialized && client_) {
    client_->ProvidePictureBuffers(
        kNumPictureBuffers,
        gfx::Size(width, height),
        GL_TEXTURE_2D);
  }
}

void DXVAVideoDecodeAccelerator::NotifyPictureReady(
    int picture_buffer_id,
    int input_buffer_id) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  // This task could execute after the decoder has been torn down.
  if (GetState() != kUninitialized && client_) {
    // TODO(henryhsu): Use correct visible size instead of (0, 0). We can't use
    // coded size here so use (0, 0) intentionally to have the client choose.
    media::Picture picture(picture_buffer_id, input_buffer_id,
                           gfx::Rect(0, 0), false);
    client_->PictureReady(picture);
  }
}

void DXVAVideoDecodeAccelerator::NotifyInputBuffersDropped() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (!client_)
    return;

  for (PendingInputs::iterator it = pending_input_buffers_.begin();
       it != pending_input_buffers_.end(); ++it) {
    LONGLONG input_buffer_id = 0;
    RETURN_ON_HR_FAILURE((*it)->GetSampleTime(&input_buffer_id),
                         "Failed to get buffer id associated with sample",);
    client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
  }
  pending_input_buffers_.clear();
}

void DXVAVideoDecodeAccelerator::DecodePendingInputBuffers() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state != kUninitialized),
      "Invalid state: " << state, ILLEGAL_STATE,);

  if (pending_input_buffers_.empty() || OutputSamplesPresent())
    return;

  PendingInputs pending_input_buffers_copy;
  std::swap(pending_input_buffers_, pending_input_buffers_copy);

  for (PendingInputs::iterator it = pending_input_buffers_copy.begin();
       it != pending_input_buffers_copy.end(); ++it) {
    DecodeInternal(*it);
  }
}

void DXVAVideoDecodeAccelerator::FlushInternal() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  // We allow only one output frame to be present at any given time. If we have
  // an output frame, then we cannot complete the flush at this time.
  if (OutputSamplesPresent())
    return;

  // First drain the pending input because once the drain message is sent below,
  // the decoder will ignore further input until it's drained.
  if (!pending_input_buffers_.empty()) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::DecodePendingInputBuffers,
                    base::Unretained(this)));
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::FlushInternal,
                    base::Unretained(this)));
    return;
  }

  {
    base::AutoLock lock(decoder_lock_);
    if (!sent_drain_message_) {
      RETURN_AND_NOTIFY_ON_FAILURE(SendMFTMessage(MFT_MESSAGE_COMMAND_DRAIN, 0),
                                   "Failed to send drain message",
                                   PLATFORM_FAILURE,);
      sent_drain_message_ = true;
    }
  }

  // Attempt to retrieve an output frame from the decoder. If we have one,
  // return and proceed when the output frame is processed. If we don't have a
  // frame then we are done.
  DoDecode();
  if (OutputSamplesPresent())
    return;

  SetState(kFlushing);

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::NotifyFlushDone,
                 weak_this_factory_.GetWeakPtr()));

  SetState(kNormal);
}

void DXVAVideoDecodeAccelerator::DecodeInternal(
    const base::win::ScopedComPtr<IMFSample>& sample) {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (GetState() == kUninitialized)
    return;

  if (OutputSamplesPresent() || !pending_input_buffers_.empty()) {
    pending_input_buffers_.push_back(sample);
    return;
  }

  if (!inputs_before_decode_) {
    TRACE_EVENT_ASYNC_BEGIN0("gpu", "DXVAVideoDecodeAccelerator.Decoding",
                             this);
  }
  inputs_before_decode_++;

  HRESULT hr = decoder_->ProcessInput(0, sample.get(), 0);
  // As per msdn if the decoder returns MF_E_NOTACCEPTING then it means that it
  // has enough data to produce one or more output samples. In this case the
  // recommended options are to
  // 1. Generate new output by calling IMFTransform::ProcessOutput until it
  //    returns MF_E_TRANSFORM_NEED_MORE_INPUT.
  // 2. Flush the input data
  // We implement the first option, i.e to retrieve the output sample and then
  // process the input again. Failure in either of these steps is treated as a
  // decoder failure.
  if (hr == MF_E_NOTACCEPTING) {
    DoDecode();
    // If the DoDecode call resulted in an output frame then we should not
    // process any more input until that frame is copied to the target surface.
    if (!OutputSamplesPresent()) {
      State state = GetState();
      RETURN_AND_NOTIFY_ON_FAILURE((state == kStopped || state == kNormal ||
                                    state == kFlushing),
          "Failed to process output. Unexpected decoder state: " << state,
          PLATFORM_FAILURE,);
      hr = decoder_->ProcessInput(0, sample.get(), 0);
    }
    // If we continue to get the MF_E_NOTACCEPTING error we do the following:-
    // 1. Add the input sample to the pending queue.
    // 2. If we don't have any output samples we post the
    //    DecodePendingInputBuffers task to process the pending input samples.
    //    If we have an output sample then the above task is posted when the
    //    output samples are sent to the client.
    // This is because we only support 1 pending output sample at any
    // given time due to the limitation with the Microsoft media foundation
    // decoder where it recycles the output Decoder surfaces.
    if (hr == MF_E_NOTACCEPTING) {
      pending_input_buffers_.push_back(sample);
      decoder_thread_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&DXVAVideoDecodeAccelerator::DecodePendingInputBuffers,
                      base::Unretained(this)));
      return;
    }
  }
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to process input sample",
      PLATFORM_FAILURE,);

  DoDecode();

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state == kStopped || state == kNormal ||
                                state == kFlushing),
      "Failed to process output. Unexpected decoder state: " << state,
      ILLEGAL_STATE,);

  LONGLONG input_buffer_id = 0;
  RETURN_ON_HR_FAILURE(sample->GetSampleTime(&input_buffer_id),
                       "Failed to get input buffer id associated with sample",);
  // The Microsoft Media foundation decoder internally buffers up to 30 frames
  // before returning a decoded frame. We need to inform the client that this
  // input buffer is processed as it may stop sending us further input.
  // Note: This may break clients which expect every input buffer to be
  // associated with a decoded output buffer.
  // TODO(ananta)
  // Do some more investigation into whether it is possible to get the MFT
  // decoder to emit an output packet for every input packet.
  // http://code.google.com/p/chromium/issues/detail?id=108121
  // http://code.google.com/p/chromium/issues/detail?id=150925
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::NotifyInputBufferRead,
                 weak_this_factory_.GetWeakPtr(),
                 input_buffer_id));
}

void DXVAVideoDecodeAccelerator::HandleResolutionChanged(int width,
                                                         int height) {
  dx11_video_format_converter_media_type_needs_init_ = true;

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::DismissStaleBuffers,
                 weak_this_factory_.GetWeakPtr()));

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::RequestPictureBuffers,
                 weak_this_factory_.GetWeakPtr(),
                 width,
                 height));
}

void DXVAVideoDecodeAccelerator::DismissStaleBuffers() {
  OutputBuffers::iterator index;

  for (index = output_picture_buffers_.begin();
       index != output_picture_buffers_.end();
       ++index) {
    if (index->second->available()) {
      DVLOG(1) << "Dismissing picture id: " << index->second->id();
      client_->DismissPictureBuffer(index->second->id());
    } else {
      // Move to |stale_output_picture_buffers_| for deferred deletion.
      stale_output_picture_buffers_.insert(
          std::make_pair(index->first, index->second));
    }
  }

  output_picture_buffers_.clear();
}

void DXVAVideoDecodeAccelerator::DeferredDismissStaleBuffer(
    int32_t picture_buffer_id) {
  OutputBuffers::iterator it = stale_output_picture_buffers_.find(
      picture_buffer_id);
  DCHECK(it != stale_output_picture_buffers_.end());
  DVLOG(1) << "Dismissing picture id: " << it->second->id();
  client_->DismissPictureBuffer(it->second->id());
  stale_output_picture_buffers_.erase(it);
}

DXVAVideoDecodeAccelerator::State
DXVAVideoDecodeAccelerator::GetState() {
  static_assert(sizeof(State) == sizeof(long), "mismatched type sizes");
  State state = static_cast<State>(
      InterlockedAdd(reinterpret_cast<volatile long*>(&state_), 0));
  return state;
}

void DXVAVideoDecodeAccelerator::SetState(State new_state) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::SetState,
                   weak_this_factory_.GetWeakPtr(),
                   new_state));
    return;
  }

  static_assert(sizeof(State) == sizeof(long), "mismatched type sizes");
  ::InterlockedExchange(reinterpret_cast<volatile long*>(&state_),
                        new_state);
  DCHECK_EQ(state_, new_state);
}

void DXVAVideoDecodeAccelerator::StartDecoderThread() {
  decoder_thread_.init_com_with_mta(false);
  decoder_thread_.Start();
  decoder_thread_task_runner_ = decoder_thread_.task_runner();
}

bool DXVAVideoDecodeAccelerator::OutputSamplesPresent() {
  base::AutoLock lock(decoder_lock_);
  return !pending_output_samples_.empty();
}

void DXVAVideoDecodeAccelerator::CopySurface(IDirect3DSurface9* src_surface,
                                             IDirect3DSurface9* dest_surface,
                                             int picture_buffer_id,
                                             int input_buffer_id) {
  if (!decoder_thread_task_runner_->BelongsToCurrentThread()) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::CopySurface,
                   base::Unretained(this),
                   src_surface,
                   dest_surface,
                   picture_buffer_id,
                   input_buffer_id));
    return;
  }

  HRESULT hr = d3d9_device_ex_->StretchRect(src_surface, NULL, dest_surface,
                                            NULL, D3DTEXF_NONE);
  RETURN_ON_HR_FAILURE(hr, "Colorspace conversion via StretchRect failed",);

  // Ideally, this should be done immediately before the draw call that uses
  // the texture. Flush it once here though.
  hr = query_->Issue(D3DISSUE_END);
  RETURN_ON_HR_FAILURE(hr, "Failed to issue END",);

  // If we are sharing the ANGLE device we don't need to wait for the Flush to
  // complete.
  if (using_angle_device_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::CopySurfaceComplete,
                   weak_this_factory_.GetWeakPtr(),
                   src_surface,
                   dest_surface,
                   picture_buffer_id,
                   input_buffer_id));
    return;
  }

  // Flush the decoder device to ensure that the decoded frame is copied to the
  // target surface.
  decoder_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::FlushDecoder,
                 base::Unretained(this), 0, src_surface, dest_surface,
                 picture_buffer_id, input_buffer_id),
      base::TimeDelta::FromMilliseconds(kFlushDecoderSurfaceTimeoutMs));
}

void DXVAVideoDecodeAccelerator::CopySurfaceComplete(
    IDirect3DSurface9* src_surface,
    IDirect3DSurface9* dest_surface,
    int picture_buffer_id,
    int input_buffer_id) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  // The output buffers may have changed in the following scenarios:-
  // 1. A resolution change.
  // 2. Decoder instance was destroyed.
  // Ignore copy surface notifications for such buffers.
  // copy surface notifications for such buffers.
  OutputBuffers::iterator it = output_picture_buffers_.find(picture_buffer_id);
  if (it == output_picture_buffers_.end())
    return;

  // If the picture buffer is marked as available it probably means that there
  // was a Reset operation which dropped the output frame.
  DXVAPictureBuffer* picture_buffer = it->second.get();
  if (picture_buffer->available())
    return;

  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_.Run(),
      "Failed to make context current", PLATFORM_FAILURE,);

  DCHECK(!output_picture_buffers_.empty());

  bool result = picture_buffer->CopySurfaceComplete(src_surface, dest_surface);
  RETURN_AND_NOTIFY_ON_FAILURE(result, "Failed to complete copying surface",
                               PLATFORM_FAILURE, );

  NotifyPictureReady(picture_buffer->id(), input_buffer_id);

  {
    base::AutoLock lock(decoder_lock_);
    if (!pending_output_samples_.empty())
      pending_output_samples_.pop_front();
  }

  if (pending_flush_) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::FlushInternal,
                   base::Unretained(this)));
    return;
  }
  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::DecodePendingInputBuffers,
                 base::Unretained(this)));
}

void DXVAVideoDecodeAccelerator::CopyTexture(
    ID3D11Texture2D* src_texture,
    ID3D11Texture2D* dest_texture,
    base::win::ScopedComPtr<IDXGIKeyedMutex> dest_keyed_mutex,
    uint64_t keyed_mutex_value,
    IMFSample* video_frame,
    int picture_buffer_id,
    int input_buffer_id) {
  HRESULT hr = E_FAIL;

  DCHECK(use_dx11_);

  if (!decoder_thread_task_runner_->BelongsToCurrentThread()) {
    // The media foundation H.264 decoder outputs YUV12 textures which we
    // cannot copy into ANGLE as they expect ARGB textures. In D3D land
    // the StretchRect API in the IDirect3DDevice9Ex interface did the color
    // space conversion for us. Sadly in DX11 land the API does not provide
    // a straightforward way to do this.
    // We use the video processor MFT.
    // https://msdn.microsoft.com/en-us/library/hh162913(v=vs.85).aspx
    // This object implements a media foundation transform (IMFTransform)
    // which follows the same contract as the decoder. The color space
    // conversion as per msdn is done in the GPU.

    D3D11_TEXTURE2D_DESC source_desc;
    src_texture->GetDesc(&source_desc);

    // Set up the input and output types for the video processor MFT.
    if (!InitializeDX11VideoFormatConverterMediaType(source_desc.Width,
                                                     source_desc.Height)) {
      RETURN_AND_NOTIFY_ON_FAILURE(
          false, "Failed to initialize media types for convesion.",
          PLATFORM_FAILURE,);
    }

    // The input to the video processor is the output sample.
    base::win::ScopedComPtr<IMFSample> input_sample_for_conversion;
    {
      base::AutoLock lock(decoder_lock_);
      PendingSampleInfo& sample_info = pending_output_samples_.front();
      input_sample_for_conversion = sample_info.output_sample;
    }

    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DXVAVideoDecodeAccelerator::CopyTexture,
                              base::Unretained(this), src_texture, dest_texture,
                              dest_keyed_mutex, keyed_mutex_value,
                              input_sample_for_conversion.Detach(),
                              picture_buffer_id, input_buffer_id));
    return;
  }

  DCHECK(video_frame);

  base::win::ScopedComPtr<IMFSample> input_sample;
  input_sample.Attach(video_frame);

  DCHECK(video_format_converter_mft_.get());

  if (dest_keyed_mutex) {
    HRESULT hr =
        dest_keyed_mutex->AcquireSync(keyed_mutex_value, kAcquireSyncWaitMs);
    RETURN_AND_NOTIFY_ON_FAILURE(
        hr == S_OK, "D3D11 failed to acquire keyed mutex for texture.",
        PLATFORM_FAILURE, );
  }
  // The video processor MFT requires output samples to be allocated by the
  // caller. We create a sample with a buffer backed with the ID3D11Texture2D
  // interface exposed by ANGLE. This works nicely as this ensures that the
  // video processor coverts the color space of the output frame and copies
  // the result into the ANGLE texture.
  base::win::ScopedComPtr<IMFSample> output_sample;
  hr = MFCreateSample(output_sample.Receive());
  if (FAILED(hr)) {
    RETURN_AND_NOTIFY_ON_HR_FAILURE(hr,
        "Failed to create output sample.", PLATFORM_FAILURE,);
  }

  base::win::ScopedComPtr<IMFMediaBuffer> output_buffer;
  hr = MFCreateDXGISurfaceBuffer(
      __uuidof(ID3D11Texture2D), dest_texture, 0, FALSE,
      output_buffer.Receive());
  if (FAILED(hr)) {
    base::debug::Alias(&hr);
    // TODO(ananta)
    // Remove this CHECK when the change to use DX11 for H/W decoding
    // stablizes.
    CHECK(false);
    RETURN_AND_NOTIFY_ON_HR_FAILURE(hr,
        "Failed to create output sample.", PLATFORM_FAILURE,);
  }

  output_sample->AddBuffer(output_buffer.get());

  hr = video_format_converter_mft_->ProcessInput(0, video_frame, 0);
  if (FAILED(hr)) {
    DCHECK(false);
    RETURN_AND_NOTIFY_ON_HR_FAILURE(hr,
        "Failed to convert output sample format.", PLATFORM_FAILURE,);
  }

  DWORD status = 0;
  MFT_OUTPUT_DATA_BUFFER format_converter_output = {};
  format_converter_output.pSample = output_sample.get();
  hr = video_format_converter_mft_->ProcessOutput(
        0,  // No flags
        1,  // # of out streams to pull from
        &format_converter_output,
        &status);

  if (FAILED(hr)) {
    base::debug::Alias(&hr);
    // TODO(ananta)
    // Remove this CHECK when the change to use DX11 for H/W decoding
    // stablizes.
    CHECK(false);
    RETURN_AND_NOTIFY_ON_HR_FAILURE(hr,
        "Failed to convert output sample format.", PLATFORM_FAILURE,);
  }

  if (dest_keyed_mutex) {
    HRESULT hr = dest_keyed_mutex->ReleaseSync(keyed_mutex_value + 1);
    RETURN_AND_NOTIFY_ON_FAILURE(hr == S_OK, "Failed to release keyed mutex.",
                                 PLATFORM_FAILURE, );

    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DXVAVideoDecodeAccelerator::CopySurfaceComplete,
                              weak_this_factory_.GetWeakPtr(), nullptr, nullptr,
                              picture_buffer_id, input_buffer_id));
  } else {
    d3d11_device_context_->Flush();
    d3d11_device_context_->End(d3d11_query_.get());

    decoder_thread_task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&DXVAVideoDecodeAccelerator::FlushDecoder,
                              base::Unretained(this), 0,
                              reinterpret_cast<IDirect3DSurface9*>(NULL),
                              reinterpret_cast<IDirect3DSurface9*>(NULL),
                              picture_buffer_id, input_buffer_id),
        base::TimeDelta::FromMilliseconds(kFlushDecoderSurfaceTimeoutMs));
  }
}

void DXVAVideoDecodeAccelerator::FlushDecoder(
    int iterations,
    IDirect3DSurface9* src_surface,
    IDirect3DSurface9* dest_surface,
    int picture_buffer_id,
    int input_buffer_id) {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  // The DXVA decoder has its own device which it uses for decoding. ANGLE
  // has its own device which we don't have access to.
  // The above code attempts to copy the decoded picture into a surface
  // which is owned by ANGLE. As there are multiple devices involved in
  // this, the StretchRect call above is not synchronous.
  // We attempt to flush the batched operations to ensure that the picture is
  // copied to the surface owned by ANGLE.
  // We need to do this in a loop and call flush multiple times.
  // We have seen the GetData call for flushing the command buffer fail to
  // return success occassionally on multi core machines, leading to an
  // infinite loop.
  // Workaround is to have an upper limit of 4 on the number of iterations to
  // wait for the Flush to finish.

  HRESULT hr = E_FAIL;
  if (use_dx11_) {
    BOOL query_data = 0;
    hr = d3d11_device_context_->GetData(d3d11_query_.get(), &query_data,
                                        sizeof(BOOL), 0);
    if (FAILED(hr)) {
      base::debug::Alias(&hr);
      // TODO(ananta)
      // Remove this CHECK when the change to use DX11 for H/W decoding
      // stablizes.
      CHECK(false);
    }
  } else {
    hr = query_->GetData(NULL, 0, D3DGETDATA_FLUSH);
  }

  if ((hr == S_FALSE) && (++iterations < kMaxIterationsForD3DFlush)) {
    decoder_thread_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DXVAVideoDecodeAccelerator::FlushDecoder,
                   base::Unretained(this), iterations, src_surface,
                   dest_surface, picture_buffer_id, input_buffer_id),
        base::TimeDelta::FromMilliseconds(kFlushDecoderSurfaceTimeoutMs));
    return;
  }

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::CopySurfaceComplete,
                 weak_this_factory_.GetWeakPtr(),
                 src_surface,
                 dest_surface,
                 picture_buffer_id,
                 input_buffer_id));
}

bool DXVAVideoDecodeAccelerator::InitializeDX11VideoFormatConverterMediaType(
    int width, int height) {
  if (!dx11_video_format_converter_media_type_needs_init_)
    return true;

  CHECK(video_format_converter_mft_.get());

  HRESULT hr = video_format_converter_mft_->ProcessMessage(
      MFT_MESSAGE_SET_D3D_MANAGER,
      reinterpret_cast<ULONG_PTR>(
          d3d11_device_manager_.get()));

  if (FAILED(hr)) {
    base::debug::Alias(&hr);
    // TODO(ananta)
    // Remove this CHECK when the change to use DX11 for H/W decoding
    // stablizes.
    CHECK(false);
  }
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr,
      "Failed to initialize video format converter", PLATFORM_FAILURE, false);

  video_format_converter_mft_->ProcessMessage(
      MFT_MESSAGE_NOTIFY_END_STREAMING, 0);

  base::win::ScopedComPtr<IMFMediaType> media_type;
  hr = MFCreateMediaType(media_type.Receive());
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "MFCreateMediaType failed",
      PLATFORM_FAILURE, false);

  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to set major input type",
      PLATFORM_FAILURE, false);

  hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to set input sub type",
      PLATFORM_FAILURE, false);

  hr = MFSetAttributeSize(media_type.get(), MF_MT_FRAME_SIZE, width, height);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to set media type attributes",
      PLATFORM_FAILURE, false);

  hr = video_format_converter_mft_->SetInputType(0, media_type.get(), 0);
  if (FAILED(hr)) {
    base::debug::Alias(&hr);
    // TODO(ananta)
    // Remove this CHECK when the change to use DX11 for H/W decoding
    // stablizes.
    CHECK(false);
  }
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to set converter input type",
      PLATFORM_FAILURE, false);

  // It appears that we fail to set MFVideoFormat_ARGB32 as the output media
  // type in certain configurations. Try to fallback to MFVideoFormat_RGB32
  // in such cases. If both fail, then bail.
  bool media_type_set =
      SetTransformOutputType(video_format_converter_mft_.get(),
                             MFVideoFormat_ARGB32,
                             width,
                             height);
  if (!media_type_set) {
    media_type_set =
        SetTransformOutputType(video_format_converter_mft_.get(),
                               MFVideoFormat_RGB32,
                               width,
                               height);
  }

  if (!media_type_set) {
    // Remove this once this stabilizes in the field.
    CHECK(false);
    LOG(ERROR) << "Failed to find a matching RGB output type in the converter";
    return false;
  }

  dx11_video_format_converter_media_type_needs_init_ = false;
  return true;
}

bool DXVAVideoDecodeAccelerator::GetVideoFrameDimensions(
    IMFSample* sample,
    int* width,
    int* height) {
  base::win::ScopedComPtr<IMFMediaBuffer> output_buffer;
  HRESULT hr = sample->GetBufferByIndex(0, output_buffer.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from output sample", false);

  if (use_dx11_) {
    base::win::ScopedComPtr<IMFDXGIBuffer> dxgi_buffer;
    base::win::ScopedComPtr<ID3D11Texture2D> d3d11_texture;
    hr = dxgi_buffer.QueryFrom(output_buffer.get());
    RETURN_ON_HR_FAILURE(hr, "Failed to get DXGIBuffer from output sample",
                         false);
    hr = dxgi_buffer->GetResource(
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(d3d11_texture.Receive()));
    RETURN_ON_HR_FAILURE(hr, "Failed to get D3D11Texture from output buffer",
                         false);
    D3D11_TEXTURE2D_DESC d3d11_texture_desc;
    d3d11_texture->GetDesc(&d3d11_texture_desc);
    *width = d3d11_texture_desc.Width;
    *height = d3d11_texture_desc.Height;
  } else {
    base::win::ScopedComPtr<IDirect3DSurface9> surface;
    hr = MFGetService(output_buffer.get(), MR_BUFFER_SERVICE,
                      IID_PPV_ARGS(surface.Receive()));
    RETURN_ON_HR_FAILURE(hr, "Failed to get D3D surface from output sample",
                         false);
    D3DSURFACE_DESC surface_desc;
    hr = surface->GetDesc(&surface_desc);
    RETURN_ON_HR_FAILURE(hr, "Failed to get surface description", false);
    *width = surface_desc.Width;
    *height = surface_desc.Height;
  }
  return true;
}

bool DXVAVideoDecodeAccelerator::SetTransformOutputType(
    IMFTransform* transform,
    const GUID& output_type,
    int width,
    int height) {
  HRESULT hr = E_FAIL;
  base::win::ScopedComPtr<IMFMediaType> media_type;

  for (uint32_t i = 0;
       SUCCEEDED(transform->GetOutputAvailableType(
           0, i, media_type.Receive()));
       ++i) {
    GUID out_subtype = {0};
    hr = media_type->GetGUID(MF_MT_SUBTYPE, &out_subtype);
    RETURN_ON_HR_FAILURE(hr, "Failed to get output major type", false);

    if (out_subtype == output_type) {
      if (width && height) {
        hr = MFSetAttributeSize(media_type.get(), MF_MT_FRAME_SIZE, width,
                                height);
        RETURN_ON_HR_FAILURE(hr, "Failed to set media type attributes", false);
      }
      hr = transform->SetOutputType(0, media_type.get(), 0);  // No flags
      if (FAILED(hr)) {
        base::debug::Alias(&hr);
        // TODO(ananta)
        // Remove this CHECK when this stabilizes in the field.
        CHECK(false);
      }
      RETURN_ON_HR_FAILURE(hr, "Failed to set output type", false);
      return true;
    }
    media_type.Release();
  }
  return false;
}

}  // namespace content
