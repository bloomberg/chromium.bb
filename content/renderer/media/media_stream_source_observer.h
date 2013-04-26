// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_OBSERVER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_OBSERVER_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"

namespace content {

class MediaStreamSourceExtraData;

// MediaStreamSourceObserver listens to events on MediaSourceInterface and
// notify WebKit. It will be owned by MediaStreamSourceExtraData.
class CONTENT_EXPORT MediaStreamSourceObserver
    : NON_EXPORTED_BASE(public webrtc::ObserverInterface),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  MediaStreamSourceObserver(webrtc::MediaSourceInterface* webrtc_source,
                            MediaStreamSourceExtraData* extra_data);
  virtual ~MediaStreamSourceObserver();

 private:
  // webrtc::ObserverInterface implementation.
  virtual void OnChanged() OVERRIDE;

  webrtc::MediaSourceInterface::SourceState state_;
  scoped_refptr<webrtc::MediaSourceInterface> webrtc_source_;
  MediaStreamSourceExtraData* extra_data_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamSourceObserver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_OBSERVER_H_
