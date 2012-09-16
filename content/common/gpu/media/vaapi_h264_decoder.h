// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of a class that provides H264 decode
// support for use with VAAPI hardware video decode acceleration on Intel
// systems.

#ifndef CONTENT_COMMON_GPU_MEDIA_VAAPI_H264_DECODER_H_
#define CONTENT_COMMON_GPU_MEDIA_VAAPI_H264_DECODER_H_

#include <GL/glx.h>

#include <queue>

#include "base/callback_forward.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/media/h264_dpb.h"
#include "content/common/gpu/media/h264_parser.h"
#include "media/base/video_decoder_config.h"
#include "media/base/limits.h"
#include "third_party/libva/va/va.h"

namespace content {

// An H264 decoder for use for VA-API-specific decoding. Provides features not
// supported by libva, including stream parsing, reference picture management
// and other operations not supported by the HW codec.
//
// Provides functionality to allow plugging VAAPI HW acceleration into the
// VDA framework.
//
// Clients of this class are expected to pass H264 Annex-B byte stream and
// will receive decoded pictures via client-provided |OutputPicCB|.
//
// If used in multi-threaded environment, some of the functions have to be
// called on the child thread, i.e. the main thread of the GPU process
// (the one that has the GLX context passed to Initialize() set as current).
// This is essential so that the GLX calls can work properly.
// Decoder thread, on the other hand, does not require a GLX context and should
// be the same as the one on which Decode*() functions are called.
class VaapiH264Decoder {
 public:
  // Callback invoked on the client when a picture is to be displayed.
  // Arguments: input buffer id, output buffer id (both provided by the client
  // at the time of Decode() and AssignPictureBuffer() calls).
  typedef base::Callback<void(int32, int32)> OutputPicCB;

  // Callback invoked on the client to start a GPU job to put a decoded picture
  // into a pixmap/texture. Callee has to call PutPicToTexture() for the given
  // picture.
  // Argument: output buffer id (provided by the client at the time of
  // AssignPictureBuffer() call).
  typedef base::Callback<void(int32)> SyncPicCB;

  // Decode result codes.
  enum DecResult {
    kDecodeError,  // Error while decoding.
    // TODO posciak: unsupported streams are currently treated as error
    // in decoding; in future it could perhaps be possible to fall back
    // to software decoding instead.
    // kStreamError,  // Error in stream.
    kReadyToDecode,  // Successfully initialized.
    kDecodedFrame,  // Successfully decoded a frame.
    kNeedMoreStreamData,  // Need more stream data to decode the next frame.
    kNoOutputAvailable,  // Waiting for the client to free up output surfaces.
  };

  VaapiH264Decoder();
  // Should be called on the GLX thread, for the surface cleanup to work
  // properly.
  ~VaapiH264Decoder();

  // Initializes and sets up libva connection and GL/X11 resources.
  // Must be called on the GLX thread with |glx_context| being current and
  // with decoder thread not yet running.
  // |output_pic_cb| will be called to notify when a picture can be displayed.
  bool Initialize(media::VideoCodecProfile profile,
                  Display* x_display,
                  GLXContext glx_context,
                  const base::Callback<bool(void)>& make_context_current,
                  const OutputPicCB& output_pic_cb,
                  const SyncPicCB& sync_pic_cb) WARN_UNUSED_RESULT;
  void Destroy();

  // Notify the decoder that this output buffer has been consumed and
  // can be reused (overwritten).
  // Must be run on the decoder thread.
  void ReusePictureBuffer(int32 picture_buffer_id);

  // Give a new picture buffer (texture) to decoder for use.
  // Must be run on the GLX thread with decoder thread not yet running.
  bool AssignPictureBuffer(int32 picture_buffer_id, uint32 texture_id)
      WARN_UNUSED_RESULT;

  // Sync the data so that the texture for given |picture_buffer_id| can
  // be displayed.
  // Must be run on the GLX thread.
  bool PutPicToTexture(int32 picture_buffer_id) WARN_UNUSED_RESULT;

  // Have the decoder flush its state and trigger output of all previously
  // decoded pictures via OutputPicCB.
  // Returns false if any of the resulting invocations of the callback fail.
  bool Flush() WARN_UNUSED_RESULT;

  // Called while decoding.
  // Stop decoding, discarding all remaining input/output, but do not flush
  // state, so the playback of the same stream can be resumed (possibly from
  // another location).
  void Reset();

  // Set current stream data pointer to |ptr| and |size|.
  // Must be run on decoder thread.
  void SetStream(uint8* ptr, size_t size);

