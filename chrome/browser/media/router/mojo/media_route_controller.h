// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTE_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTE_CONTROLLER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojo/media_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media_router {

class MediaRouter;

// A controller for a MediaRoute. Forwards commands for controlling the route to
// an out-of-process controller. Notifies its observers whenever there is a
// change in the route's MediaStatus.
//
// It is owned by its observers, each of which holds a scoped_refptr to it. All
// the classes that hold a scoped_refptr must inherit from the Observer class.
// An observer should be instantiated with a scoped_refptr obtained through
// MediaRouter::GetRouteController().
//
// A MediaRouteController instance is destroyed when all its observers dispose
// their references to it. When the Mojo connection with the out-of-process
// controller is terminated or has an error, Invalidate() will be called by the
// MediaRouter or OnMojoConnectionError() to make observers dispose their
// refptrs.
class MediaRouteController : public mojom::MediaStatusObserver,
                             public base::RefCounted<MediaRouteController> {
 public:
  // Observes MediaRouteController for MediaStatus updates. The ownership of a
  // MediaRouteController is shared by its observers.
  class Observer {
   public:
    // Adds itself as an observer to |controller|.
    explicit Observer(scoped_refptr<MediaRouteController> controller);

    // Removes itself as an observer if |controller_| is still valid.
    virtual ~Observer();

    virtual void OnMediaStatusUpdated(const MediaStatus& status) = 0;

    // Returns a reference to the observed MediaRouteController. The reference
    // should not be stored by any object that does not subclass ::Observer.
    scoped_refptr<MediaRouteController> controller() const {
      return controller_;
    }

   private:
    friend class MediaRouteController;

    // Disposes the reference to the controller.
    void InvalidateController();

    // Called by InvalidateController() after the reference to the controller is
    // disposed. Overridden by subclasses to do custom cleanup.
    virtual void OnControllerInvalidated();

    scoped_refptr<MediaRouteController> controller_;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Constructs a MediaRouteController that forwards media commands to
  // |mojo_media_controller|. |media_router| will be notified when the
  // MediaRouteController is destroyed via DetachRouteController().
  MediaRouteController(const MediaRoute::Id& route_id,
                       mojom::MediaControllerPtr mojo_media_controller,
                       MediaRouter* media_router);

  // Media controller methods for forwarding commands to a
  // mojom::MediaControllerPtr held in |mojo_media_controller_|.
  void Play();
  void Pause();
  void Seek(base::TimeDelta time);
  void SetMute(bool mute);
  void SetVolume(float volume);

  // mojom::MediaStatusObserver:
  // Notifies |observers_| of a status update.
  void OnMediaStatusUpdated(const MediaStatus& status) override;

  // Notifies |observers_| to dispose their references to the controller. The
  // controller gets destroyed when all the references are disposed.
  void Invalidate();

  // Returns a mojo pointer bound to |this| by |binding_|. This must only be
  // called at most once in the lifetime of the controller.
  mojom::MediaStatusObserverPtr BindObserverPtr();

  MediaRoute::Id route_id() const { return route_id_; }

 private:
  friend class base::RefCounted<MediaRouteController>;

  ~MediaRouteController() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called when the connection between |this| and the MediaControllerPtr or
  // the MediaStatusObserver binding is no longer valid. Notifies
  // |media_router_| and |observers_| to dispose their references to |this|.
  void OnMojoConnectionError();

  // The ID of the Media Route that |this| controls.
  const MediaRoute::Id route_id_;

  // Handle to the mojom::MediaController that receives media commands.
  mojom::MediaControllerPtr mojo_media_controller_;

  // |media_router_| will be notified when the controller is destroyed.
  MediaRouter* const media_router_;

  // The binding to observe the out-of-process provider of status updates.
  mojo::Binding<mojom::MediaStatusObserver> binding_;

  // Observers that are notified of status updates. The observers share the
  // ownership of the controller through scoped_refptr.
  base::ObserverList<Observer> observers_;

  // This becomes false when the controller is invalidated.
  bool is_valid_ = true;

  DISALLOW_COPY_AND_ASSIGN(MediaRouteController);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTE_CONTROLLER_H_
