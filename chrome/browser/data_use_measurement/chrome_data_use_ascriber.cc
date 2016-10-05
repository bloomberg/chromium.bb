// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

namespace data_use_measurement {

ChromeDataUseAscriber::ChromeDataUseAscriber() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

ChromeDataUseAscriber::~ChromeDataUseAscriber() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

DataUseRecorder* ChromeDataUseAscriber::GetDataUseRecorder(
    net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return nullptr;
}

void ChromeDataUseAscriber::RenderFrameCreated(int render_process_id,
                                               int render_frame_id,
                                               int parent_render_process_id,
                                               int parent_render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ChromeDataUseAscriber::RenderFrameDeleted(int render_process_id,
                                               int render_frame_id,
                                               int parent_render_process_id,
                                               int parent_render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ChromeDataUseAscriber::DidStartMainFrameNavigation(
    GURL gurl,
    int render_process_id,
    int render_frame_id,
    void* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ChromeDataUseAscriber::DidFinishMainFrameNavigation(
    GURL gurl,
    int render_process_id,
    int render_frame_id,
    bool is_same_page_navigation,
    void* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ChromeDataUseAscriber::DidRedirectMainFrameNavigation(
    GURL gurl,
    int render_process_id,
    int render_frame_id,
    void* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

}  // namespace data_use_measurement
