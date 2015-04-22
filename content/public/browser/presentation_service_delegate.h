// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/presentation_session.h"

namespace content {

class PresentationScreenAvailabilityListener;

// An interface implemented by embedders to handle presentation API calls
// forwarded from PresentationServiceImpl.
class CONTENT_EXPORT PresentationServiceDelegate {
 public:
  // Observer interface to listen for changes to PresentationServiceDelegate.
  class CONTENT_EXPORT Observer {
   public:
    // Called when the PresentationServiceDelegate is being destroyed.
    virtual void OnDelegateDestroyed() = 0;

   protected:
    virtual ~Observer() {}
  };

  using PresentationSessionSuccessCallback =
      base::Callback<void(const PresentationSessionInfo&)>;
  using PresentationSessionErrorCallback =
      base::Callback<void(const PresentationError&)>;

  virtual ~PresentationServiceDelegate() {}

  // Registers an observer with this class to listen for updates to this class.
  // This class does not own the observer.
  // It is an error to add an observer if it has already been added before.
  virtual void AddObserver(Observer* observer) = 0;
  // Unregisters an observer with this class.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Registers |listener| to continuously listen for
  // availability updates for a presentation URL, originated from the frame
  // given by |render_process_id| and |render_frame_id|.
  // This class does not own |listener|.
  // Returns true on success.
  // This call will return false if a listener with the same presentation URL
  // from the same frame is already registered.
  virtual bool AddScreenAvailabilityListener(
      int render_process_id,
      int render_frame_id,
      PresentationScreenAvailabilityListener* listener) = 0;

  // Unregisters |listener| originated from the frame given by
  // |render_process_id| and |render_frame_id| from this class. The listener
  // will no longer receive availability updates.
  virtual void RemoveScreenAvailabilityListener(
      int render_process_id,
      int render_frame_id,
      PresentationScreenAvailabilityListener* listener) = 0;

  // Resets the presentation state for the frame given by |render_process_id|
  // and |render_frame_id|.
  // This unregisters all listeners associated with the given frame, and clears
  // the default presentation URL and ID set for the frame.
  virtual void Reset(
      int render_process_id,
      int render_frame_id) = 0;

  // Sets the default presentation URL and ID for frame given by
  // |render_process_id| and |render_frame_id|.
  // If |default_presentation_url| is empty, the default presentation URL will
  // be cleared.
  virtual void SetDefaultPresentationUrl(
      int render_process_id,
      int render_frame_id,
      const std::string& default_presentation_url,
      const std::string& default_presentation_id) = 0;

  // Starts a new presentation session.
  // Typically, the embedder will allow the user to select a screen to show
  // |presentation_url|.
  // |render_process_id|, |render_frame_id|: ID of originating frame.
  // |presentation_url|: URL of the presentation.
  // |presentation_id|: The caller may provide an non-empty string to be used
  // as the ID of the presentation. If empty, the default presentation ID
  // will be used. If both are empty, the embedder will automatically generate
  // one.
  // |success_cb|: Invoked with session info, if presentation session started
  // successfully.
  // |error_cb|: Invoked with error reason, if presentation session did not
  // start.
  virtual void StartSession(
      int render_process_id,
      int render_frame_id,
      const std::string& presentation_url,
      const std::string& presentation_id,
      const PresentationSessionSuccessCallback& success_cb,
      const PresentationSessionErrorCallback& error_cb) = 0;

  // Joins an existing presentation session. Unlike StartSession(), this
  // does not bring a screen list UI.
  // |render_process_id|, |render_frame_id|: ID for originating frame.
  // |presentation_url|: URL of the presentation.
  // |presentation_id|: The ID of the presentation to join.
  // |success_cb|: Invoked with session info, if presentation session joined
  // successfully.
  // |error_cb|: Invoked with error reason, if joining failed.
  virtual void JoinSession(
      int render_process_id,
      int render_frame_id,
      const std::string& presentation_url,
      const std::string& presentation_id,
      const PresentationSessionSuccessCallback& success_cb,
      const PresentationSessionErrorCallback& error_cb) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_
