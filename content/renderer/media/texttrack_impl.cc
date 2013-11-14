// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/texttrack_impl.h"

#include "content/renderer/media/webinbandtexttrack_impl.h"
#include "third_party/WebKit/public/platform/WebInbandTextTrackClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"

namespace content {

TextTrackImpl::TextTrackImpl(blink::WebMediaPlayerClient* client,
                             WebInbandTextTrackImpl* text_track)
    : client_(client), text_track_(text_track) {
  client_->addTextTrack(text_track_.get());
}

TextTrackImpl::~TextTrackImpl() {
  if (text_track_->client())
    client_->removeTextTrack(text_track_.get());
}

void TextTrackImpl::addWebVTTCue(const base::TimeDelta& start,
                                 const base::TimeDelta& end,
                                 const std::string& id,
                                 const std::string& content,
                                 const std::string& settings) {
  if (blink::WebInbandTextTrackClient* client = text_track_->client())
    client->addWebVTTCue(start.InSecondsF(),
                         end.InSecondsF(),
                         blink::WebString::fromUTF8(id),
                         blink::WebString::fromUTF8(content),
                         blink::WebString::fromUTF8(settings));
}

}  // namespace content
