// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediasourceclient_impl.h"

#include "base/guid.h"
#include "content/renderer/media/websourcebuffer_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"

using ::WebKit::WebString;
using ::WebKit::WebMediaSourceClient;

namespace content {

#define COMPILE_ASSERT_MATCHING_STATUS_ENUM(webkit_name, chromium_name) \
  COMPILE_ASSERT(static_cast<int>(WebMediaSourceClient::webkit_name) == \
                 static_cast<int>(media::ChunkDemuxer::chromium_name),  \
                 mismatching_status_enums)
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddStatusOk, kOk);
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddStatusNotSupported, kNotSupported);
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddStatusReachedIdLimit, kReachedIdLimit);
#undef COMPILE_ASSERT_MATCHING_STATUS_ENUM

WebMediaSourceClientImpl::WebMediaSourceClientImpl(
    media::ChunkDemuxer* demuxer, media::LogCB log_cb)
    : demuxer_(demuxer),
      log_cb_(log_cb) {
  DCHECK(demuxer_);
}

WebMediaSourceClientImpl::~WebMediaSourceClientImpl() {}

WebMediaSourceClient::AddStatus WebMediaSourceClientImpl::addSourceBuffer(
    const WebKit::WebString& type,
    const WebKit::WebVector<WebKit::WebString>& codecs,
    WebKit::WebSourceBuffer** source_buffer) {
  std::string id = base::GenerateGUID();
  std::vector<std::string> new_codecs(codecs.size());
  for (size_t i = 0; i < codecs.size(); ++i)
    new_codecs[i] = codecs[i].utf8().data();
  WebMediaSourceClient::AddStatus result =
      static_cast<WebMediaSourceClient::AddStatus>(
          demuxer_->AddId(id, type.utf8().data(), new_codecs));

  if (result == WebMediaSourceClient::AddStatusOk)
    *source_buffer = new WebSourceBufferImpl(id, demuxer_);

  return result;
}

double WebMediaSourceClientImpl::duration() {
  return demuxer_->GetDuration();
}

void WebMediaSourceClientImpl::setDuration(double new_duration) {
  DCHECK_GE(new_duration, 0);
  demuxer_->SetDuration(new_duration);
}

// TODO(acolwell): Remove this once endOfStream() is removed from Blink.
void WebMediaSourceClientImpl::endOfStream(
    WebMediaSourceClient::EndOfStreamStatus status) {
  markEndOfStream(status);
}

void WebMediaSourceClientImpl::markEndOfStream(
    WebMediaSourceClient::EndOfStreamStatus status) {
  media::PipelineStatus pipeline_status = media::PIPELINE_OK;

  switch (status) {
    case WebMediaSourceClient::EndOfStreamStatusNoError:
      break;
    case WebMediaSourceClient::EndOfStreamStatusNetworkError:
      pipeline_status = media::PIPELINE_ERROR_NETWORK;
      break;
    case WebMediaSourceClient::EndOfStreamStatusDecodeError:
      pipeline_status = media::PIPELINE_ERROR_DECODE;
      break;
    default:
      NOTIMPLEMENTED();
  }

  demuxer_->MarkEndOfStream(pipeline_status);
}

void WebMediaSourceClientImpl::unmarkEndOfStream() {
  demuxer_->UnmarkEndOfStream();
}

}  // namespace content
