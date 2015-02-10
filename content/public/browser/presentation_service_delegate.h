// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_

#include "content/common/content_export.h"

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

  // Unregisters all listeners associated with the frame given by
  // |render_process_id| and |render_frame_id|.
  virtual void RemoveAllScreenAvailabilityListeners(
      int render_process_id,
      int render_frame_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_
