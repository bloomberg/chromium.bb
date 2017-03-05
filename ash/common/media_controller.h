// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_MEDIA_CONTROLLER_H_
#define ASH_COMMON_MEDIA_CONTROLLER_H_

#include "ash/public/interfaces/media.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ash {

// Forwards notifications from the MediaController to observers.
class MediaCaptureObserver {
 public:
  // Called when media capture state has changed.
  virtual void OnMediaCaptureChanged(
      const std::vector<mojom::MediaCaptureState>& capture_states) = 0;

 protected:
  virtual ~MediaCaptureObserver() {}
};

// Provides the MediaController interface to the outside world. This lets a
// consumer of ash provide a MediaClient, which we will dispatch to if one has
// been provided to us.
class MediaController : public mojom::MediaController,
                        public mojom::MediaClient {
 public:
  MediaController();
  ~MediaController() override;

  void BindRequest(mojom::MediaControllerRequest request);

  void AddObserver(MediaCaptureObserver* observer);
  void RemoveObserver(MediaCaptureObserver* observer);

  // mojom::MediaClient:
  void HandleMediaNextTrack() override;
  void HandleMediaPlayPause() override;
  void HandleMediaPrevTrack() override;
  void RequestCaptureState() override;

 private:
  friend class MultiProfileMediaTrayItemTest;

  // mojom::MediaController:
  void SetClient(mojom::MediaClientAssociatedPtrInfo client) override;
  void NotifyCaptureState(
      const std::vector<mojom::MediaCaptureState>& capture_states) override;

  mojo::BindingSet<mojom::MediaController> bindings_;

  mojom::MediaClientAssociatedPtr client_;

  base::ObserverList<MediaCaptureObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaController);
};

}  // namespace ash

#endif  // ASH_COMMON_MEDIA_CONTROLLER_H_
