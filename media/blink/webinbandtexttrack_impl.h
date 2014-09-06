// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBINBANDTEXTTRACK_IMPL_H_
#define MEDIA_BLINK_WEBINBANDTEXTTRACK_IMPL_H_

#include "third_party/WebKit/public/platform/WebInbandTextTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace media {

class WebInbandTextTrackImpl : public blink::WebInbandTextTrack {
 public:
  WebInbandTextTrackImpl(Kind kind,
                         const blink::WebString& label,
                         const blink::WebString& language,
                         const blink::WebString& id,
                         int index);
  virtual ~WebInbandTextTrackImpl();

  virtual void setClient(blink::WebInbandTextTrackClient* client);
  virtual blink::WebInbandTextTrackClient* client();

  virtual Kind kind() const;

  virtual blink::WebString label() const;
  virtual blink::WebString language() const;
  virtual blink::WebString id() const;

  virtual int textTrackIndex() const;

 private:
  blink::WebInbandTextTrackClient* client_;
  Kind kind_;
  blink::WebString label_;
  blink::WebString language_;
  blink::WebString id_;
  int index_;
  DISALLOW_COPY_AND_ASSIGN(WebInbandTextTrackImpl);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBINBANDTEXTTRACK_IMPL_H_
