// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOCK_RESOURCE_CONTEXT_H_
#define CONTENT_BROWSER_MOCK_RESOURCE_CONTEXT_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/resource_context.h"
#include "net/url_request/url_request_context.h"

namespace base {
template <typename T>
struct DefaultLazyInstanceTraits;
}  // namespace base

namespace content {

class MockResourceContext : public ResourceContext {
 public:
  // Note that this is a shared instance between all tests. Make no assumptions
  // regarding its members. Or you can construct a local version below.
  static MockResourceContext* GetInstance();

  MockResourceContext();
  explicit MockResourceContext(net::URLRequestContext* context);
  virtual ~MockResourceContext();

  void set_request_context(net::URLRequestContext* context) {
    test_request_context_ = context;
  }

  void set_media_observer(MediaObserver* observer) {
    media_observer_ = observer;
  }
  void set_media_stream_manager(media_stream::MediaStreamManager* manager) {
    media_stream_manager_ = manager;
  }
  void set_audio_manager(AudioManager* manager) { audio_manager_ = manager; }

  // ResourceContext implementation:
  virtual net::HostResolver* GetHostResolver() OVERRIDE;
  virtual net::URLRequestContext* GetRequestContext() OVERRIDE;
  virtual HostZoomMap* GetHostZoomMap() OVERRIDE;
  virtual MediaObserver* GetMediaObserver() OVERRIDE;
  virtual media_stream::MediaStreamManager* GetMediaStreamManager() OVERRIDE;
  virtual AudioManager* GetAudioManager() OVERRIDE;

 private:
  friend struct base::DefaultLazyInstanceTraits<MockResourceContext>;

  scoped_refptr<net::URLRequestContext> test_request_context_;
  MediaObserver* media_observer_;
  media_stream::MediaStreamManager* media_stream_manager_;
  AudioManager* audio_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOCK_RESOURCE_CONTEXT_H_
