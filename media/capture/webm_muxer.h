// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_LIBWEBM_MUXER_H_
#define MEDIA_FILTERS_LIBWEBM_MUXER_H_

#include <set>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "third_party/libwebm/source/mkvmuxer.hpp"
#include "ui/gfx/geometry/size.h"

namespace media {

// Adapter class to manage a WebM container [1], a simplified version of a
// Matroska container [2], composed of an EBML header, and a single Segment
// including at least a Track Section and a number of SimpleBlocks each
// containing a single encoded video frame. WebM container has no Trailer.
// Clients will push encoded VPx video frames one by one via OnEncodedVideo()
// with indication of the Track they belong to, and libwebm will eventually ping
// the WriteDataCB passed on contructor with the wrapped encoded data. All
// operations must happen in a single thread, where WebmMuxer is created and
// destroyed.
// [1] http://www.webmproject.org/docs/container/
// [2] http://www.matroska.org/technical/specs/index.html
// TODO(mcasas): Add support for Audio muxing.
class MEDIA_EXPORT WebmMuxer : public NON_EXPORTED_BASE(mkvmuxer::IMkvWriter) {
 public:
  // Callback to be called when WebmMuxer is ready to write a chunk of data,
  // either any file header or a SingleBlock. The chunk is described as a byte
  // array and a byte length.
  using WriteDataCB = base::Callback<void(const base::StringPiece&)>;

  explicit WebmMuxer(const WriteDataCB& write_data_callback);
  ~WebmMuxer() override;

  // Creates and adds a new video track. Called before receiving the first
  // frame of a given Track, adds |frame_size| and |frame_rate| to the Segment
  // info, although individual frames passed to OnEncodedVideo() can have any
  // frame size. Returns the track_index to be used for OnEncodedVideo().
  uint64_t AddVideoTrack(const gfx::Size& frame_size, double frame_rate);

  // Adds a frame with |encoded_data.data()| to WebM Segment. |track_number| is
  // a caller-side identifier and must have been provided by AddVideoTrack().
  // TODO(mcasas): Revisit how |encoded_data| is passed once the whole recording
  // chain is setup (http://crbug.com/262211).
  void OnEncodedVideo(uint64_t track_number,
                      const base::StringPiece& encoded_data,
                      base::TimeDelta timestamp,
                      bool keyframe);

 private:
  friend class WebmMuxerTest;

  // IMkvWriter interface.
  mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) override;
  mkvmuxer::int64 Position() const override;
  mkvmuxer::int32 Position(mkvmuxer::int64 position) override;
  bool Seekable() const override;
  void ElementStartNotify(mkvmuxer::uint64 element_id,
                          mkvmuxer::int64 position) override;

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // Callback to dump written data as being called by libwebm.
  const WriteDataCB write_data_callback_;

  // Rolling counter of the position in bytes of the written goo.
  base::CheckedNumeric<mkvmuxer::int64> position_;

  // The MkvMuxer active element.
  mkvmuxer::Segment segment_;

  DISALLOW_COPY_AND_ASSIGN(WebmMuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_LIBWEBM_MUXER_H_