  // Start parsing stream to detect picture sizes. Does not produce any
  // decoded pictures and can be called without providing output textures.
  // Also to be used after Reset() to find a suitable location in the
  // stream to resume playback from.
  DecResult DecodeInitial(int32 input_id) WARN_UNUSED_RESULT;

  // Runs until a frame is decoded or end of provided stream data buffer
  // is reached. Decoded pictures will be returned asynchronously via
  // OutputPicCB.
  DecResult DecodeOneFrame(int32 input_id) WARN_UNUSED_RESULT;

  // Return dimensions for output buffer (texture) allocation.
  // Valid only after a successful DecodeInitial().
  int pic_height() { return pic_height_; }
  int pic_width() { return pic_width_; }

  // Return the number of output pictures required for decoding.
  // Valid after a successful DecodeInitial().
  static size_t GetRequiredNumOfPictures();

  // Do any necessary initialization before the sandbox is enabled.
  static void PreSandboxInitialization();

  // Lazily initialize static data after sandbox is enabled.  Return false on
  // init failure.
  static bool PostSandboxInitialization();

 private:
  // We need to keep at least kDPBMaxSize pictures in DPB for
  // reference/to display later and an additional one for the one currently
  // being decoded. We also ask for some additional ones since VDA needs
  // to accumulate a few ready-to-output pictures before it actually starts
  // displaying and giving them back. +2 instead of +1 because of subjective
  // smoothness improvement during testing.
  enum { kNumReqPictures = H264DPB::kDPBMaxSize +
      media::limits::kMaxVideoFrames + 2 };

  // Internal state of the decoder.
  enum State {
    kUninitialized,  // Initialize() not yet called.
    kInitialized,  // Initialize() called, pictures requested.
    kDecoding,  // DecodeInitial() successful, output surfaces allocated.
    kAfterReset,  // After Reset() during decoding.
    kError,  // Error in kDecoding state.
  };

  // Get usable framebuffer configuration for use in binding textures
  // or return false on failure.
  bool InitializeFBConfig();

  // Process H264 stream structures.
  bool ProcessSPS(int sps_id);
  bool ProcessPPS(int pps_id);
  bool ProcessSlice(H264SliceHeader* slice_hdr);

  // Initialize the current picture according to data in |slice_hdr|.
  bool InitCurrPicture(H264SliceHeader* slice_hdr);

  // Calculate picture order counts for the new picture
  // on initialization of a new frame (see spec).
  bool CalculatePicOrderCounts(H264SliceHeader* slice_hdr);

  // Update PicNum values in pictures stored in DPB on creation of new
  // frame (see spec).
  void UpdatePicNums();

  // Construct initial reference picture lists for use in decoding of
  // P and B pictures (see 8.2.4 in spec).
  void ConstructReferencePicListsP(H264SliceHeader* slice_hdr);
  void ConstructReferencePicListsB(H264SliceHeader* slice_hdr);

  // Helper functions for reference list construction, per spec.
  int PicNumF(H264Picture *pic);
  int LongTermPicNumF(H264Picture *pic);

  // Perform the reference picture lists' modification (reordering), as
  // specified in spec (8.2.4).
  //
  // |list| indicates list number and should be either 0 or 1.
  bool ModifyReferencePicList(H264SliceHeader *slice_hdr, int list);

  // Perform reference picture memory management operations (marking/unmarking
  // of reference pictures, long term picture management, discarding, etc.).
  // See 8.2.5 in spec.
  bool HandleMemoryManagementOps();
  void ReferencePictureMarking();

  // Start processing a new frame.
  bool StartNewFrame(H264SliceHeader* slice_hdr);

  // All data for a frame received, process it and decode.
  bool FinishPrevFrameIfPresent();

  // Called after decoding, performs all operations to be done after decoding,
  // including DPB management, reference picture marking and memory management
  // operations.
  // This will also output a picture if one is ready for output.
  bool FinishPicture();

  // Convert VideoCodecProfile to VAProfile and set it as active.
  bool SetProfile(media::VideoCodecProfile profile);

  // Vaapi-related functions.

  // Allocates VASurfaces and creates a VAContext for them.
  bool CreateVASurfaces();

  // Destroys allocated VASurfaces and related VAContext.
  void DestroyVASurfaces();
  // Destroys all buffers in |pending_slice_bufs_| and |pending_va_bufs_|.
  void DestroyPendingBuffers();
  // Destroys a list of buffers.
  void DestroyBuffers(size_t num_va_buffers, const VABufferID* va_buffers);

