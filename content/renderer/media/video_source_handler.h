// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_SOURCE_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_SOURCE_HANDLER_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/videosourceinterface.h"

namespace cricket {
class VideoFrame;
}

namespace content {

class MediaStreamDependencyFactory;
class MediaStreamRegistryInterface;

// Interface used by the effects pepper plugin to get captured frame
// from the video track.
class CONTENT_EXPORT FrameReaderInterface {
 public:
  // Got a new captured frame.
  // The ownership of the |frame| is transfered to the caller. So the caller
  // must delete |frame| when done with it.
  virtual bool GotFrame(cricket::VideoFrame* frame) = 0;

 protected:
  virtual ~FrameReaderInterface() {}
};

// VideoSourceHandler is a glue class between the webrtc MediaStream and
// the effects pepper plugin host.
class CONTENT_EXPORT VideoSourceHandler {
 public:
  // |registry| is used to look up the media stream by url. If a NULL |registry|
  // is given, the global WebKit::WebMediaStreamRegistry will be used.
  explicit VideoSourceHandler(MediaStreamRegistryInterface* registry);
  virtual ~VideoSourceHandler();
  // Connects to the first video track in the MediaStream specified by |url| and
  // the received frames will be delivered via |reader|.
  // Returns true on success and false on failure.
  bool Open(const std::string& url, FrameReaderInterface* reader);
  // Closes |reader|'s connection with the first video track in
  // the MediaStream specified by |url|, i.e. stops receiving frames from the
  // video track.
  // Returns true on success and false on failure.
  bool Close(const std::string& url, FrameReaderInterface* reader);

  // Gets the VideoRenderer associated with |reader|.
  // Made it public only for testing purpose.
  cricket::VideoRenderer* GetReceiver(FrameReaderInterface* reader);

 private:
  scoped_refptr<webrtc::VideoSourceInterface> GetFirstVideoSource(
      const std::string& url);

  MediaStreamRegistryInterface* registry_;
  std::map<FrameReaderInterface*, cricket::VideoRenderer*> reader_to_receiver_;

  DISALLOW_COPY_AND_ASSIGN(VideoSourceHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_SOURCE_HANDLER_H_

