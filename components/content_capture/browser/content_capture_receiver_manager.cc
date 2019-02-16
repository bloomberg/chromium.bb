// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_receiver_manager.h"

#include <utility>

#include "components/content_capture/browser/content_capture_receiver.h"
#include "content/public/browser/web_contents.h"

namespace content_capture {
namespace {

const void* const kUserDataKey = &kUserDataKey;

}  // namespace

ContentCaptureReceiverManager::ContentCaptureReceiverManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  const std::vector<content::RenderFrameHost*> frames =
      web_contents->GetAllFrames();
  for (content::RenderFrameHost* frame : frames)
    RenderFrameCreated(frame);

  web_contents->SetUserData(
      kUserDataKey, std::unique_ptr<ContentCaptureReceiverManager>(this));
}

ContentCaptureReceiverManager::~ContentCaptureReceiverManager() = default;

ContentCaptureReceiverManager* ContentCaptureReceiverManager::FromWebContents(
    content::WebContents* contents) {
  return static_cast<ContentCaptureReceiverManager*>(
      contents->GetUserData(kUserDataKey));
}

void ContentCaptureReceiverManager::BindContentCaptureReceiver(
    mojom::ContentCaptureReceiverAssociatedRequest request,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  if (!web_contents)
    return;

  ContentCaptureReceiverManager* manager =
      ContentCaptureReceiverManager::FromWebContents(web_contents);
  if (!manager)
    return;

  auto* receiver = manager->ContentCaptureReceiverForFrame(render_frame_host);
  if (receiver)
    receiver->BindRequest(std::move(request));
}

ContentCaptureReceiver*
ContentCaptureReceiverManager::ContentCaptureReceiverForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto mapping = frame_map_.find(render_frame_host);
  return mapping == frame_map_.end() ? nullptr : mapping->second.get();
}

void ContentCaptureReceiverManager::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // The frame might not have content, but it could be parent of other frame. we
  // always create the ContentCaptureReceiver for ContentCaptureSession
  // purpose.
  if (ContentCaptureReceiverForFrame(render_frame_host))
    return;
  frame_map_.insert(std::make_pair(
      render_frame_host,
      std::make_unique<ContentCaptureReceiver>(render_frame_host)));
}

void ContentCaptureReceiverManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_map_.erase(render_frame_host);
}

void ContentCaptureReceiverManager::DidCaptureContent(
    ContentCaptureReceiver* content_capture_receiver,
    const ContentCaptureData& data) {
  // The root of |data| is frame, we need get its ancestor only.
  ContentCaptureSession parent_session;
  BuildContentCaptureSession(*content_capture_receiver,
                             true /* ancestor_only */, &parent_session);
  DidCaptureContent(parent_session, data);
}

void ContentCaptureReceiverManager::DidRemoveContent(
    ContentCaptureReceiver* content_capture_receiver,
    const std::vector<int64_t>& data) {
  ContentCaptureSession session;
  // The |data| is a list of text content id, the session should include
  // |content_capture_receiver| associated frame.
  BuildContentCaptureSession(*content_capture_receiver,
                             false /* ancestor_only */, &session);
  DidRemoveContent(session, data);
}

void ContentCaptureReceiverManager::DidRemoveSession(
    ContentCaptureReceiver* content_capture_receiver) {
  ContentCaptureSession session;
  // The session should include the removed frame that the
  // |content_capture_receiver| associated with.
  BuildContentCaptureSession(*content_capture_receiver,
                             false /* ancestor_only */, &session);
  DidRemoveSession(session);
}

void ContentCaptureReceiverManager::BuildContentCaptureSession(
    const ContentCaptureReceiver& content_capture_receiver,
    bool ancestor_only,
    ContentCaptureSession* session) {
  if (!ancestor_only)
    session->push_back(content_capture_receiver.frame_content_capture_data());

  content::RenderFrameHost* rfh = content_capture_receiver.rfh()->GetParent();
  while (rfh) {
    ContentCaptureReceiver* receiver = ContentCaptureReceiverForFrame(rfh);
    // TODO(michaelbai): Only creates ContentCaptureReceiver here, clean up the
    // code in RenderFrameCreated().
    if (!receiver) {
      RenderFrameCreated(rfh);
      receiver = ContentCaptureReceiverForFrame(rfh);
    }
    session->push_back(receiver->frame_content_capture_data());
    rfh = receiver->rfh()->GetParent();
  }
}

}  // namespace content_capture
