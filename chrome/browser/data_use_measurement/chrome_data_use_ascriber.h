// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_ASCRIBER_H_
#define CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_ASCRIBER_H_

#include "base/macros.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}

namespace data_use_measurement {

// Browser implementation of DataUseAscriber. Maintains a list of
// DataUseRecorder instances, one for each source of data, such as a page
// load.
//
// A page includes all resources loaded in response to a main page navigation.
// The scope of a page load ends either when the tab is closed or a new page
// load is initiated by clicking on a link, entering a new URL, window location
// change, etc.
//
// For URLRequests initiated outside the context of a page load, such as
// Service Workers, Chrome Services, etc, a new instance of DataUseRecorder
// will be created for each URLRequest.
//
// Each page load will be associated with an instance of DataUseRecorder.
// Each URLRequest initiated in the context of a page load will be mapped to
// the DataUseRecorder instance for page load.
//
// This class lives entirely on the IO thread. It maintains a copy of frame and
// navigation information on the IO thread.
class ChromeDataUseAscriber : public DataUseAscriber {
 public:
  ChromeDataUseAscriber();

  ~ChromeDataUseAscriber() override;

  // DataUseAscriber:
  DataUseRecorder* GetDataUseRecorder(net::URLRequest* request) override;

  // Called when a render frame host is created.
  void RenderFrameCreated(int render_process_id,
                          int render_frame_id,
                          int parent_render_process_id,
                          int parent_render_frame_id);

  // Called when a render frame host is deleted.
  void RenderFrameDeleted(int render_process_id,
                          int render_frame_id,
                          int parent_render_process_id,
                          int parent_render_frame_id);

  // Called when a main frame navigation is started.
  void DidStartMainFrameNavigation(GURL gurl,
                                   int render_process_id,
                                   int render_frame_id,
                                   void* navigation_handle);

  // Called when a main frame navigation is completed.
  void DidFinishMainFrameNavigation(GURL gurl,
                                    int render_process_id,
                                    int render_frame_id,
                                    bool is_same_page_navigation,
                                    void* navigation_handle);

  // Called when a main frame navigation is redirected.
  void DidRedirectMainFrameNavigation(GURL gurl,
                                      int render_process_id,
                                      int render_frame_id,
                                      void* navigation_handle);

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeDataUseAscriber);
};

}  // namespace data_use_measurement

#endif  // CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_ASCRIBER_H_
