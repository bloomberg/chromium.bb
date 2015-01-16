// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/dxva_video_decode_accelerator.h"

#if !defined(OS_WIN)
#error This file should only be built on Windows.
#endif   // !defined(OS_WIN)

#include <ks.h>
#include <codecapi.h>
#include <mfapi.h>
#include <mferror.h>
#include <wmcodecdsp.h>

#include "base/base_paths_win.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/win/windows_version.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gl/gl_bindings.h"
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

}

namespace content {

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

  return sample.Detach();
}

// Creates a Media Foundation sample with one buffer containing a copy of the
// given Annex B stream data.
// If duration and sample time are not known, provide 0.
// |min_size| specifies the minimum size of the buffer (might be required by
// the decoder for input). If no alignment is required, provide 0.
static IMFSample* CreateInputSample(const uint8* stream, int size,
                                    int min_size, int alignment) {
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
  uint8* destination = NULL;
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

  return CreateInputSample(reinterpret_cast<const uint8*>(shm.memory()),
                           bitstream_buffer.size(),
                           stream_size,
                           alignment);
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

  void ReusePictureBuffer();
  // Copies the output sample data to the picture buffer provided by the
  // client.
  // The dest_surface parameter contains the decoded bits.
  bool CopyOutputSampleDataToPictureBuffer(
      DXVAVideoDecodeAccelerator* decoder,
      IDirect3DSurface9* dest_surface,
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
  void CopySurfaceComplete(IDirect3DSurface9* src_surface,
                           IDirect3DSurface9* dest_surface);

 private:
  explicit DXVAPictureBuffer(const media::PictureBuffer& buffer);

  bool available_;
  media::PictureBuffer picture_buffer_;
  EGLSurface decoding_surface_;
  base::win::ScopedComPtr<IDirect3DTexture9> decoding_texture_;

  // The following |IDirect3DSurface9| interface pointers are used to hold
  // references on the surfaces during the course of a StretchRect operation
  // to copy the source surface to the target. The references are released
  // when the StretchRect operation i.e. the copy completes.
  base::win::ScopedComPtr<IDirect3DSurface9> decoder_surface_;
  base::win::ScopedComPtr<IDirect3DSurface9> target_surface_;

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

  EGLint attrib_list[] = {
    EGL_WIDTH, buffer.size().width(),
    EGL_HEIGHT, buffer.size().height(),
    EGL_TEXTURE_FORMAT, use_rgb ? EGL_TEXTURE_RGB : EGL_TEXTURE_RGBA,
    EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
    EGL_NONE
  };

  picture_buffer->decoding_surface_ = eglCreatePbufferSurface(
      egl_display,
      egl_config,
      attrib_list);
  RETURN_ON_FAILURE(picture_buffer->decoding_surface_,
                    "Failed to create surface",
                    linked_ptr<DXVAPictureBuffer>(NULL));

  HANDLE share_handle = NULL;
  EGLBoolean ret = eglQuerySurfacePointerANGLE(
      egl_display,
      picture_buffer->decoding_surface_,
      EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
      &share_handle);

  RETURN_ON_FAILURE(share_handle && ret == EGL_TRUE,
                    "Failed to query ANGLE surface pointer",
                    linked_ptr<DXVAPictureBuffer>(NULL));

  // TODO(dshwang): after moving to D3D11, use RGBA surface. crbug.com/438691
  HRESULT hr = decoder.device_->CreateTexture(
      buffer.size().width(),
      buffer.size().height(),
      1,
      D3DUSAGE_RENDERTARGET,
      use_rgb ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8,
      D3DPOOL_DEFAULT,
      picture_buffer->decoding_texture_.Receive(),
      &share_handle);

  RETURN_ON_HR_FAILURE(hr, "Failed to create texture",
                       linked_ptr<DXVAPictureBuffer>(NULL));
  picture_buffer->use_rgb_ = !!use_rgb;
  return picture_buffer;
}

DXVAVideoDecodeAccelerator::DXVAPictureBuffer::DXVAPictureBuffer(
    const media::PictureBuffer& buffer)
    : available_(true),
      picture_buffer_(buffer),
      decoding_surface_(NULL),
      use_rgb_(true) {
}

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

void DXVAVideoDecodeAccelerator::DXVAPictureBuffer::ReusePictureBuffer() {
  DCHECK(decoding_surface_);
  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();
  eglReleaseTexImage(
    egl_display,
    decoding_surface_,
    EGL_BACK_BUFFER);
  decoder_surface_.Release();
  target_surface_.Release();
  set_available(true);
}

bool DXVAVideoDecodeAccelerator::DXVAPictureBuffer::
    CopyOutputSampleDataToPictureBuffer(
        DXVAVideoDecodeAccelerator* decoder,
        IDirect3DSurface9* dest_surface,
        int input_buffer_id) {
  DCHECK(dest_surface);

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

void DXVAVideoDecodeAccelerator::DXVAPictureBuffer::CopySurfaceComplete(
    IDirect3DSurface9* src_surface,
    IDirect3DSurface9* dest_surface) {
  DCHECK(!available());

  GLint current_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

  glBindTexture(GL_TEXTURE_2D, picture_buffer_.texture_id());

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  DCHECK_EQ(src_surface, decoder_surface_.get());
  DCHECK_EQ(dest_surface, target_surface_.get());

  decoder_surface_.Release();
  target_surface_.Release();

  EGLDisplay egl_display = gfx::GLSurfaceEGL::GetHardwareDisplay();
  eglBindTexImage(
      egl_display,
      decoding_surface_,
      EGL_BACK_BUFFER);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, current_texture);
}

DXVAVideoDecodeAccelerator::PendingSampleInfo::PendingSampleInfo(
    int32 buffer_id, IMFSample* sample)
    : input_buffer_id(buffer_id),
      picture_buffer_id(-1) {
  output_sample.Attach(sample);
}

DXVAVideoDecodeAccelerator::PendingSampleInfo::~PendingSampleInfo() {}

// static
bool DXVAVideoDecodeAccelerator::CreateD3DDevManager() {
  TRACE_EVENT0("gpu", "DXVAVideoDecodeAccelerator_CreateD3DDevManager");

  HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, d3d9_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Direct3DCreate9Ex failed", false);

  D3DPRESENT_PARAMETERS present_params = {0};
  present_params.BackBufferWidth = 1;
  present_params.BackBufferHeight = 1;
  present_params.BackBufferFormat = D3DFMT_UNKNOWN;
  present_params.BackBufferCount = 1;
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = ::GetShellWindow();
  present_params.Windowed = TRUE;
  present_params.Flags = D3DPRESENTFLAG_VIDEO;
  present_params.FullScreen_RefreshRateInHz = 0;
  present_params.PresentationInterval = 0;

  hr = d3d9_->CreateDeviceEx(D3DADAPTER_DEFAULT,
                             D3DDEVTYPE_HAL,
                             ::GetShellWindow(),
                             D3DCREATE_FPU_PRESERVE |
                             D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                             D3DCREATE_DISABLE_PSGP_THREADING |
                             D3DCREATE_MULTITHREADED,
                             &present_params,
                             NULL,
                             device_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to create D3D device", false);

  hr = DXVA2CreateDirect3DDeviceManager9(&dev_manager_reset_token_,
                                         device_manager_.Receive());
  RETURN_ON_HR_FAILURE(hr, "DXVA2CreateDirect3DDeviceManager9 failed", false);

  hr = device_manager_->ResetDevice(device_.get(), dev_manager_reset_token_);
  RETURN_ON_HR_FAILURE(hr, "Failed to reset device", false);

  hr = device_->CreateQuery(D3DQUERYTYPE_EVENT, query_.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to create D3D device query", false);
  // Ensure query_ API works (to avoid an infinite loop later in
  // CopyOutputSampleDataToPictureBuffer).
  hr = query_->Issue(D3DISSUE_END);
  RETURN_ON_HR_FAILURE(hr, "Failed to issue END test query", false);
  return true;
}

DXVAVideoDecodeAccelerator::DXVAVideoDecodeAccelerator(
    const base::Callback<bool(void)>& make_context_current)
    : client_(NULL),
      dev_manager_reset_token_(0),
      egl_config_(NULL),
      state_(kUninitialized),
      pictures_requested_(false),
      inputs_before_decode_(0),
      sent_drain_message_(false),
      make_context_current_(make_context_current),
      codec_(media::kUnknownVideoCodec),
      decoder_thread_("DXVAVideoDecoderThread"),
      weak_this_factory_(this),
      weak_ptr_(weak_this_factory_.GetWeakPtr()),
      pending_flush_(false) {
  memset(&input_stream_info_, 0, sizeof(input_stream_info_));
  memset(&output_stream_info_, 0, sizeof(output_stream_info_));
}

DXVAVideoDecodeAccelerator::~DXVAVideoDecodeAccelerator() {
  client_ = NULL;
}

bool DXVAVideoDecodeAccelerator::Initialize(media::VideoCodecProfile profile,
                                           Client* client) {
  client_ = client;

  main_thread_task_runner_ = base::MessageLoop::current()->task_runner();

  // Not all versions of Windows 7 and later include Media Foundation DLLs.
  // Instead of crashing while delay loading the DLL when calling MFStartup()
  // below, probe whether we can successfully load the DLL now.
  //
  // See http://crbug.com/339678 for details.
  HMODULE mfplat_dll = ::LoadLibrary(L"MFPlat.dll");
  RETURN_ON_FAILURE(mfplat_dll, "MFPlat.dll is required for decoding", false);

  if (profile != media::H264PROFILE_BASELINE &&
      profile != media::H264PROFILE_MAIN &&
      profile != media::H264PROFILE_HIGH &&
      profile != media::VP8PROFILE_ANY &&
      profile != media::VP9PROFILE_ANY) {
    RETURN_AND_NOTIFY_ON_FAILURE(false,
        "Unsupported h.264, vp8, or vp9 profile", PLATFORM_FAILURE, false);
  }

  RETURN_AND_NOTIFY_ON_FAILURE(
      gfx::g_driver_egl.ext.b_EGL_ANGLE_surface_d3d_texture_2d_share_handle,
      "EGL_ANGLE_surface_d3d_texture_2d_share_handle unavailable",
      PLATFORM_FAILURE,
      false);

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state == kUninitialized),
      "Initialize: invalid state: " << state, ILLEGAL_STATE, false);

  HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "MFStartup failed.", PLATFORM_FAILURE,
      false);

  RETURN_AND_NOTIFY_ON_FAILURE(CreateD3DDevManager(),
                               "Failed to initialize D3D device and manager",
                               PLATFORM_FAILURE,
                               false);

  RETURN_AND_NOTIFY_ON_FAILURE(InitDecoder(profile),
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
  RETURN_AND_NOTIFY_ON_FAILURE((kNumPictureBuffers == buffers.size()),
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

void DXVAVideoDecodeAccelerator::ReusePictureBuffer(
    int32 picture_buffer_id) {
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

  it->second->ReusePictureBuffer();
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

bool DXVAVideoDecodeAccelerator::InitDecoder(media::VideoCodecProfile profile) {
  HMODULE decoder_dll = NULL;

  // Profile must fall within the valid range for one of the supported codecs.
  if (profile >= media::H264PROFILE_MIN && profile <= media::H264PROFILE_MAX) {
    // We mimic the steps CoCreateInstance uses to instantiate the object. This
    // was previously done because it failed inside the sandbox, and now is done
    // as a more minimal approach to avoid other side-effects CCI might have (as
    // we are still in a reduced sandbox).
    decoder_dll = ::LoadLibrary(L"msmpeg2vdec.dll");
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
  } else if (profile == media::VP8PROFILE_ANY ||
             profile == media::VP9PROFILE_ANY) {
    base::FilePath dll_path;
    RETURN_ON_FAILURE(PathService::Get(base::DIR_PROGRAM_FILES, &dll_path),
        "failed to get path for DIR_PROGRAM_FILES", false);
    dll_path = dll_path.Append(kVPXDecoderDLLPath);
    if (profile == media::VP8PROFILE_ANY) {
      codec_ = media::kCodecVP8;
      dll_path = dll_path.Append(kVP8DecoderDLLName);
    } else {
      codec_ = media::kCodecVP9;
      dll_path = dll_path.Append(kVP9DecoderDLLName);
    }
    decoder_dll = ::LoadLibraryEx(dll_path.value().data(), NULL,
        LOAD_WITH_ALTERED_SEARCH_PATH);
    RETURN_ON_FAILURE(decoder_dll, "vpx decoder dll is not loaded", false);
  } else {
    RETURN_ON_FAILURE(false, "Unsupported codec.", false);
  }

  typedef HRESULT(WINAPI * GetClassObject)(
      const CLSID & clsid, const IID & iid, void * *object);

  GetClassObject get_class_object = reinterpret_cast<GetClassObject>(
      GetProcAddress(decoder_dll, "DllGetClassObject"));
  RETURN_ON_FAILURE(
      get_class_object, "Failed to get DllGetClassObject pointer", false);

  base::win::ScopedComPtr<IClassFactory> factory;
  HRESULT hr;
  if (codec_ == media::kCodecH264) {
    hr  = get_class_object(__uuidof(CMSH264DecoderMFT),
                           __uuidof(IClassFactory),
                           reinterpret_cast<void**>(factory.Receive()));
  } else if (codec_ == media::kCodecVP8) {
    hr  = get_class_object(CLSID_WebmMfVp8Dec,
                           __uuidof(IClassFactory),
                           reinterpret_cast<void**>(factory.Receive()));
  } else if (codec_ == media::kCodecVP9) {
    hr  = get_class_object(CLSID_WebmMfVp9Dec,
                           __uuidof(IClassFactory),
                           reinterpret_cast<void**>(factory.Receive()));
  } else {
    RETURN_ON_FAILURE(false, "Unsupported codec.", false);
  }
  RETURN_ON_HR_FAILURE(hr, "DllGetClassObject for decoder failed", false);

  hr = factory->CreateInstance(NULL,
                               __uuidof(IMFTransform),
                               reinterpret_cast<void**>(decoder_.Receive()));
  RETURN_ON_HR_FAILURE(hr, "Failed to create decoder instance", false);

  RETURN_ON_FAILURE(CheckDecoderDxvaSupport(),
                    "Failed to check decoder DXVA support", false);

  hr = decoder_->ProcessMessage(
            MFT_MESSAGE_SET_D3D_MANAGER,
            reinterpret_cast<ULONG_PTR>(device_manager_.get()));
  RETURN_ON_HR_FAILURE(hr, "Failed to pass D3D manager to decoder", false);

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
  base::win::ScopedComPtr<IMFMediaType> out_media_type;

  for (uint32 i = 0;
       SUCCEEDED(decoder_->GetOutputAvailableType(0, i,
                                                  out_media_type.Receive()));
       ++i) {
    GUID out_subtype = {0};
    HRESULT hr = out_media_type->GetGUID(MF_MT_SUBTYPE, &out_subtype);
    RETURN_ON_HR_FAILURE(hr, "Failed to get output major type", false);

    if (out_subtype == subtype) {
      hr = decoder_->SetOutputType(0, out_media_type.get(), 0);  // No flags
      RETURN_ON_HR_FAILURE(hr, "Failed to set decoder output type", false);
      return true;
    }
    out_media_type.Release();
  }
  return false;
}

bool DXVAVideoDecodeAccelerator::SendMFTMessage(MFT_MESSAGE_TYPE msg,
                                                int32 param) {
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
  TRACE_EVENT_END_ETW("DXVAVideoDecodeAccelerator.Decoding", this, "");

  TRACE_COUNTER1("DXVA Decoding", "TotalPacketsBeforeDecode",
                 inputs_before_decode_);

  inputs_before_decode_ = 0;

  RETURN_AND_NOTIFY_ON_FAILURE(ProcessOutputSample(output_data_buffer.pSample),
      "Failed to process output sample.", PLATFORM_FAILURE,);
}

bool DXVAVideoDecodeAccelerator::ProcessOutputSample(IMFSample* sample) {
  RETURN_ON_FAILURE(sample, "Decode succeeded with NULL output sample", false);

  base::win::ScopedComPtr<IMFMediaBuffer> output_buffer;
  HRESULT hr = sample->GetBufferByIndex(0, output_buffer.Receive());
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from output sample", false);

  base::win::ScopedComPtr<IDirect3DSurface9> surface;
  hr = MFGetService(output_buffer.get(), MR_BUFFER_SERVICE,
                    IID_PPV_ARGS(surface.Receive()));
  RETURN_ON_HR_FAILURE(hr, "Failed to get D3D surface from output sample",
                       false);

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

  // We only read the surface description, which contains its width/height when
  // we need the picture buffers from the client. Once we have those, then they
  // are reused.
  D3DSURFACE_DESC surface_desc;
  hr = surface->GetDesc(&surface_desc);
  RETURN_ON_HR_FAILURE(hr, "Failed to get surface description", false);

  // Go ahead and request picture buffers.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DXVAVideoDecodeAccelerator::RequestPictureBuffers,
                 weak_this_factory_.GetWeakPtr(),
                 surface_desc.Width,
                 surface_desc.Height));

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

      base::win::ScopedComPtr<IMFMediaBuffer> output_buffer;
      HRESULT hr = pending_sample->output_sample->GetBufferByIndex(
          0, output_buffer.Receive());
      RETURN_AND_NOTIFY_ON_HR_FAILURE(
          hr, "Failed to get buffer from output sample", PLATFORM_FAILURE,);

      base::win::ScopedComPtr<IDirect3DSurface9> surface;
      hr = MFGetService(output_buffer.get(), MR_BUFFER_SERVICE,
                        IID_PPV_ARGS(surface.Receive()));
      RETURN_AND_NOTIFY_ON_HR_FAILURE(
          hr, "Failed to get D3D surface from output sample",
          PLATFORM_FAILURE,);

      D3DSURFACE_DESC surface_desc;
      hr = surface->GetDesc(&surface_desc);
      RETURN_AND_NOTIFY_ON_HR_FAILURE(
          hr, "Failed to get surface description", PLATFORM_FAILURE,);

      if (surface_desc.Width !=
              static_cast<uint32>(index->second->size().width()) ||
          surface_desc.Height !=
              static_cast<uint32>(index->second->size().height())) {
        HandleResolutionChanged(surface_desc.Width, surface_desc.Height);
        return;
      }

      pending_sample->picture_buffer_id = index->second->id();

      RETURN_AND_NOTIFY_ON_FAILURE(
          index->second->CopyOutputSampleDataToPictureBuffer(
              this,
              surface.get(),
              pending_sample->input_buffer_id),
          "Failed to copy output sample", PLATFORM_FAILURE, );

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
  MFShutdown();
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
    int input_buffer_id,
    const gfx::Rect& picture_buffer_size) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  // This task could execute after the decoder has been torn down.
  if (GetState() != kUninitialized && client_) {
    media::Picture picture(picture_buffer_id,
                           input_buffer_id,
                           picture_buffer_size);
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
    TRACE_EVENT_BEGIN_ETW("DXVAVideoDecodeAccelerator.Decoding", this, "");
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
    int32 picture_buffer_id) {
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

  HRESULT hr = device_->StretchRect(src_surface, NULL, dest_surface,
                                    NULL, D3DTEXF_NONE);
  RETURN_ON_HR_FAILURE(hr, "Colorspace conversion via StretchRect failed",);

  // Ideally, this should be done immediately before the draw call that uses
  // the texture. Flush it once here though.
  hr = query_->Issue(D3DISSUE_END);
  RETURN_ON_HR_FAILURE(hr, "Failed to issue END",);

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

  picture_buffer->CopySurfaceComplete(src_surface,
                                      dest_surface);

  NotifyPictureReady(picture_buffer->id(),
                     input_buffer_id,
                     gfx::Rect(picture_buffer->size()));

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
  HRESULT hr = query_->GetData(NULL, 0, D3DGETDATA_FLUSH);
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

}  // namespace content