  // These queue up data for HW decoder to be committed on running HW decode.
  bool SendPPS();
  bool SendIQMatrix();
  bool SendVASliceParam(H264SliceHeader* slice_hdr);
  bool SendSliceData(const uint8* ptr, size_t size);
  bool QueueSlice(H264SliceHeader* slice_hdr);

  // Helper methods for filling HW structures.
  void FillVAPicture(VAPictureH264 *va_pic, H264Picture* pic);
  int FillVARefFramesFromDPB(VAPictureH264 *va_pics, int num_pics);

  // Commits all pending data for HW decoder and starts HW decoder.
  bool DecodePicture();

  // Notifies client that a picture is ready for output.
  bool OutputPic(H264Picture* pic);

  State state_;

  // A frame has been sent to hardware as the result of the last
  // DecodeOneFrame() call.
  bool frame_ready_at_hw_;

  // Parser in use.
  H264Parser parser_;

  // DPB in use.
  H264DPB dpb_;

  // Picture currently being processed/decoded.
  scoped_ptr<H264Picture> curr_pic_;

  // Reference picture lists, constructed for each picture before decoding.
  // Those lists are not owners of the pointers (DPB is).
  H264Picture::PtrVector ref_pic_list0_;
  H264Picture::PtrVector ref_pic_list1_;

  // Global state values, needed in decoding. See spec.
  int max_pic_order_cnt_lsb_;
  int max_frame_num_;
  int max_pic_num_;
  int max_long_term_frame_idx_;

  int frame_num_;
  int prev_frame_num_;
  int prev_frame_num_offset_;
  bool prev_has_memmgmnt5_;

  // Values related to previously decoded reference picture.
  bool prev_ref_has_memmgmnt5_;
  int prev_ref_top_field_order_cnt_;
  int prev_ref_pic_order_cnt_msb_;
  int prev_ref_pic_order_cnt_lsb_;
  H264Picture::Field prev_ref_field_;

  // Currently active SPS and PPS.
  int curr_sps_id_;
  int curr_pps_id_;

  // Output picture size.
  int pic_width_;
  int pic_height_;

  // Data queued up for HW decoder, to be committed on next HW decode.
  std::queue<VABufferID> pending_slice_bufs_;
  std::queue<VABufferID> pending_va_bufs_;

  // Manages binding of a client-provided output buffer (texture) to VASurface.
  class DecodeSurface;

  // Maps output_buffer_id to a decode surface. Used to look up surfaces
  // on requests from the client.
  typedef std::map<int32, linked_ptr<DecodeSurface> > DecodeSurfaces;
  DecodeSurfaces decode_surfaces_;

  // Number of decode surface currently available for decoding.
  int num_available_decode_surfaces_;

  // Maps decode surfaces to PicOrderCount, used to look up output buffers
  // when a decision to output a picture has been made.
  typedef std::map<int, DecodeSurface*> POCToDecodeSurfaces;
  POCToDecodeSurfaces poc_to_decode_surfaces_;

  // Find an available surface and assign it to given PicOrderCnt |poc|,
  // removing it from the available surfaces pool. Return true if a surface
  // has been found, false otherwise.
  bool AssignSurfaceToPoC(int poc);

  // Mark a surface as unused for decoding, unassigning it from |poc|. If the
  // corresponding picture is not at client to be displayed,
  // release the surface.
  void UnassignSurfaceFromPoC(int poc);

  // The id of current input buffer, which will be associated with an
  // output picture if a frame is decoded successfully.
  int32 curr_input_id_;

  // Any method that uses GL/VA routines probably wants to make sure
  // make_context_current_.Run() is called at the top of the method.
  // X/GLX handles.
  Display* x_display_;
  base::Callback<bool(void)> make_context_current_;
  GLXFBConfig fb_config_;

  // VA handles.
  VADisplay va_display_;
  VAConfigID va_config_id_;
  VAContextID va_context_id_;
  VAProfile profile_;
  bool va_context_created_;

  // Allocated VASurfaces.
  VASurfaceID va_surface_ids_[kNumReqPictures];

  // Called by decoder when a picture should be outputted.
  OutputPicCB output_pic_cb_;

  // Called by decoder to post a Sync job on the ChildThread.
  SyncPicCB sync_pic_cb_;

  // PicOrderCount of the previously outputted frame.
  int last_output_poc_;

  // Has static initialization of pre-sandbox components completed successfully?
  static bool pre_sandbox_init_done_;

  DISALLOW_COPY_AND_ASSIGN(VaapiH264Decoder);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VAAPI_H264_DECODER_H_
