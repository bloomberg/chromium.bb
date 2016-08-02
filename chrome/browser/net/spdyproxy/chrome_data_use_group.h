// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_CHROME_DATA_USE_GROUP_H_
#define CHROME_BROWSER_NET_SPDYPROXY_CHROME_DATA_USE_GROUP_H_

#include <string>

#include "components/data_reduction_proxy/core/browser/data_use_group.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

// Chrome implementation of |DataUseGroup|. Instances are created on the
// IO thread but can be destroyed on any thread. The class is reference counted,
// because its lifetime is determined by both the frames and the URL requests
// whose data use is being tracked. Logically, a ChromeDataUseGroup would be
// owned by its corresponding frame, but outstanding requests may incur data use
// even after the frame is destroyed. This class is not thread safe and each
// method and field can only be accessed on its designated thread.
class ChromeDataUseGroup : public data_reduction_proxy::DataUseGroup {
 public:
  // Creates a new instance for the given URL request. Typically this will be
  // the main frame URL request, but any URL request from the same group can be
  // used. This constructor can be called at any stage in the life cycle of
  // the URL request after construction by |resource_dispatcher_host_impl|.
  // Instances can only be created on the IO thread.
  explicit ChromeDataUseGroup(const net::URLRequest* request);

  // Chrome implementation of GetHostname(). This method must be called on the
  // UI thread.
  std::string GetHostname() override;

  // Initializes the object. This method will fetch the URL to ascribe data to
  // from the UI thread if it has not already been fetched. This method must be
  // called on the IO thread.
  void Initialize() override;

 protected:
  friend class base::RefCountedThreadSafe<ChromeDataUseGroup>;

  ~ChromeDataUseGroup() override;

 private:
  // Fetches the URL to ascribe data to if it has not already been fetched. This
  // method must be called on the IO thread.
  void InitializeOnUIThread();

  // Fields accessed on the IO thread only:

  // Tracks whether Initialized() has been called.
  bool initialized_;

  // Fields accessed on UI thread only:

  // Tracks whether InitializeOnUIThread has been called.
  bool initilized_on_ui_thread_;

  // IDs to identify the |RenderFrameHost| that ascribes its data use to this
  // instance.
  int render_process_id_;
  int render_frame_id_;

  // Whether the |RenderFrameHost| ids above are valid. This field will only
  // be true if requests are made in the context of a tab. It will be false
  // for requests not initiated from within a |RenderFrameHost|. e.g.:
  // requests initiated by service workers. This information is not invalidated
  // when the |RenderFrameHost| is destroyed but rather used to match requests
  // to the render frames they belonged to.
  bool has_valid_render_frame_id_;

  // URL to ascribe data use. For page loads, this is the main frame URL. For
  // requests that are not made from a |RenderFrameHost| this will be the
  // request URL.
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDataUseGroup);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_CHROME_DATA_USE_GROUP_H_
