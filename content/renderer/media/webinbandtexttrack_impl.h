// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBINBANDTEXTTRACK_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBINBANDTEXTTRACK_IMPL_H_

#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebInbandTextTrack.h"

namespace content {

class WebInbandTextTrackImpl : public WebKit::WebInbandTextTrack {
 public:
  WebInbandTextTrackImpl(Kind kind,
                         const WebKit::WebString& label,
                         const WebKit::WebString& language,
                         int index);
  virtual ~WebInbandTextTrackImpl();

  virtual void setClient(WebKit::WebInbandTextTrackClient* client);
  virtual WebKit::WebInbandTextTrackClient* client();

  virtual void setMode(Mode mode);
  virtual Mode mode() const;

  virtual Kind kind() const;
  virtual bool isClosedCaptions() const;

  virtual WebKit::WebString label() const;
  virtual WebKit::WebString language() const;
  virtual bool isDefault() const;

  virtual int textTrackIndex() const;

 private:
  WebKit::WebInbandTextTrackClient* client_;
  Mode mode_;
  Kind kind_;
  WebKit::WebString label_;
  WebKit::WebString language_;
  int index_;
  DISALLOW_COPY_AND_ASSIGN(WebInbandTextTrackImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBINBANDTEXTTRACK_IMPL_H_
