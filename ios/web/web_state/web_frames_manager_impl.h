// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_FRAMES_MANAGER_IMPL_H_
#define IOS_WEB_WEB_STATE_WEB_FRAMES_MANAGER_IMPL_H_

#include "ios/web/public/web_state/web_frames_manager.h"
#include "ios/web/public/web_state/web_state_user_data.h"

namespace web {

class WebFrame;

class WebFramesManagerImpl
    : public WebFramesManager,
      public web::WebStateUserData<WebFramesManagerImpl> {
 public:
  ~WebFramesManagerImpl() override;
  explicit WebFramesManagerImpl(web::WebState* web_state);

  // Adds |frame| to the list of web frames associated with WebState.
  void AddFrame(std::unique_ptr<WebFrame> frame);
  // Removes the web frame with |frame_id|, if one exists, from the list of
  // associated web frames.
  void RemoveFrameWithId(const std::string& frame_id);
  // Removes all web frames from the list of associated web frames.
  void RemoveAllWebFrames();
  // Broadcasts a (not encrypted) JavaScript message to get the identifiers
  // and keys of existing frames.
  void RegisterExistingFrames();

  // Returns the web frame with |frame_id|, if one exisits, from the list of
  // associated web frames.
  WebFrame* GetFrameWithId(const std::string& frame_id);

  // WebFramesManager overrides
  const std::vector<WebFrame*>& GetAllWebFrames() override;
  WebFrame* GetMainWebFrame() override;

 private:
  friend class web::WebStateUserData<WebFramesManagerImpl>;

  // List of all web frames associated with WebState.
  std::vector<std::unique_ptr<WebFrame>> web_frames_;
  // List of pointers to all web frames associated with WebState.
  std::vector<WebFrame*> web_frame_ptrs_;

  // Reference to the current main web frame.
  WebFrame* main_web_frame_ = nullptr;
  WebState* web_state_ = nullptr;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_FRAMES_MANAGER_H_
