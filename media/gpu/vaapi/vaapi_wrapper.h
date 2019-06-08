// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of VaapiWrapper, used by
// VaapiVideoDecodeAccelerator and VaapiH264Decoder for decode,
// and VaapiVideoEncodeAccelerator for encode, to interface
// with libva (VA-API library for hardware video codec).

#ifndef MEDIA_GPU_VAAPI_VAAPI_WRAPPER_H_
#define MEDIA_GPU_VAAPI_VAAPI_WRAPPER_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include <va/va.h>

#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11.h"
#endif  // USE_X11

namespace gfx {
class NativePixmap;
}

namespace media {

class ScopedVAImage;
class VideoFrame;

// This class handles VA-API calls and ensures proper locking of VA-API calls
// to libva, the userspace shim to the HW codec driver. libva is not
// thread-safe, so we have to perform locking ourselves. This class is fully
// synchronous and its methods can be called from any thread and may wait on
// the va_lock_ while other, concurrent calls run.
//
// This class is responsible for managing VAAPI connection, contexts and state.
// It is also responsible for managing and freeing VABuffers (not VASurfaces),
// which are used to queue parameters and slice data to the HW codec,
// as well as underlying memory for VASurfaces themselves.
class MEDIA_GPU_EXPORT VaapiWrapper
    : public base::RefCountedThreadSafe<VaapiWrapper> {
 public:
  enum CodecMode {
    kDecode,
    kEncode,
    kVideoProcess,
    kCodecModeMax,
  };

  using InternalFormats = struct {
    bool yuv420 : 1;
    bool yuv422 : 1;
    bool yuv444 : 1;
  };

  // Returns the VAAPI vendor string (obtained using vaQueryVendorString()).
  static const std::string& GetVendorStringForTesting();

  // Return an instance of VaapiWrapper initialized for |va_profile| and
  // |mode|. |report_error_to_uma_cb| will be called independently from
  // reporting errors to clients via method return values.
  static scoped_refptr<VaapiWrapper> Create(
      CodecMode mode,
      VAProfile va_profile,
      const base::Closure& report_error_to_uma_cb);

  // Create VaapiWrapper for VideoCodecProfile. It maps VideoCodecProfile
  // |profile| to VAProfile.
  // |report_error_to_uma_cb| will be called independently from reporting
  // errors to clients via method return values.
  static scoped_refptr<VaapiWrapper> CreateForVideoCodec(
      CodecMode mode,
      VideoCodecProfile profile,
      const base::Closure& report_error_to_uma_cb);

  // Return the supported video encode profiles.
  static VideoEncodeAccelerator::SupportedProfiles GetSupportedEncodeProfiles();

  // Return the supported video decode profiles.
  static VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles();

  // Return true when JPEG decode is supported.
  static bool IsJpegDecodeSupported();

  // Returns the supported internal formats for JPEG decoding. If JPEG decoding
  // is not supported, returns InternalFormats{}.
  static InternalFormats GetJpegDecodeSupportedInternalFormats();

  // Returns true if |rt_format| is supported for JPEG decoding. If it's not or
  // JPEG decoding is not supported, returns false.
  static bool IsJpegDecodingSupportedForInternalFormat(unsigned int rt_format);

  // Gets the minimum surface size allowed for JPEG decoding. Returns true if
  // the size can be obtained, false otherwise. If a dimension is not reported
  // by the driver, the dimension is returned as 0.
  static bool GetJpegDecodeMinResolution(gfx::Size* min_size);

  // Gets the maximum surface size allowed for JPEG decoding. Returns true if
  // the size can be obtained, false otherwise. Because of the initialization in
  // VASupportedProfiles::FillProfileInfo_Locked(), the size is guaranteed to
  // not be empty (as long as this method returns true).
  static bool GetJpegDecodeMaxResolution(gfx::Size* max_size);

  // Obtains a suitable FOURCC that can be used in vaCreateImage() +
  // vaGetImage(). |rt_format| corresponds to the JPEG's subsampling format.
  // |preferred_fourcc| is the FOURCC of the format preferred by the caller. If
  // it is determined that the VAAPI driver can do the conversion from the
  // internal format (|rt_format|), *|suitable_fourcc| is set to
  // |preferred_fourcc|. Otherwise, it is set to a supported format. Returns
  // true if a suitable FOURCC could be determined, false otherwise (e.g., if
  // the |rt_format| is unsupported by the driver). If |preferred_fourcc| is not
  // a supported image format, *|suitable_fourcc| is set to VA_FOURCC_I420.
  static bool GetJpegDecodeSuitableImageFourCC(unsigned int rt_format,
                                               uint32_t preferred_fourcc,
                                               uint32_t* suitable_fourcc);

  // Return true when JPEG encode is supported.
  static bool IsJpegEncodeSupported();

  // Return true when the specified image format is supported.
  static bool IsImageFormatSupported(const VAImageFormat& format);

  // Returns the list of VAImageFormats supported by the driver.
  static const std::vector<VAImageFormat>& GetSupportedImageFormatsForTesting();

  static uint32_t BufferFormatToVARTFormat(gfx::BufferFormat fmt);

  // Creates |num_surfaces| VASurfaceIDs of |va_format| and |size| and, if
  // successful, creates a |va_context_id_| of the same format and size. Returns
  // true if successful, with the created IDs in |va_surfaces|. The client is
  // responsible for destroying |va_surfaces| via DestroyContextAndSurfaces() to
  // free the allocated surfaces.
  virtual bool CreateContextAndSurfaces(unsigned int va_format,
                                        const gfx::Size& size,
                                        size_t num_surfaces,
                                        std::vector<VASurfaceID>* va_surfaces);
  // Releases the |va_surfaces| and destroys |va_context_id_|.
  virtual void DestroyContextAndSurfaces(std::vector<VASurfaceID> va_surfaces);

  // Creates a VA Context of |va_format| and |size|, and sets |va_context_id_|.
  // The client is responsible for releasing it via DestroyContext() or
  // DestroyContextAndSurfaces(), or it will be released on dtor.
  virtual bool CreateContext(unsigned int va_format, const gfx::Size& size);
  // Destroys the context identified by |va_context_id_| and clears the local
  // associated |va_surface_format_|.
  void DestroyContext();

  // Creates a self-releasing VASurface from |pixmap|. The ownership of the
  // surface is transferred to the caller.
  scoped_refptr<VASurface> CreateVASurfaceForPixmap(
      const scoped_refptr<gfx::NativePixmap>& pixmap);

  // Submit parameters or slice data of |va_buffer_type|, copying them from
  // |buffer| of size |size|, into HW codec. The data in |buffer| is no
  // longer needed and can be freed after this method returns.
  // Data submitted via this method awaits in the HW codec until
  // ExecuteAndDestroyPendingBuffers() is called to execute or
  // DestroyPendingBuffers() is used to cancel a pending job.
  bool SubmitBuffer(VABufferType va_buffer_type,
                    size_t size,
                    const void* buffer);

  // Convenient templatized version of SubmitBuffer() where |size| is deduced to
  // be the size of the type of |*buffer|.
  template <typename T>
  bool SubmitBuffer(VABufferType va_buffer_type, const T* buffer) {
    return SubmitBuffer(va_buffer_type, sizeof(T), buffer);
  }

  // Submit a VAEncMiscParameterBuffer of given |misc_param_type|, copying its
  // data from |buffer| of size |size|, into HW codec. The data in |buffer| is
  // no longer needed and can be freed after this method returns.
  // Data submitted via this method awaits in the HW codec until
  // ExecuteAndDestroyPendingBuffers() is called to execute or
  // DestroyPendingBuffers() is used to cancel a pending job.
  bool SubmitVAEncMiscParamBuffer(VAEncMiscParameterType misc_param_type,
                                  size_t size,
                                  const void* buffer);

  // Cancel and destroy all buffers queued to the HW codec via SubmitBuffer().
  // Useful when a pending job is to be cancelled (on reset or error).
  void DestroyPendingBuffers();

  // Execute job in hardware on target |va_surface_id| and destroy pending
  // buffers. Return false if Execute() fails.
  bool ExecuteAndDestroyPendingBuffers(VASurfaceID va_surface_id);

#if defined(USE_X11)
  // Put data from |va_surface_id| into |x_pixmap| of size
  // |dest_size|, converting/scaling to it.
  bool PutSurfaceIntoPixmap(VASurfaceID va_surface_id,
                            Pixmap x_pixmap,
                            gfx::Size dest_size);
#endif  // USE_X11

  // Creates a ScopedVAImage from a VASurface |va_surface_id| and map it into
  // memory with the given |format| and |size|. If |format| is not equal to the
  // internal format, the underlying implementation will do format conversion if
  // supported. |size| should be smaller than or equal to the surface. If |size|
  // is smaller, the image will be cropped.
  std::unique_ptr<ScopedVAImage> CreateVaImage(VASurfaceID va_surface_id,
                                               VAImageFormat* format,
                                               const gfx::Size& size);

  // Upload contents of |frame| into |va_surface_id| for encode.
  bool UploadVideoFrameToSurface(const VideoFrame& frame,
                                 VASurfaceID va_surface_id);

  // Create a buffer of |size| bytes to be used as encode output.
  bool CreateVABuffer(size_t size, VABufferID* buffer_id);

  // Download the contents of the buffer with given |buffer_id| into a buffer of
  // size |target_size|, pointed to by |target_ptr|. The number of bytes
  // downloaded will be returned in |coded_data_size|. |sync_surface_id| will
  // be used as a sync point, i.e. it will have to become idle before starting
  // the download. |sync_surface_id| should be the source surface passed
  // to the encode job.
  bool DownloadFromVABuffer(VABufferID buffer_id,
                            VASurfaceID sync_surface_id,
                            uint8_t* target_ptr,
                            size_t target_size,
                            size_t* coded_data_size);

  // See DownloadFromVABuffer() for details. After downloading, it deletes
  // the VA buffer with |buffer_id|.
  bool DownloadAndDestroyVABuffer(VABufferID buffer_id,
                                  VASurfaceID sync_surface_id,
                                  uint8_t* target_ptr,
                                  size_t target_size,
                                  size_t* coded_data_size);

  // Destroy all previously-allocated (and not yet destroyed) buffers.
  void DestroyVABuffers();

  // Get the max number of reference frames for encoding supported by the
  // driver.
  // For H.264 encoding, the value represents the maximum number of reference
  // frames for both the reference picture list 0 (bottom 16 bits) and the
  // reference picture list 1 (top 16 bits).
  bool GetVAEncMaxNumOfRefFrames(VideoCodecProfile profile,
                                 size_t* max_ref_frames);

  // Blits a VASurface |va_surface_src| into another VASurface
  // |va_surface_dest| applying pixel format conversion and scaling
  // if needed.
  bool BlitSurface(const scoped_refptr<VASurface>& va_surface_src,
                   const scoped_refptr<VASurface>& va_surface_dest);

  // Initialize static data before sandbox is enabled.
  static void PreSandboxInitialization();

  // Get the created surfaces format. TODO(crbug.com/971891): remove.
  unsigned int va_surface_format() const { return va_surface_format_; }

 protected:
  VaapiWrapper();
  virtual ~VaapiWrapper();

 private:
  friend class base::RefCountedThreadSafe<VaapiWrapper>;

  FRIEND_TEST_ALL_PREFIXES(VaapiUtilsTest, ScopedVAImage);
  FRIEND_TEST_ALL_PREFIXES(VaapiUtilsTest, BadScopedVAImage);
  FRIEND_TEST_ALL_PREFIXES(VaapiUtilsTest, BadScopedVABufferMapping);

  bool Initialize(CodecMode mode, VAProfile va_profile);
  void Deinitialize();
  bool VaInitialize(const base::Closure& report_error_to_uma_cb);

  // Tries to allocate |num_surfaces| VASurfaceIDs of |size| and |va_format|.
  // Fills |va_surfaces| and returns true if successful, or returns false.
  bool CreateSurfaces(unsigned int va_format,
                      const gfx::Size& size,
                      size_t num_surfaces,
                      std::vector<VASurfaceID>* va_surfaces);
  // vaDestroySurfaces() a vector or a single VASurfaceID.
  void DestroySurfaces(std::vector<VASurfaceID> va_surfaces);
  void DestroySurface(VASurfaceID va_surface_id);

  // Execute pending job in hardware and destroy pending buffers. Return false
  // if vaapi driver refuses to accept parameter or slice buffers submitted
  // by client, or if execution fails in hardware.
  bool Execute(VASurfaceID va_surface_id);

  // Attempt to set render mode to "render to texture.". Failure is non-fatal.
  void TryToSetVADisplayAttributeToLocalGPU();

  // Check low-power encode support for the given profile
  bool IsLowPowerEncSupported(VAProfile va_profile) const;

  // Pointer to VADisplayState's member |va_lock_|. Guaranteed to be valid for
  // the lifetime of VaapiWrapper.
  base::Lock* va_lock_;

  // VA format of allocated surfaces. TODO(crbug.com/971891): remove.
  unsigned int va_surface_format_;

  // VA handles.
  // All valid after successful Initialize() and until Deinitialize().
  VADisplay va_display_ GUARDED_BY(va_lock_);
  VAConfigID va_config_id_;
  // Created in CreateContext() or CreateContextAndSurfaces() and valid until
  // DestroyContext() or DestroyContextAndSurfaces().
  VAContextID va_context_id_;

  // Data queued up for HW codec, to be committed on next execution.
  std::vector<VABufferID> pending_slice_bufs_;
  std::vector<VABufferID> pending_va_bufs_;

  // Buffers for kEncode or kVideoProcess.
  std::set<VABufferID> va_buffers_;

  // Called to report codec errors to UMA. Errors to clients are reported via
  // return values from public methods.
  base::Closure report_error_to_uma_cb_;

  DISALLOW_COPY_AND_ASSIGN(VaapiWrapper);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_WRAPPER_H_
