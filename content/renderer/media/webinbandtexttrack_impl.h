// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBINBANDTEXTTRACK_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBINBANDTEXTTRACK_IMPL_H_

#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebInbandTextTrack.h"

namespace content {

class WebInbandTextTrackImpl : public blink::WebInbandTextTrack {
 public:
  WebInbandTextTrackImpl(Kind kind,
                         const blink::WebString& label,
                         const blink::WebString& language,
                         int index);
  virtual ~WebInbandTextTrackImpl();

  virtual void setClient(blink::WebInbandTextTrackClient* client);
  virtual blink::WebInbandTextTrackClient* client();

  virtual void setMode(Mode mode);
  virtual Mode mode() const;

  virtual Kind kind() const;
  virtual bool isClosedCaptions() const;

  virtual blink::WebString label() const;
  virtual blink::WebString language() const;
  virtual bool isDefault() const;

  virtual int textTrackIndex() const;

 private:
  blink::WebInbandTextTrackClient* client_;
  Mode mode_;
  Kind kind_;
  blink::WebString label_;
  blink::WebString language_;
  int index_;
  DISALLOW_COPY_AND_ASSIGN(WebInbandTextTrackImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBINBANDTEXTTRACK_IMPL_H_
