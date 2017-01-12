// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_RECEIVER_PRESENTATION_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_RECEIVER_PRESENTATION_SERVICE_DELEGATE_IMPL_H_

#include <string>

#include "chrome/browser/media/router/presentation_service_delegate_observers.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

class OffscreenPresentationManager;

// Implements the receiver side of Presentation API for offscreen presentation.
// Created with offscreen WebContents for an offscreen presentation. Each
// instance is tied to a single offscreen presentation whose ID is given during
// construction. As such, the receiver APIs are contextual with the offscreen
// presentation. Only the main frame of the offscreen WebContents is allowed to
// make receiver Presentation API requests; requests made from any other frame
// will be rejected.
class ReceiverPresentationServiceDelegateImpl
    : public content::WebContentsUserData<
          ReceiverPresentationServiceDelegateImpl>,
      public content::ReceiverPresentationServiceDelegate {
 public:
  // Creates an instance of ReceiverPresentationServiceDelegateImpl under
  // |web_contents| and registers it as the receiver of the offscreen
  // presentation |presentation_id| with OffscreenPresentationManager.
  // No-op if a ReceiverPresentationServiceDelegateImpl instance already
  // exists under |web_contents|. This class does not take ownership of
  // |web_contents|.
  static void CreateForWebContents(content::WebContents* web_contents,
                                   const std::string& presentation_id);

  // content::ReceiverPresentationServiceDelegate implementation.
  void AddObserver(
      int render_process_id,
      int render_frame_id,
      content::PresentationServiceDelegate::Observer* observer) override;
  void RemoveObserver(int render_process_id, int render_frame_id) override;
  void Reset(int render_process_id, int render_frame_id) override;
  void RegisterReceiverConnectionAvailableCallback(
      const content::ReceiverConnectionAvailableCallback&
          receiver_available_callback) override;

 private:
  friend class content::WebContentsUserData<
      ReceiverPresentationServiceDelegateImpl>;

  ReceiverPresentationServiceDelegateImpl(content::WebContents* web_contents,
                                          const std::string& presentation_id);

  // Reference to the WebContents that owns this instance.
  content::WebContents* const web_contents_;

  const std::string presentation_id_;

  // This is an unowned pointer to the OffscreenPresentationManager.
  OffscreenPresentationManager* const offscreen_presentation_manager_;

  PresentationServiceDelegateObservers observers_;

  DISALLOW_COPY_AND_ASSIGN(ReceiverPresentationServiceDelegateImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_RECEIVER_PRESENTATION_SERVICE_DELEGATE_IMPL_H_
