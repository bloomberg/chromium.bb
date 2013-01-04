// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"

#include "android_webview/common/render_view_messages.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {

AwRenderViewHostExt::AwRenderViewHostExt(content::WebContents* contents,
                                         Client* client)
    : content::WebContentsObserver(contents),
      has_new_hit_test_data_(false),
      client_(client) {
}

AwRenderViewHostExt::~AwRenderViewHostExt() {}

void AwRenderViewHostExt::DocumentHasImages(DocumentHasImagesResult result) {
  DCHECK(CalledOnValidThread());
  if (!web_contents()->GetRenderViewHost()) {
    result.Run(false);
    return;
  }
  static int next_id = 1;
  int this_id = next_id++;
  pending_document_has_images_requests_[this_id] = result;
  Send(new AwViewMsg_DocumentHasImages(web_contents()->GetRoutingID(),
                                       this_id));
}

void AwRenderViewHostExt::ClearCache() {
  DCHECK(CalledOnValidThread());
  Send(new AwViewMsg_ClearCache);
}

bool AwRenderViewHostExt::HasNewHitTestData() const {
  return has_new_hit_test_data_;
}

void AwRenderViewHostExt::MarkHitTestDataRead() {
  has_new_hit_test_data_ = false;
}

void AwRenderViewHostExt::RequestNewHitTestDataAt(int view_x, int view_y) {
  DCHECK(CalledOnValidThread());
  Send(new AwViewMsg_DoHitTest(web_contents()->GetRoutingID(),
                               view_x,
                               view_y));
}

const AwHitTestData& AwRenderViewHostExt::GetLastHitTestData() const {
  DCHECK(CalledOnValidThread());
  return last_hit_test_data_;
}

void AwRenderViewHostExt::RenderViewGone(base::TerminationStatus status) {
  DCHECK(CalledOnValidThread());
  for (std::map<int, DocumentHasImagesResult>::iterator pending_req =
           pending_document_has_images_requests_.begin();
       pending_req != pending_document_has_images_requests_.end();
      ++pending_req) {
    pending_req->second.Run(false);
  }
}

bool AwRenderViewHostExt::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AwRenderViewHostExt, message)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_DocumentHasImagesResponse,
                        OnDocumentHasImagesResponse)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_UpdateHitTestData,
                        OnUpdateHitTestData)
    IPC_MESSAGE_HANDLER(AwViewHostMsg_PictureUpdated,
                        OnPictureUpdated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled ? true : WebContentsObserver::OnMessageReceived(message);
}

void AwRenderViewHostExt::OnDocumentHasImagesResponse(int msg_id,
                                                      bool has_images) {
  DCHECK(CalledOnValidThread());
  std::map<int, DocumentHasImagesResult>::iterator pending_req =
      pending_document_has_images_requests_.find(msg_id);
  if (pending_req == pending_document_has_images_requests_.end()) {
    DLOG(WARNING) << "unexpected DocumentHasImages Response: " << msg_id;
  } else {
    pending_req->second.Run(has_images);
    pending_document_has_images_requests_.erase(pending_req);
  }
}

void AwRenderViewHostExt::OnUpdateHitTestData(
    const AwHitTestData& hit_test_data) {
  DCHECK(CalledOnValidThread());
  last_hit_test_data_ = hit_test_data;
  has_new_hit_test_data_ = true;
}

void AwRenderViewHostExt::OnPictureUpdated() {
  if (client_)
    client_->OnPictureUpdated(web_contents()->GetRenderProcessHost()->GetID(),
                              routing_id());
}

}  // namespace android_webview
