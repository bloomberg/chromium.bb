// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_resource_shim.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"

namespace media {

MediaResourceShim::MediaResourceShim(
    std::vector<mojom::DemuxerStreamPtr> streams,
    const base::Closure& demuxer_ready_cb)
    : demuxer_ready_cb_(demuxer_ready_cb),
      streams_ready_(0),
      weak_factory_(this) {
  DCHECK(!streams.empty());
  DCHECK(!demuxer_ready_cb_.is_null());

  for (auto& s : streams) {
    streams_.emplace_back(new MojoDemuxerStreamAdapter(
        std::move(s), base::Bind(&MediaResourceShim::OnStreamReady,
                                 weak_factory_.GetWeakPtr())));
  }
}

MediaResourceShim::~MediaResourceShim() {}

// This function returns only the first stream of the given |type| for now.
// TODO(servolk): Make this work with multiple streams.
DemuxerStream* MediaResourceShim::GetStream(DemuxerStream::Type type) {
  DCHECK(demuxer_ready_cb_.is_null());
  for (auto& stream : streams_) {
    if (stream->type() == type)
      return stream.get();
  }

  return nullptr;
}

void MediaResourceShim::OnStreamReady() {
  if (++streams_ready_ == streams_.size())
    base::ResetAndReturn(&demuxer_ready_cb_).Run();
}

}  // namespace media
